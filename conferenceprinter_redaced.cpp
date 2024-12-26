#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <cups/cups.h>
#include "unistd.h"
#include "slacking.hpp"

using namespace std;

//simple helper function to clean up string names
void removeSubstrs(string& s, string p) { 
    string::size_type n = p.length();
    for (string::size_type i = s.find(p);
        i != string::npos;
        i = s.find(p))
        s.erase(i, n);
}

void downloadFile(string link, string filename, bool followredir) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return;
    }

    std::cout << "DL check1\n";
    
    FILE* output_file = fopen(filename.c_str(), "wb");
    if (!output_file) {
        std::cerr << "Failed to open output file " << filename << std::endl;
        curl_easy_cleanup(curl);
        return;
    }

    std::cout << "DL check2\n";
    

    curl_easy_setopt(curl, CURLOPT_URL, link.c_str());
    if(followredir) curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, output_file);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_COOKIELIST, "Set-Cookie: AUTHORIZATION=Bearer AIzaSyC8t5-dbdzCeF2eA0nV6KF8uMqos8fGfjc");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, false);

    
    std::cout << "DL check3\n";

    CURLcode res = curl_easy_perform(curl);
    std::cout << "DL check3.1\n";
    if (res != CURLE_OK) {
        std::cerr << "Failed to download file: " << curl_easy_strerror(res) << std::endl;
        fclose(output_file);
        curl_easy_cleanup(curl);
        return;
    }

    
    std::cout << "DL check4\n";
    
    fclose(output_file);
    curl_easy_cleanup(curl);
    std::cout << "Downloaded file to " << filename << std::endl;
}

vector<vector<string>> readResponses() {
    vector<vector<string>> responses;
    vector<string> row;
    string line, word;

    ifstream file("responses.csv");
    if(file.is_open()) {
        //loop through every row in the csv
        while(getline(file, line)) {
            row.clear();

            stringstream str(line);

            //loop through every word in each row, putting into the row vector
            while(getline(str, word, ',')) {
                row.push_back(word);
            }

            //put the row vector into the responses 2D vector
            responses.push_back(row);
        }
    }
    return responses;
}

void sendPrintRequestMessage(const vector<string> &fileinfo, vector<pair<string, vector<string>>> &timestamps) {
    string requesttimestamp = fileinfo[0];
    string gDriveURL = fileinfo[1];
    string docType = fileinfo[2];
    string numCopies = fileinfo[3];
    string building = fileinfo[4];
    string roomNumber = fileinfo[5];
    string extraInfo = fileinfo[6];
    if(extraInfo == "") extraInfo = "They didn't leave any special instructions. ";
    else extraInfo = "They left some special instructions: " + extraInfo + "         ";
    string committee = fileinfo[8];
    removeSubstrs(committee, "\\r");
    if(committee == "") committee = "Anonymous Committee";

    

    

    //send slack confirmation message
    std::cout << "Sending confirmation message in #printing..." << endl;
    auto& slack = slack::create("APIKEY");
    string message0 = committee + " has requested to print " + numCopies + " of a " + docType + " file at " + requesttimestamp + ". They are located in room " + roomNumber + " of " + building + ".\n" + extraInfo + "\n";
    string message1 = "File Google Drive link: " + gDriveURL + "\n";
    string message2 = "Please react a thumbs up like mine to confirm the print request. Do not send any other reactions.";


    // deprecated
    /*auto messagedatajson = slack::post (
        "chat.postMessage",
        {
            {"text", message0 + message1 + message2},
            {"channel", "C0586GKEKS5"},
            {"username", "Print Notification Bot"},
            {"iconemoji", ":printer:"}
        }
    );*/

    auto messagedatajson = slack.chat.postMessage(message0 + message1 + message2, "C0586GKEKS5");

    //add the first thumbs up to the message
    string timestamp = messagedatajson["ts"];
    slack::post (
        "reactions.add",
        {
            {"channel", "C0586GKEKS5"},
            {"name", "thumbsup"},
            {"timestamp", timestamp}
        }
    );

    //put that message's timestamp and the asoociated gDrive URL into the timestamps vector
    timestamps.push_back({timestamp, fileinfo});


    // Below commented function has been outdated.

    /*
    for(int i = 0; i < 30; i++) {

        std::cout << endl;
        std::cout << "searching reactions of confirmation message for the thumbs up..." << endl;

        auto reactions = slack::post (
            "reactions.get",
            {
                {"channel", "C0586GKEKS5"},
                {"timestamp", timestamp}
            }
        );

        
        int numthumbsup = reactions["message"]["reactions"][0]["count"];
        if(numthumbsup > 1) {
            slack::post (
                "chat.postMessage",
                {
                    {"text", "You reacted thumbs up to the message! Now printing the file."},
                    {"channel", "C0586GKEKS5"},
                    {"username", "Test-username"},
                    {"icon_emoji", ":printer:"}
                }
            );
            std::cout << "using default printer: " << cupsGetDefault() << "..." << endl;
            std::cout << "printing \"" << "doctoprint.pdf" << "\"..." << endl;
            cupsPrintFile(cupsGetDefault(), "doctoprint.pdf", "Main Program Print Job", 0, NULL);
            break;
        }
        sleep(2);
    }*/

    
}

void printFile(vector<string> gSheetRow) {
    std::ios::sync_with_stdio(true);
    slack::post (
        "chat.postMessage",
        {
            {"text", "You reacted thumbs up to the message! Now attempting to print file at: " + gSheetRow[1]},
            {"channel", "C0586GKEKS5"},
            {"username", "Print Notification Bot"},
            {"icon_emoji", ":printer:"}
        }
    );

    string documentURL = "https://drive.google.com/u/1/uc?id=" + gSheetRow[1].substr(33, 33) + "&export=download";
    std::cout << "documentURL: " << documentURL << endl;
    downloadFile(documentURL, "doctoprint.pdf", true);


    std::cout << "Now attempting to configure printer options...\n";
    //set up options based on the gSheet row
    int numoptions = 0;
    //std::cout << "checkpoint1\n";
    cups_option_t *options = NULL;
    //std::cout << "checkpoint2\n";
    numoptions = cupsAddOption(CUPS_COPIES, gSheetRow[3].c_str(), numoptions, &options);
    numoptions = cupsAddOption(CUPS_PRINT_COLOR_MODE, CUPS_PRINT_COLOR_MODE_COLOR, numoptions, &options); //just set it to color every time or else it breaks the code
    //std::cout << "checkpoint3\n";
    //TODO: Complete the options section when we know better what the options will accept for our printers.



    std::cout << "using default printer: " << cupsGetDefault() << "..." << endl;
    std::cout << "printing file" << endl;

    cupsPrintFile(cupsGetDefault(), "doctoprint.pdf", "Main Program Print Job", numoptions, options);
}

int main(int argc, char* argv[]) {

    std::ios::sync_with_stdio(true);

    vector<pair<string, vector<string>>> pendingRequests;

    ifstream ifs("nextRow.txt");
    string nextRowString = "-1";
    if(ifs.is_open()) ifs >> nextRowString;
    else {
        std::cout << "problem opening \"nextRow.txt\". Stopping operation." << endl;
        return 1;
    }
    ifs.close();

    int nextRow = stoi(nextRowString);
    if(nextRow == -1) {
        std::cout << "problem reading \"nextRow.txt\". Stopping operation." << endl;
        return 1;
    }

    while(true) {
        std::cout << "Starting loop. Next google sheet row: " << nextRow+1 << endl;

        std::cout << "Downloading responses sheet..." << endl;
        string responsesURL = "REDACTED";
        downloadFile(responsesURL, "responses.csv", true);

        std::cout << "Processing responses..." << endl;
        vector<vector<string>> responses = readResponses();


        //if there are new rows that have been added after the last row read
        if(responses.size() > nextRow) {
            std::cout << "New row found!" << endl;
            //string documentURL = "https://drive.google.com/u/1/uc?id=" + responses[nextRow][1].substr(33, 33) + "&export=download";
            //std::cout << "documentURL: " << documentURL << endl;
            //downloadFile(documentURL, "doctoprint.pdf", true);


            //increment the next row and push it to the file for the next time the program is started
            nextRow++;
            ofstream ofs("nextRow.txt");
            if(ofs.is_open()) {
                ofs << nextRow;
                ofs.close();
            } else {
                std::cout << "problem writing to \"nextRow.txt\". To avoid double printing, stopping operation now." << endl;
                return 1;
            }

            sendPrintRequestMessage(responses[nextRow-1], pendingRequests);
        } else {
            std::cout << "No new rows since last loop." << endl;
        }

        std::cout << "Now checking old pending request slack messages" << endl << endl;
        
        // look through all of the currently pending request messages to see if anyone has given thumbs up to them.
        for(int i = 0; i < pendingRequests.size(); i++) {
            std::cout << "checking one message right now..." << endl;
            // read which reactions are on the message
            auto reactions = slack::post (
                "reactions.get",
                {
                    {"channel", "C0586GKEKS5"},
                    {"timestamp", pendingRequests[i].first}
                }
            );

            // if the first listed reaction has a count more than 1, send file to printer, then delete that entry from the pending requests vector
            // TODO: Make this more robust so that it can handle accidental reactions other than the thumbs up
            int numthumbsup = reactions["message"]["reactions"][0]["count"];
            if(numthumbsup > 1) {

                slack::post (
                    "reactions.add",
                    {
                        {"channel", "C0586GKEKS5"},
                        {"name", "printer"},
                        {"timestamp", pendingRequests[i].first}
                    }
                );

                printFile(pendingRequests[i].second);
                
                pendingRequests.erase(pendingRequests.begin() + i);
            }
        }

        std::cout << "now sleeping for 10" << endl;
        sleep(10);
    }
}


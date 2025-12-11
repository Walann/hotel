#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <limits>
#include <algorithm>
#include <cctype>
#include <list>          // List
#include <queue>         // Queue (for BFS)
#include <stack>         // Stack (undo)
#include <unordered_map> // Hash table
#include <set>           // For BFS visited set

using namespace std;

// Requirement 1: Use classes, inheritance, and encapsulation
class Hotel {
protected:
    // Requirement 2: Use a struct to group related room data
    struct RoomType {
        std::string description;
        int totalRooms;
        int availableRooms;
        double pricePerNight;
        std::string roomRange;
        std::vector<int> availableRoomNumbers;  // Requirement 3: Use STL vector
        std::map<int, std::string> guests;      // Requirement 4: Use STL map
        std::vector<int> allRoomNumbers;        // used to reset availability per day
    };

    // Explicit tree node (Requirement: Tree)
    struct TreeNode {
        int roomNumber;
        TreeNode* left;
        TreeNode* right;
    };

    // For undo stack (Requirement: Stack)
    struct Action {
        std::string guestName;
        std::string date;       // stay date (reservation date)
        int roomNumber;
        int nights;
        double pricePerNight;
        double totalCost;
    };

    // Detailed reservation record for saving
    struct Reservation {
        std::string guestName;
        int roomNumber;
        std::string roomType;
        std::string stayDate;   // reservation date
        int nights;
        int checkInHour;
        double pricePerNight;
        double totalCost;
    };

    std::string name;
    int totalRooms;
    double totalRevenue;

    // Track all reservations made/loaded in this session
    vector<string> people;      // guest names
    vector<int> roomsnums;      // room numbers

    // All reservations (can be for multiple dates)
    std::vector<Reservation> reservationsForDay;

    // reservations[date][roomNumber] = guestName
    std::map<std::string, std::map<int, std::string>> reservations;

    // Hash table for guest lookups
    std::unordered_map<std::string, std::vector<int>> guestToRooms;

    // List for guest history
    std::list<std::string> guestHistory;

    // Tree for occupied rooms
    TreeNode* occupiedRoomsRoot;

    // Graph (adjacency list of room connections)
    std::map<int, std::vector<int>> roomGraph;

    // Stack for undo operations
    std::stack<Action> bookingHistory;

    // Helper: split a string by a delimiter (used for file parsing)
    vector<string> split(const string& s, char delim) {
        vector<string> result;
        stringstream ss(s);
        string item;

        while (getline(ss, item, delim)) {
            result.push_back(item);
        }

        return result;
    }

    // ---- Tree helper functions ----
    TreeNode* insertRoomInTree(TreeNode* node, int roomNumber) {
        if (!node) {
            TreeNode* newNode = new TreeNode{ roomNumber, nullptr, nullptr };
            return newNode;
        }
        if (roomNumber < node->roomNumber) {
            node->left = insertRoomInTree(node->left, roomNumber);
        }
        else if (roomNumber > node->roomNumber) {
            node->right = insertRoomInTree(node->right, roomNumber);
        }
        // Ignore duplicates
        return node;
    }

    TreeNode* findMinNode(TreeNode* node) {
        while (node && node->left) {
            node = node->left;
        }
        return node;
    }

    TreeNode* removeRoomFromTree(TreeNode* node, int roomNumber) {
        if (!node) return nullptr;

        if (roomNumber < node->roomNumber) {
            node->left = removeRoomFromTree(node->left, roomNumber);
        }
        else if (roomNumber > node->roomNumber) {
            node->right = removeRoomFromTree(node->right, roomNumber);
        }
        else {
            // Found node to remove
            if (!node->left) {
                TreeNode* rightChild = node->right;
                delete node;
                return rightChild;
            }
            else if (!node->right) {
                TreeNode* leftChild = node->left;
                delete node;
                return leftChild;
            }
            else {
                // Two children: replace with inorder successor
                TreeNode* successor = findMinNode(node->right);
                node->roomNumber = successor->roomNumber;
                node->right = removeRoomFromTree(node->right, successor->roomNumber);
            }
        }
        return node;
    }

    void inorderPrint(TreeNode* node) {
        if (!node) return;
        inorderPrint(node->left);
        cout << node->roomNumber << " ";
        inorderPrint(node->right);
    }

    void clearTree(TreeNode* node) {
        if (!node) return;
        clearTree(node->left);
        clearTree(node->right);
        delete node;
    }

    // Requirement 6: Ability to reset hotel state for a "new day"
    void resetStateForNewDate() {
        totalRevenue = 0.0;
        people.clear();
        roomsnums.clear();
        reservationsForDay.clear();
        reservations.clear();
        guestHistory.clear();
        guestToRooms.clear();

        // Clear undo stack
        while (!bookingHistory.empty()) {
            bookingHistory.pop();
        }

        // Clear tree
        clearTree(occupiedRoomsRoot);
        occupiedRoomsRoot = nullptr;

        // Reset availability for each room type
        for (auto& pair : roomTypes) {
            RoomType& rt = pair.second;
            rt.availableRooms = rt.totalRooms;
            rt.guests.clear();
            rt.availableRoomNumbers = rt.allRoomNumbers;
        }
        // roomGraph is structural; we do NOT clear it here.
    }

    // Core booking logic (does NOT touch totalRevenue directly)
    bool bookRoom(const std::string& guestName,
                  const std::string& date,
                  int roomNumber) {
        for (auto& p : roomTypes) {
            RoomType& rt = p.second;
            auto itRoomNumber = std::find(rt.availableRoomNumbers.begin(),
                                          rt.availableRoomNumbers.end(),
                                          roomNumber);
            if (itRoomNumber != rt.availableRoomNumbers.end()) {
                rt.guests[roomNumber] = guestName;
                rt.availableRoomNumbers.erase(itRoomNumber);
                rt.availableRooms--;

                reservations[date][roomNumber] = guestName;
                people.push_back(guestName);
                roomsnums.push_back(roomNumber);

                // Update hash table
                guestToRooms[guestName].push_back(roomNumber);

                // Update guest history list
                guestHistory.push_back(guestName);

                // Insert into tree of occupied rooms
                occupiedRoomsRoot = insertRoomInTree(occupiedRoomsRoot, roomNumber);

                return true;
            }
        }
        return false;
    }

public:
    // Requirement 8: Maintain multiple room types in a map
    std::map<std::string, RoomType> roomTypes;

    Hotel(std::string hotelName, int totalRooms)
        : name(hotelName),
          totalRooms(totalRooms),
          totalRevenue(0.0),
          occupiedRoomsRoot(nullptr) {}

    virtual ~Hotel() {
        clearTree(occupiedRoomsRoot);
    }

    // Requirement 9: Show menu-driven interface
    void showOptions() {
        std::cout << "\nChoose an action:\n";
        std::cout << "1. Reserve a room\n";
        std::cout << "2. Display total revenue and guests\n";
        std::cout << "3. Display room availability\n";
        std::cout << "4. Save information to file\n";
        std::cout << "5. Show reservations for a specific date\n";
        std::cout << "6. New Day (switch date)\n";
        std::cout << "7. Exit\n";
        std::cout << "8. Find guest by name (hash table lookup)\n";
        std::cout << "9. Undo last booking (stack)\n";
        std::cout << "10. Show reachable rooms from a room (graph BFS)\n";
        std::cout << "11. Show guest history (list)\n";
    }

    // Requirement 10: Display available room types and counts
    void showAvailableRooms(const string& todayDate) {
        std::cout << "\nWelcome to " << name << "!" << std::endl;
        std::cout << "Today's date: " << todayDate << std::endl;
        std::cout << "Choose a room type to reserve:\n";
        int option = 1;
        for (const auto& rt : roomTypes) {
            std::cout << option++ << ". " << rt.first
                      << " - " << rt.second.availableRooms << " available - $"
                      << rt.second.pricePerNight << " a night - Rooms "
                      << rt.second.roomRange << "\n";
        }
    }

    // Requirement 11: Prompt user for reservation details (date, nights, time)
    void promptForReservationDetails(std::string& startDate,
                                     std::string& endDate,
                                     int& startTime,
                                     int& durationDays) {
        cout << "\n--- Reservation Details ---\n";

        cout << "Enter reservation start date (MM-DD-YYYY) "
             << "or '.' to use today's date (" << startDate << "): ";
        std::string input;
        cin >> input;

        if (input != ".") {
            startDate = input;
        }

        cout << "How many nights will you stay? ";
        cin >> durationDays;
        while (durationDays <= 0) {
            cout << "Nights must be at least 1. Enter again: ";
            cin >> durationDays;
        }

        cout << "Enter check-in time (0 – 23 hours): ";
        cin >> startTime;
        while (startTime < 0 || startTime > 23) {
            cout << "Invalid time. Enter check-in hour between 0–23: ";
            cin >> startTime;
        }

        // For simplicity, treat endDate as same as startDate (no date math)
        endDate = startDate;

        cout << "\nReservation date: " << startDate
             << "\nNights: " << durationDays
             << "\nCheck-in time: " << startTime << ":00\n\n";
    }

    // Requirement 12: Reserve a room of given type (option) for a guest
    void reserveRoom(int option,
                     const std::string& guestName,
                     const std::string& startDate,
                     const std::string& endDate,
                     int startTime,
                     int durationDays) {
        if (option < 1 || option > static_cast<int>(roomTypes.size())) {
            std::cout << "Invalid room type option.\n";
            return;
        }

        auto it = roomTypes.begin();
        std::advance(it, option - 1);
        RoomType& rt = it->second;

        if (rt.availableRoomNumbers.empty()) {
            std::cout << "No available rooms for selected type.\n";
            return;
        }

        int roomNumber = rt.availableRoomNumbers.front();
        if (bookRoom(guestName, startDate, roomNumber)) {
            double totalCost = rt.pricePerNight * durationDays;

            // Update revenue for this session
            totalRevenue += totalCost;

            // Push full info to undo stack
            bookingHistory.push({ guestName,
                                  startDate,
                                  roomNumber,
                                  durationDays,
                                  rt.pricePerNight,
                                  totalCost });

            // Record detailed reservation (for saving later)
            Reservation r;
            r.guestName     = guestName;
            r.roomNumber    = roomNumber;
            r.roomType      = it->first;
            r.stayDate      = startDate;       // reservation date
            r.nights        = durationDays;
            r.checkInHour   = startTime;
            r.pricePerNight = rt.pricePerNight;
            r.totalCost     = totalCost;
            reservationsForDay.push_back(r);

            cout << "\n--- Reservation Complete ---\n";
            cout << "Guest Name     : " << guestName << "\n";
            cout << "Room Type      : " << it->first << "\n";
            cout << "Room Number    : " << roomNumber << "\n";
            cout << "Check-in Time  : " << startTime << ":00\n";
            cout << "Nights         : " << durationDays << "\n";
            cout << "Price per Night: $" << rt.pricePerNight << "\n";
            cout << "Total Cost     : $" << totalCost << "\n";
            cout << "-----------------------------\n\n";
        }
        else {
            std::cout << "Failed to reserve room.\n";
        }
    }

    // Requirement 13: Show total revenue and list of guests for current date
    void getTotal() {
        std::cout << "\nHotel: " << name << std::endl;
        std::cout << "Total Revenue (for current loaded date): $"
                  << totalRevenue << std::endl;

        if (!people.empty()) {
            std::cout << "Current reservations:\n";
            for (size_t i = 0; i < people.size(); ++i) {
                std::cout << "  Guest Name: " << people[i]
                          << " | Room Number: " << roomsnums[i] << std::endl;
            }
        }
        else {
            std::cout << "No reservations made yet for this date.\n";
        }
    }

    // Requirement 14: Display room availability by type
    void displayRoomAvailability() {
        std::cout << "\nRoom Availability:\n";
        for (const auto& rt : roomTypes) {
            std::cout << "  " << rt.first << " - "
                      << rt.second.availableRooms << " available\n";
        }
    }

    // Requirement 15: Save reservations for a **specific reservation date** to a text file
    //   File name: <date>.txt
    //   Line 1: TOTAL_REVENUE=<value for that date>
    //   Line 2: Header
    //   Next lines: reservations whose stayDate == date
    void saveToFile(const string& date) {
        // Collect reservations for this stay date
        std::vector<Reservation> toSave;
        double dateRevenue = 0.0;

        for (const Reservation& r : reservationsForDay) {
            if (r.stayDate == date) {
                toSave.push_back(r);
                dateRevenue += r.totalCost;
            }
        }

        if (toSave.empty()) {
            std::cout << "No reservations to save for " << date << ".\n";
            return;
        }

        std::ofstream outFile(date + ".txt");
        if (!outFile.is_open()) {
            std::cout << "Unable to open file for saving." << std::endl;
            return;
        }

        // First line: total revenue with label
        outFile << "TOTAL_REVENUE=" << dateRevenue << "\n";

        // Header line for readability
        outFile << "GuestName,RoomNumber,RoomType,StayDate,"
                   "Nights,CheckInHour,PricePerNight,TotalCost\n";

        // Then each reservation with full details
        for (const Reservation& r : toSave) {
            outFile << r.guestName << ","
                    << r.roomNumber << ","
                    << r.roomType << ","
                    << r.stayDate << ","
                    << r.nights << ","
                    << r.checkInHour << ","
                    << r.pricePerNight << ","
                    << r.totalCost << "\n";
        }

        outFile.close();
        std::cout << "Data saved to file: " << date << ".txt" << std::endl;
    }

    // Requirement 16: Load reservations and revenue from file for a given date
    void loadFromFile(const string& date) {
        // Reset state and represent only this date
        resetStateForNewDate();

        std::ifstream inFile(date + ".txt");
        if (!inFile.is_open()) {
            std::cout << "No existing reservations file found for " << date
                      << ". Starting fresh.\n";
            return;
        }

        std::string line;
        if (!std::getline(inFile, line)) {
            std::cout << "File for " << date << " is empty.\n";
            return;
        }

        // ----- Parse total revenue from first line -----
        try {
            if (line.rfind("TOTAL_REVENUE=", 0) == 0) {
                std::string value = line.substr(std::string("TOTAL_REVENUE=").size());
                totalRevenue = std::stod(value);
            }
            else if (!line.empty() && line[0] == '$') {
                totalRevenue = std::stod(line.substr(1));
            }
            else {
                totalRevenue = std::stod(line);
            }
        }
        catch (...) {
            totalRevenue = 0.0;
        }

        // ----- Optional header line (new format) -----
        std::streampos posAfterFirst = inFile.tellg();
        if (std::getline(inFile, line)) {
            vector<string> header = split(line, ',');
            if (header.size() == 8 && header[0] == "GuestName") {
                // header -> skip
            } else {
                // not header, rewind
                inFile.clear();
                inFile.seekg(posAfterFirst);
            }
        }

        // ----- Restore reservations from remaining lines -----
        while (std::getline(inFile, line)) {
            if (line.empty()) continue;
            vector<string> parsed = split(line, ',');

            // New full format
            if (parsed.size() >= 8) {
                Reservation r;
                try {
                    r.guestName     = parsed[0];
                    r.roomNumber    = std::stoi(parsed[1]);
                    r.roomType      = parsed[2];
                    r.stayDate      = parsed[3];
                    r.nights        = std::stoi(parsed[4]);
                    r.checkInHour   = std::stoi(parsed[5]);
                    r.pricePerNight = std::stod(parsed[6]);
                    r.totalCost     = std::stod(parsed[7]);
                }
                catch (...) {
                    continue; // skip bad line
                }

                // Add to in-memory list
                reservationsForDay.push_back(r);

                // Use stayDate as the key in reservations map
                if (!bookRoom(r.guestName, r.stayDate, r.roomNumber)) {
                    std::cout << "Warning: Could not restore room " << r.roomNumber
                              << " for guest " << r.guestName << ".\n";
                } else {
                    // Restore undo action as well
                    bookingHistory.push({ r.guestName,
                                          r.stayDate,
                                          r.roomNumber,
                                          r.nights,
                                          r.pricePerNight,
                                          r.totalCost });
                }
            }
            // Old simple format: guestName,roomNumber
            else if (parsed.size() >= 2) {
                std::string guestName = parsed[0];
                int roomNumber = 0;
                try {
                    roomNumber = std::stoi(parsed[1]);
                }
                catch (...) {
                    continue;
                }

                Reservation r;
                r.guestName     = guestName;
                r.roomNumber    = roomNumber;
                r.roomType      = "";
                r.stayDate      = date;
                r.nights        = 1;
                r.checkInHour   = 15;
                r.pricePerNight = 0.0;
                r.totalCost     = 0.0;
                reservationsForDay.push_back(r);

                if (!bookRoom(guestName, r.stayDate, roomNumber)) {
                    std::cout << "Warning: Could not restore room " << roomNumber
                              << " for guest " << guestName << ".\n";
                } else {
                    bookingHistory.push({ guestName,
                                          r.stayDate,
                                          roomNumber,
                                          r.nights,
                                          r.pricePerNight,
                                          r.totalCost });
                }
            }
        }

        inFile.close();
        std::cout << "Reservations loaded from file for " << date << ".\n";
        std::cout << "Total revenue from file: $" << totalRevenue << std::endl;
    }

    // Requirement 17: Show reservations for a specific date
    void showReservationsForDate(const std::string& date) {
        auto it = reservations.find(date);
        if (it != reservations.end() && !it->second.empty()) {
            std::cout << "Reservations for " << date << ":\n";
            for (const auto& reservation : it->second) {
                std::cout << "  Room " << reservation.first
                          << ": " << reservation.second << std::endl;
            }
        }
        else {
            std::cout << "No reservations found for " << date << ".\n";
        }
    }

    // Hash table lookup
    void findGuestReservations(const std::string& guestName) {
        auto it = guestToRooms.find(guestName);
        if (it == guestToRooms.end()) {
            std::cout << "No reservations found for " << guestName << ".\n";
            return;
        }
        std::cout << "Rooms reserved for " << guestName << ": ";
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << it->second[i];
        }
        std::cout << "\n";
    }

    // Undo last booking (stack)
    void undoLastBooking() {
        if (bookingHistory.empty()) {
            std::cout << "No bookings to undo.\n";
            return;
        }

        Action last = bookingHistory.top();
        bookingHistory.pop();

        // Find room type that contains this room number
        RoomType* foundType = nullptr;
        for (auto& p : roomTypes) {
            RoomType& rt = p.second;
            if (std::find(rt.allRoomNumbers.begin(),
                          rt.allRoomNumbers.end(),
                          last.roomNumber) != rt.allRoomNumbers.end()) {
                foundType = &rt;
                break;
            }
        }

        if (!foundType) {
            std::cout << "Error: Could not find room type for room "
                      << last.roomNumber << ". Undo failed.\n";
            return;
        }

        // Adjust revenue by full cost of this booking
        totalRevenue -= last.totalCost;
        if (totalRevenue < 0) totalRevenue = 0;

        // Remove guest from room's guest map
        foundType->guests.erase(last.roomNumber);

        // Return room to availability (avoid duplicates)
        if (std::find(foundType->availableRoomNumbers.begin(),
                      foundType->availableRoomNumbers.end(),
                      last.roomNumber) == foundType->availableRoomNumbers.end()) {
            foundType->availableRoomNumbers.push_back(last.roomNumber);
            foundType->availableRooms++;
        }

        // Remove from reservations map
        auto dateIt = reservations.find(last.date);
        if (dateIt != reservations.end()) {
            dateIt->second.erase(last.roomNumber);
            if (dateIt->second.empty()) {
                reservations.erase(dateIt);
            }
        }

        // Remove from people/roomsnums (last matching)
        for (int i = static_cast<int>(people.size()) - 1; i >= 0; --i) {
            if (people[i] == last.guestName && roomsnums[i] == last.roomNumber) {
                people.erase(people.begin() + i);
                roomsnums.erase(roomsnums.begin() + i);
                break;
            }
        }

        // Remove from reservationsForDay (last matching one)
        for (int i = static_cast<int>(reservationsForDay.size()) - 1; i >= 0; --i) {
            if (reservationsForDay[i].guestName == last.guestName &&
                reservationsForDay[i].roomNumber == last.roomNumber &&
                reservationsForDay[i].stayDate == last.date) {
                reservationsForDay.erase(reservationsForDay.begin() + i);
                break;
            }
        }

        // Remove from guest history (remove one occurrence from back)
        for (auto it = guestHistory.end(); it != guestHistory.begin();) {
            --it;
            if (*it == last.guestName) {
                guestHistory.erase(it);
                break;
            }
        }

        // Remove from hash table guestToRooms
        auto git = guestToRooms.find(last.guestName);
        if (git != guestToRooms.end()) {
            auto& vec = git->second;
            vec.erase(std::remove(vec.begin(), vec.end(), last.roomNumber), vec.end());
            if (vec.empty()) {
                guestToRooms.erase(git);
            }
        }

        // Remove from tree
        occupiedRoomsRoot = removeRoomFromTree(occupiedRoomsRoot, last.roomNumber);

        std::cout << "Booking for " << last.guestName
                  << " in room " << last.roomNumber
                  << " on " << last.date << " has been undone.\n";
    }

    // Show occupied rooms via tree traversal
    void displayOccupiedRoomsInOrder() {
        if (!occupiedRoomsRoot) {
            std::cout << "No occupied rooms yet.\n";
            return;
        }
        std::cout << "Occupied rooms (in-order from tree): ";
        inorderPrint(occupiedRoomsRoot);
        std::cout << "\n";
    }

    // Graph traversal (BFS using roomGraph and std::queue)
    void bfsFromRoom(int startRoom) {
        if (roomGraph.find(startRoom) == roomGraph.end()) {
            std::cout << "Room " << startRoom << " not found in hotel graph.\n";
            return;
        }

        std::set<int> visited;
        std::queue<int> q;  // Requirement: Queue

        q.push(startRoom);
        visited.insert(startRoom);

        std::cout << "BFS starting from room " << startRoom << ": ";
        bool first = true;

        while (!q.empty()) {
            int r = q.front();
            q.pop();

            if (!first) std::cout << " -> ";
            std::cout << r;
            first = false;

            for (int neighbor : roomGraph[r]) {
                if (visited.find(neighbor) == visited.end()) {
                    visited.insert(neighbor);
                    q.push(neighbor);
                }
            }
        }
        std::cout << "\n";
    }

    // List guest history
    void showGuestHistory() {
        if (guestHistory.empty()) {
            std::cout << "No guest history yet.\n";
            return;
        }
        std::cout << "Guest reservation history (in order):\n";
        for (const auto& name : guestHistory) {
            std::cout << "  " << name << "\n";
        }
    }

    // Allow derived classes to build the graph
    void addGraphEdge(int roomA, int roomB) {
        roomGraph[roomA].push_back(roomB);
        roomGraph[roomB].push_back(roomA);
    }

    // Optional helper: get the real system date (not strictly required)
    std::string getCurrentDate() {
        tm now;
        std::time_t t2 = std::time(nullptr);
#ifdef _WIN32
        localtime_s(&now, &t2);
#else
        now = *std::localtime(&t2);
#endif
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%m-%d-%Y", &now);
        return buffer;
    }
};

// Requirement 18: Derived class that initializes room types and pricing
class HiltonHotel : public Hotel {
public:
    HiltonHotel(int totalRooms) : Hotel("Hilton", totalRooms) {
        // Standard Rooms, Courtyard: 101-170
        roomTypes["Standard Rooms, Courtyard"] =
        { "Standard Rooms, Courtyard", 70, 70, 125.0, "101 thru 170", {}, {}, {} };
        for (int i = 101; i <= 170; ++i) {
            roomTypes["Standard Rooms, Courtyard"].availableRoomNumbers.push_back(i);
            roomTypes["Standard Rooms, Courtyard"].allRoomNumbers.push_back(i);
        }

        // Standard Room, Scenic: 201-235
        roomTypes["Standard Room, Scenic"] =
        { "Standard Room, Scenic", 35, 35, 145.0, "201 thru 235", {}, {}, {} };
        for (int i = 201; i <= 235; ++i) {
            roomTypes["Standard Room, Scenic"].availableRoomNumbers.push_back(i);
            roomTypes["Standard Room, Scenic"].allRoomNumbers.push_back(i);
        }

        // Deluxe Suite: 236-250
        roomTypes["Deluxe Suite"] =
        { "Deluxe Suite", 15, 15, 350.0, "236 thru 250", {}, {}, {} };
        for (int i = 236; i <= 250; ++i) {
            roomTypes["Deluxe Suite"].availableRoomNumbers.push_back(i);
            roomTypes["Deluxe Suite"].allRoomNumbers.push_back(i);
        }

        // Penthouse: 301 and 302
        roomTypes["Penthouse"] =
        { "Penthouse", 2, 2, 1135.0, "301 and 302", {}, {}, {} };
        roomTypes["Penthouse"].availableRoomNumbers.push_back(301);
        roomTypes["Penthouse"].availableRoomNumbers.push_back(302);
        roomTypes["Penthouse"].allRoomNumbers.push_back(301);
        roomTypes["Penthouse"].allRoomNumbers.push_back(302);

        // Build graph connections between rooms (Requirement: Graph)
        auto connectRange = [this](int start, int end) {
            for (int r = start; r < end; ++r) {
                addGraphEdge(r, r + 1);
            }
        };

        connectRange(101, 170);
        connectRange(201, 235);
        connectRange(236, 250);
        connectRange(301, 302);
    }
};

int main() {
    // Requirement 19: Drive program with a user-controlled menu loop
    int totalRooms = 122;
    HiltonHotel hilton(totalRooms);

    char againChoice;
    int menuOption;
    std::string currentDate;

    std::cout << "Enter today's date (MM-DD-YYYY): ";
    std::cin >> currentDate;

    // Load existing data (if any) for today's date
    hilton.loadFromFile(currentDate);

    do {
        hilton.showAvailableRooms(currentDate);
        hilton.showOptions();

        std::cout << "\nEnter your number of choice (1-11): ";
        std::cin >> menuOption;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (menuOption) {
        case 1: {
            // Reserve a room
            std::string startDate = currentDate;
            std::string endDate;
            int startTime = 0;
            int durationDays = 1;

            hilton.promptForReservationDetails(startDate, endDate, startTime, durationDays);

            std::cout << "Enter room option (1-4): ";
            int roomOption;
            std::cin >> roomOption;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            if (roomOption >= 1 && roomOption <= 4) {
                std::cout << "Enter your full name: ";
                std::string guestName;
                std::getline(std::cin, guestName);

                hilton.reserveRoom(roomOption, guestName, startDate, endDate, startTime, durationDays);
            }
            else {
                std::cout << "Invalid room option.\n";
            }
            break;
        }
        case 2:
            // Show total revenue and guests
            hilton.getTotal();
            hilton.displayOccupiedRoomsInOrder();
            break;
        case 3:
            // Show room availability
            hilton.displayRoomAvailability();
            break;
        case 4: {
            // Save reservations for a specific reservation date
            std::string dateToSave;
            std::cout << "Enter reservation date to save (MM-DD-YYYY): ";
            std::cin >> dateToSave;
            hilton.saveToFile(dateToSave);
            std::cout << "Information has been saved (if any reservations existed for that date).\n";
            break;
        }
        case 5: {
            // Show reservations for a specific date (load that date and display)
            std::string date;
            std::cout << "Enter date to show reservations (MM-DD-YYYY): ";
            std::cin >> date;
            hilton.loadFromFile(date);
            hilton.showReservationsForDate(date);
            currentDate = date; // update current context date
            break;
        }
        case 6: {
            // New Day: switch active date
            std::cout << "Enter new date (MM-DD-YYYY): ";
            std::cin >> currentDate;
            hilton.loadFromFile(currentDate);
            break;
        }
        case 7:
            // Exit program
            std::cout << "Exiting program...\n";
            // Optional: auto-save for currentDate (will save only reservations whose stayDate == currentDate)
            hilton.saveToFile(currentDate);
            return 0;
        case 8: {
            // Hash table lookup by guest name
            std::cout << "Enter guest name to search: ";
            std::string guestName;
            std::getline(std::cin, guestName);
            hilton.findGuestReservations(guestName);
            break;
        }
        case 9:
            // Undo last booking (stack)
            hilton.undoLastBooking();
            break;
        case 10: {
            // Graph BFS from a given room
            std::cout << "Enter starting room number for BFS: ";
            int room;
            std::cin >> room;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            hilton.bfsFromRoom(room);
            break;
        }
        case 11:
            // Show guest history (list)
            hilton.showGuestHistory();
            break;
        default:
            std::cout << "Invalid option. Please select a valid action option.\n";
            break;
        }

        std::cout << "\nDo you want to perform another action? (y/n): ";
        std::cin >> againChoice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    } while (std::tolower(againChoice) == 'y');

    // Save before final exit (for currentDate only)
    hilton.saveToFile(currentDate);
    return 0;
}

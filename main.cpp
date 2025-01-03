//Taylan İrak
//30702


#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <mutex>
#include <vector>
using namespace std;

typedef unsigned int uint;

struct transaction {
    uint curr;          // Transaction ID 
    uint prev;             // Hash of the prev transaction
    uint ran;          // Random number
    thread::id x;       // Which thread mined (added) this transaction
    transaction* next;
    transaction* prevTx;

    transaction(uint c, uint p, uint r, thread::id tid)
            : curr(c), prev(p), ran(r), x(tid),
              next(nullptr), prevTx(nullptr) {}
};

struct transactionChain {
    transaction* head;  // Start of the chain
    transaction* tail;  // End of the chain

    transactionChain() : head(nullptr), tail(nullptr) {}

    // We are adding a transaction to the chain
    void add(transaction* newTran) {
        if (!head) {
            head = newTran;
            tail = newTran;
        } else {
            tail->next = newTran;
            newTran->prevTx = tail;
            tail = newTran;
        }
    }
};

// We are hashing a transaction
uint hashTransaction(transaction tran) {
    uint result = (tran.curr << 16) ^ (tran.prev << 8) ^ (tran.ran << 1);
    hash<uint> uint_hash;
    return uint_hash(result);
}

// We are validating
bool transactionValidator(transactionChain &tChain, uint threshold) {
    transaction* temp = tChain.head->next;
    while (temp != nullptr) {
        if (hashTransaction(*temp) > threshold) {
            return false;
        }
        temp = temp->next;
    }
    return true;
}

static atomic<int> sI(0);  // We are tracking which transaction index is being mined
static bool startF = false;     // We are signaling threads to start mining
static mutex mtx;                  // We are protecting shared resources with a mutex

// We are defining the mining function for each thread
void mine(transactionChain &tChain, const vector<uint> &entries, uint threshold, vector<uint> &btcThread, int threadID) {
    uint bitcoins = 0;
    srand((hash<thread::id>()(this_thread::get_id())) ^ 2 % 10000);

    // We are waiting for the signal to start mining
    while (!startF) {
        this_thread::yield();
    }

    // We are entering the mining loop
    while (true) {
        int index = sI.load(memory_order_relaxed);  // We are checking which entry to mine
        if (index >= static_cast<int>(entries.size())) {
            break;
        }

        bool foundSol = false;
        while (!foundSol) {
            // We are checking if another thread moved on to the next entry
            if (sI.load(memory_order_relaxed) != index) {
                break;
            }

            uint ran = static_cast<uint>(rand());
            uint prevHash = 0;

            // We are getting the hash of the last transaction in the chain
            {
                lock_guard<mutex> lk(mtx);
                if (tChain.tail) {
                    prevHash = hashTransaction(*tChain.tail);
                }
            }

            // We are creating and hashing a candidate transaction
            transaction candidate(entries[index], prevHash, ran, this_thread::get_id());
            uint hVal = hashTransaction(candidate);

            // We are checking if the hash meets the difficulty threshold
            if (hVal < threshold) {
                lock_guard<mutex> lk(mtx);
                if (sI.load(memory_order_relaxed) == index) {
                    // We are adding the transaction to the chain and moving to the next index
                    tChain.add(new transaction(candidate));
                    bitcoins++;
                    sI.store(index + 1, memory_order_relaxed);
                }
                foundSol = true;
            }
        }
    }

    // We are updating the vector with the bitcoins mined by this thread
    {
        lock_guard<mutex> lk(mtx);
        btcThread[threadID] = bitcoins;
        cout << "Thread " << this_thread::get_id() << " has " << bitcoins << " bitcoin(s)" << endl;
    }
}

int main() {
    int miners;
    uint difficulty;
    string filename;

    cout << "Enter difficulty level (1-10): ";
    cin >> difficulty;

    // We are calculating the threshold based on the difficulty level
    uint threshold = (1u << (31 - difficulty));
    cout << "Threshold: " << threshold << endl;

    cout << "Enter the filename of the input file: ";
    cin >> filename;

    cout << "Enter the number of miners: ";
    cin >> miners;

    cout << "----------START---------" << endl;


    ifstream inFile(filename);
    if (!inFile.is_open()) {
        cerr << "Error: Cannot open file " << filename << endl;
        return 1;
    }

    int numEntries;
    inFile >> numEntries;

    vector<uint> entries(numEntries);
    for (int i = 0; i < numEntries; i++) {
        inFile >> entries[i];
    }
    inFile.close();

    transactionChain tChain;
    transaction* genesis = new transaction(0, 0, 0, this_thread::get_id());
    tChain.add(genesis);

    // We are creating mining threads
    vector<uint> btcThread(miners, 0);


    vector<thread> threads;
    threads.reserve(miners);
    for (int i = 0; i < miners; ++i) {
        threads.emplace_back(mine, ref(tChain), cref(entries), threshold, ref(btcThread), i);
    }

    // We are signaling threads to start mining
    {
        lock_guard<mutex> lk(mtx);
        startF = true;
    }

    // We are waiting for all threads to complete
    for (auto &th : threads) {
        th.join();
    }

    // We are validating the transaction chain
    bool isValid = transactionValidator(tChain, threshold);
    if (isValid) {
        cout << "The transaction is valid" << endl;
    } else {
        cout << "The transaction is not valid" << endl;
    }

    return 0;
}

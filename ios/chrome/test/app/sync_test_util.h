// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_SYNC_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_SYNC_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "third_party/metrics_proto/user_demographics.pb.h"
#include "url/gurl.h"

namespace synced_sessions {
struct DistantSession;
}  // namespace synced_sessions

namespace chrome_test_util {

// Whether or not the fake sync server has already been setup by
// `SetUpFakeSyncServer()`.
bool IsFakeSyncServerSetUp();

// Sets up a fake sync server to be used by the SyncServiceImpl. Must only be
// called if `IsFakeSyncServerSetUp()` returns false.
void SetUpFakeSyncServer();

// Tears down the fake sync server used by the SyncServiceImpl and restores the
// real one. Must only be called if `IsFakeSyncServerSetUp()` is true.
void TearDownFakeSyncServer();

// Clears fake sync server data if the server is running, otherwise does
// nothing.
void ClearFakeSyncServerData();

// Ensures that all of the FakeServer's data is persisted to disk. This is
// useful before app restarts, where otherwise the FakeServer may not get to do
// its usual on-destruction flush.
void FlushFakeSyncServerToDisk();

// Triggers a sync cycle for a `type`.
void TriggerSyncCycle(syncer::DataType type);

// Gets the number of entities of the given `type`.
int GetNumberOfSyncEntities(syncer::DataType type);

// Verifies that `count` entities of the given `type` and `name` exist on the
// sync FakeServer. Folders are not included in this count.
BOOL VerifyNumberOfSyncEntitiesWithName(syncer::DataType type,
                                        std::string name,
                                        size_t count,
                                        NSError** error);

// Injects a bookmark into the fake sync server with `url` and `title`.
void AddBookmarkToFakeSyncServer(std::string url, std::string title);

// Injects a legacy bookmark into the fake sync server. The legacy bookmark
// means 2015 and earlier, prior to the adoption of GUIDs for originator client
// item ID.
void AddLegacyBookmarkToFakeSyncServer(std::string url,
                                       std::string title,
                                       std::string originator_client_item_id);

// Injects a distant session into the fake sync server. Tabs in this session
// will also be injected.
// TODO(crbug.com/40904349): don't take a DistantSession; rewrite using entity
// builder pattern.
void AddSessionToFakeSyncServer(const synced_sessions::DistantSession& session);

// Injects user demographics into the fake sync server.
void AddUserDemographicsToSyncServer(
    int birth_year,
    metrics::UserDemographicsProto::Gender gender);

// Injects an autofill profile into the fake sync server with `guid` and
// `full_name`.
void AddAutofillProfileToFakeSyncServer(std::string guid,
                                        std::string full_name);

// Deletes an autofill profile from the fake sync server with `guid`, if it
// exists. If it doesn't exist, nothing is done.
void DeleteAutofillProfileFromFakeSyncServer(std::string guid);

// Clears the autofill profile for the given `guid`.
void ClearAutofillProfile(std::string guid);

// See SyncService::IsEngineInitialized().
bool IsSyncEngineInitialized();

// Returns the current sync cache guid. The sync server must be running when
// calling this.
std::string GetSyncCacheGuid();

// Returns true if the DeviceInfo specifics on the fake server contains sync
// invalidation fields.
bool VerifySyncInvalidationFieldsPopulated();

// Returns true if there is an autofilll profile with the corresponding `guid`
// and `full_name`.
bool IsAutofillProfilePresent(std::string guid, std::string full_name);

// Verifies the sessions hierarchy on the Sync FakeServer. `expected_urls` is
// the collection of URLs that are to be expected for a single window. On
// failure, returns NO and `error` is set and includes a message. See the
// SessionsHierarchy class for documentation regarding the verification.
BOOL VerifySessionsOnSyncServer(const std::multiset<std::string>& expected_urls,
                                NSError** error);

// Verifies the URLs (in the HISTORY data type) on the Sync FakeServer.
// `expected_urls` is the collection of expected URLs. On failure, returns NO
// and `error` is set to an appropriate message.
BOOL VerifyHistoryOnSyncServer(const std::multiset<GURL>& expected_urls,
                               NSError** error);

// Adds typed URL to HistoryService at timestamp `visitTimestamp`.
void AddTypedURLToClient(const GURL& url,
                         base::Time visitTimestamp = base::Time::Now());

// Injects a HISTORY visit into the fake sync server.
void AddHistoryVisitToFakeSyncServer(const GURL& url);

// Injects a device info into the fake sync server.
void AddDeviceInfoToFakeSyncServer(const std::string& device_name,
                                   base::Time last_updated_timestamp);

// Returns YES if the provided `url` is present (or not) if `expected_present`
// is YES (or NO).
BOOL IsUrlPresentOnClient(const GURL& url,
                          BOOL expect_present,
                          NSError** error);

// Deletes typed URL from HistoryService.
void DeleteTypedUrlFromClient(const GURL& url);

// Adds a bookmark with a sync passphrase. The sync server will need the sync
// passphrase to start.
void AddBookmarkWithSyncPassphrase(const std::string& sync_passphrase);

// Adds a sync passphrase. The sync server will need the sync
// passphrase to start. In order to work, this need to be called before the
// primary user is signed-in.
void AddSyncPassphrase(const std::string& sync_passphrase);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_SYNC_TEST_UTIL_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class Location;
}

namespace leveldb {
class DB;
class Env;
class Status;
class WriteBatch;
}  // namespace leveldb

namespace content {

// Class to persist serviceworker registration data in a database.
// Should NOT be used on the IO thread since this does blocking
// file io. The ServiceWorkerStorage class owns this class and
// is responsible for only calling it serially on background
// non-IO threads (ala SequencedWorkerPool).
class CONTENT_EXPORT ServiceWorkerDatabase {
 public:
  // We do leveldb stuff in |path| or in memory if |path| is empty.
  explicit ServiceWorkerDatabase(const base::FilePath& path);
  ~ServiceWorkerDatabase();

  // Used in UMA. A new value must be appended only.
  enum Status {
    STATUS_OK,
    STATUS_ERROR_NOT_FOUND,
    STATUS_ERROR_IO_ERROR,
    STATUS_ERROR_CORRUPTED,
    STATUS_ERROR_FAILED,
    STATUS_ERROR_NOT_SUPPORTED,
    STATUS_ERROR_MAX,
  };
  static const char* StatusToString(Status status);

  struct CONTENT_EXPORT RegistrationData {
    // These values are immutable for the life of a registration.
    int64_t registration_id;
    GURL scope;

    // Versions are first stored once they successfully install and become
    // the waiting version. Then transition to the active version. The stored
    // version may be in the ACTIVATED state or in the INSTALLED state.
    GURL script;
    blink::mojom::ScriptType script_type;
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache;
    int64_t version_id;
    bool is_active;
    bool has_fetch_handler;
    base::Time last_update_check;
    base::Optional<blink::TrialTokenValidator::FeatureToTokensMap>
        origin_trial_tokens;
    blink::mojom::NavigationPreloadState navigation_preload_state;
    std::set<uint32_t> used_features;

    // Not populated until ServiceWorkerStorage::StoreRegistration is called.
    int64_t resources_total_size_bytes;

    RegistrationData();
    RegistrationData(const RegistrationData& other);
    ~RegistrationData();
  };

  struct ResourceRecord {
    int64_t resource_id;
    GURL url;
    // Signed so we can store -1 to specify an unknown or error state.  When
    // stored to the database, this value should always be >= 0.
    int64_t size_bytes;

    ResourceRecord() : resource_id(-1), size_bytes(0) {}
    ResourceRecord(int64_t id, GURL url, int64_t size_bytes)
        : resource_id(id), url(url), size_bytes(size_bytes) {}
  };

  // Reads next available ids from the database. Returns OK if they are
  // successfully read. Fills the arguments with an initial value and returns
  // OK if they are not found in the database. Otherwise, returns an error.
  Status GetNextAvailableIds(int64_t* next_avail_registration_id,
                             int64_t* next_avail_version_id,
                             int64_t* next_avail_resource_id);

  // Reads origins that have one or more than one registration from the
  // database. Returns OK if they are successfully read or not found.
  // Otherwise, returns an error.
  Status GetOriginsWithRegistrations(std::set<GURL>* origins);

  // Reads registrations for |origin| from the database. Returns OK if they are
  // successfully read or not found. Otherwise, returns an error.
  Status GetRegistrationsForOrigin(
      const GURL& origin,
      std::vector<RegistrationData>* registrations,
      std::vector<std::vector<ResourceRecord>>* opt_resources_list);

  // Reads all registrations from the database. Returns OK if successfully read
  // or not found. Otherwise, returns an error.
  Status GetAllRegistrations(std::vector<RegistrationData>* registrations);

  // Saving, retrieving, and updating registration data.
  // (will bump next_avail_xxxx_ids as needed)
  // (resource ids will be added/removed from the uncommitted/purgeable
  // lists as needed)

  // Reads a registration for |registration_id| and resource records associated
  // with it from the database. Returns OK if they are successfully read.
  // Otherwise, returns an error.
  Status ReadRegistration(int64_t registration_id,
                          const GURL& origin,
                          RegistrationData* registration,
                          std::vector<ResourceRecord>* resources);

  // Looks up the origin for the registration with |registration_id|. Returns OK
  // if a registration was found and read successfully. Otherwise, returns an
  // error.
  Status ReadRegistrationOrigin(int64_t registration_id, GURL* origin);

  // Writes |registration| and |resources| into the database and does following
  // things:
  //   - If an old version of the registration exists, deletes it and sets
  //   |deleted_version| to the old version registration data object
  //   |newly_purgeable_resources| to its resources. Otherwise, sets
  //   |deleted_version->version_id| to -1.
  //   - Bumps the next registration id and the next version id if needed.
  //   - Removes |resources| from the uncommitted list if exist.
  // Returns OK they are successfully written. Otherwise, returns an error.
  Status WriteRegistration(const RegistrationData& registration,
                           const std::vector<ResourceRecord>& resources,
                           RegistrationData* deleted_version,
                           std::vector<int64_t>* newly_purgeable_resources);

  // Updates a registration for |registration_id| to an active state. Returns OK
  // if it's successfully updated. Otherwise, returns an error.
  Status UpdateVersionToActive(int64_t registration_id, const GURL& origin);

  // Updates last check time of a registration for |registration_id| by |time|.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateLastCheckTime(int64_t registration_id,
                             const GURL& origin,
                             const base::Time& time);

  // Updates the navigation preload state for the specified registration.
  // Returns OK if it's successfully updated. Otherwise, returns an error.
  Status UpdateNavigationPreloadEnabled(int64_t registration_id,
                                        const GURL& origin,
                                        bool enable);
  Status UpdateNavigationPreloadHeader(int64_t registration_id,
                                       const GURL& origin,
                                       const std::string& value);

  // Deletes a registration for |registration_id| and moves resource records
  // associated with it into the purgeable list. If deletion occurred, sets
  // |version_id| to the id of the version that was deleted and
  // |newly_purgeable_resources| to its resources; otherwise, sets |version_id|
  // to -1. Returns OK if it's successfully deleted or not found in the
  // database. Otherwise, returns an error.
  Status DeleteRegistration(int64_t registration_id,
                            const GURL& origin,
                            RegistrationData* deleted_version,
                            std::vector<int64_t>* newly_purgeable_resources);

  // Reads user data for |registration_id| and |user_data_names| from the
  // database and writes them to |user_data_values|. Returns OK only if all keys
  // are found; otherwise NOT_FOUND, and |user_data_values| will be empty.
  Status ReadUserData(int64_t registration_id,
                      const std::vector<std::string>& user_data_names,
                      std::vector<std::string>* user_data_values);

  // Reads user data for |registration_id| and |user_data_name_prefix| from the
  // database and writes them to |user_data_values|. Returns OK if they are
  // successfully read or not found.
  Status ReadUserDataByKeyPrefix(int64_t registration_id,
                                 const std::string& user_data_name_prefix,
                                 std::vector<std::string>* user_data_values);

  // Reads user keys and associated data for |registration_id| and
  // |user_data_name_prefix| from the database and writes them to
  // |user_data_map|. The map keys are stripped of |user_data_name_prefix|.
  // Returns OK if they are successfully read or not found.
  Status ReadUserKeysAndDataByKeyPrefix(
      int64_t registration_id,
      const std::string& user_data_name_prefix,
      base::flat_map<std::string, std::string>* user_data_map);

  // Writes |name_value_pairs| into the database. Returns NOT_FOUND if the
  // registration specified by |registration_id| does not exist in the database.
  Status WriteUserData(
      int64_t registration_id,
      const GURL& origin,
      const std::vector<std::pair<std::string, std::string>>& name_value_pairs);

  // Deletes user data for |registration_id| and |user_data_names| from the
  // database. Returns OK if all are successfully deleted or not found in the
  // database.
  Status DeleteUserData(int64_t registration_id,
                        const std::vector<std::string>& user_data_names);

  // Deletes user data for |registration_id| and |user_data_name_prefixes| from
  // the database. Returns OK if all are successfully deleted or not found in
  // the database.
  Status DeleteUserDataByKeyPrefixes(
      int64_t registration_id,
      const std::vector<std::string>& user_data_name_prefixes);

  // Reads user data for all registrations that have data with |user_data_name|
  // from the database. Returns OK if they are successfully read or not found.
  Status ReadUserDataForAllRegistrations(
      const std::string& user_data_name,
      std::vector<std::pair<int64_t, std::string>>* user_data);

  // Reads user data for all registrations that have data with |user_data_name|
  // from the database. Returns OK if they are successfully read or not found.
  Status ReadUserDataForAllRegistrationsByKeyPrefix(
      const std::string& user_data_name_prefix,
      std::vector<std::pair<int64_t, std::string>>* user_data);

  // Resources should belong to one of following resource lists: uncommitted,
  // committed and purgeable.
  // As new resources are put into the diskcache, they go into the uncommitted
  // list. When a registration is saved that refers to those ids, they're moved
  // to the committed list. When a resource no longer has any registrations or
  // caches referring to it, it's added to the purgeable list. Periodically,
  // the purgeable list can be purged from the diskcache. At system startup, all
  // uncommitted ids are moved to the purgeable list.

  // Reads resource ids from the uncommitted list. Returns OK on success.
  // Otherwise clears |ids| and returns an error.
  Status GetUncommittedResourceIds(std::set<int64_t>* ids);

  // Writes resource ids into the uncommitted list. Returns OK on success.
  // Otherwise writes nothing and returns an error.
  Status WriteUncommittedResourceIds(const std::set<int64_t>& ids);

  // Reads resource ids from the purgeable list. Returns OK on success.
  // Otherwise clears |ids| and returns an error.
  Status GetPurgeableResourceIds(std::set<int64_t>* ids);

  // Deletes resource ids from the purgeable list. Returns OK on success.
  // Otherwise deletes nothing and returns an error.
  Status ClearPurgeableResourceIds(const std::set<int64_t>& ids);

  // Writes resource ids into the purgeable list and removes them from the
  // uncommitted list. Returns OK on success. Otherwise writes nothing and
  // returns an error.
  Status PurgeUncommittedResourceIds(const std::set<int64_t>& ids);

  // Deletes all data for |origins|, namely, unique origin, registrations and
  // resource records. Resources are moved to the purgeable list. Returns OK if
  // they are successfully deleted or not found in the database. Otherwise,
  // returns an error.
  Status DeleteAllDataForOrigins(
      const std::set<GURL>& origins,
      std::vector<int64_t>* newly_purgeable_resources);

  // Completely deletes the contents of the database.
  // Be careful using this function.
  Status DestroyDatabase();

 private:
  // Opens the database at the |path_|. This is lazily called when the first
  // database API is called. Returns OK if the database is successfully opened.
  // Returns NOT_FOUND if the database does not exist and |create_if_missing| is
  // false. Otherwise, returns an error.
  Status LazyOpen(bool create_if_missing);

  // Helper for LazyOpen(). |status| must be the return value from LazyOpen()
  // and this must be called just after LazyOpen() is called. Returns true if
  // the database is new or nonexistent, that is, it has never been used.
  bool IsNewOrNonexistentDatabase(Status status);

  // Reads the next available id for |id_key|. Returns OK if it's successfully
  // read. Fills |next_avail_id| with an initial value and returns OK if it's
  // not found in the database. Otherwise, returns an error.
  Status ReadNextAvailableId(const char* id_key, int64_t* next_avail_id);

  // Reads registration data for |registration_id| from the database. Returns OK
  // if successfully reads. Otherwise, returns an error.
  Status ReadRegistrationData(int64_t registration_id,
                              const GURL& origin,
                              RegistrationData* registration);

  // Parses |serialized| as a RegistrationData object and pushes it into |out|.
  ServiceWorkerDatabase::Status ParseRegistrationData(
      const std::string& serialized,
      RegistrationData* out);

  // Populates |batch| with operations to write |registration|. It does not
  // actually write to db yet.
  void WriteRegistrationDataInBatch(const RegistrationData& registration,
                                    leveldb::WriteBatch* batch);

  // Reads resource records for |registration| from the database. Returns OK if
  // it's successfully read or not found in the database. Otherwise, returns an
  // error.
  Status ReadResourceRecords(const RegistrationData& registration,
                             std::vector<ResourceRecord>* resources);

  // Parses |serialized| as a ResourceRecord object and pushes it into |out|.
  ServiceWorkerDatabase::Status ParseResourceRecord(
      const std::string& serialized,
      ResourceRecord* out);

  void WriteResourceRecordInBatch(const ResourceRecord& resource,
                                  int64_t version_id,
                                  leveldb::WriteBatch* batch);

  // Deletes resource records for |version_id| from the database. Returns OK if
  // they are successfully deleted or not found in the database. Otherwise,
  // returns an error.
  Status DeleteResourceRecords(int64_t version_id,
                               std::vector<int64_t>* newly_purgeable_resources,
                               leveldb::WriteBatch* batch);

  // Reads resource ids for |id_key_prefix| from the database. Returns OK if
  // it's successfully read or not found in the database. Otherwise, returns an
  // error.
  Status ReadResourceIds(const char* id_key_prefix, std::set<int64_t>* ids);

  // Write resource ids for |id_key_prefix| into the database. Returns OK on
  // success. Otherwise, returns writes nothing and returns an error.
  Status WriteResourceIdsInBatch(const char* id_key_prefix,
                                 const std::set<int64_t>& ids,
                                 leveldb::WriteBatch* batch);

  // Deletes resource ids for |id_key_prefix| from the database. Returns OK if
  // it's successfully deleted or not found in the database. Otherwise, returns
  // an error.
  Status DeleteResourceIdsInBatch(const char* id_key_prefix,
                                  const std::set<int64_t>& ids,
                                  leveldb::WriteBatch* batch);

  // Deletes all user data for |registration_id| from the database. Returns OK
  // if they are successfully deleted or not found in the database.
  Status DeleteUserDataForRegistration(int64_t registration_id,
                                       leveldb::WriteBatch* batch);

  // Reads the current schema version from the database. If the database hasn't
  // been written anything yet, sets |db_version| to 0 and returns OK.
  Status ReadDatabaseVersion(int64_t* db_version);

  // Writes a batch into the database.
  // NOTE: You must call this when you want to put something into the database
  // because this initializes the database if needed.
  Status WriteBatch(leveldb::WriteBatch* batch);

  // Bumps the next available id if |used_id| is greater than or equal to the
  // cached one.
  void BumpNextRegistrationIdIfNeeded(int64_t used_id,
                                      leveldb::WriteBatch* batch);
  void BumpNextResourceIdIfNeeded(int64_t used_id, leveldb::WriteBatch* batch);
  void BumpNextVersionIdIfNeeded(int64_t used_id, leveldb::WriteBatch* batch);

  bool IsOpen();

  void Disable(const base::Location& from_here, Status status);
  void HandleOpenResult(const base::Location& from_here, Status status);
  void HandleReadResult(const base::Location& from_here, Status status);
  void HandleWriteResult(const base::Location& from_here, Status status);

  const base::FilePath path_;
  std::unique_ptr<leveldb::Env> env_;
  std::unique_ptr<leveldb::DB> db_;

  int64_t next_avail_registration_id_;
  int64_t next_avail_resource_id_;
  int64_t next_avail_version_id_;

  enum State {
    UNINITIALIZED,
    INITIALIZED,
    DISABLED,
  };
  State state_;

  bool IsDatabaseInMemory() const;

  base::SequenceChecker sequence_checker_;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase_InMemory);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           DatabaseVersion_ValidSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           DatabaseVersion_ObsoleteSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           DatabaseVersion_CorruptedSchemaVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, GetNextAvailableIds);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           Registration_UninitializedDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest,
                           UserData_UninitializedDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, DestroyDatabase);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

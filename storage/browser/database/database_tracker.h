// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_TRACKER_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/database/database_connections.h"
#include "url/origin.h"

namespace sql {
class Database;
class MetaTable;
}

namespace storage {

class DatabaseQuotaClient;
class QuotaClientCallbackWrapper;
class QuotaManagerProxy;

COMPONENT_EXPORT(STORAGE_BROWSER)
extern const base::FilePath::CharType kDatabaseDirectoryName[];
COMPONENT_EXPORT(STORAGE_BROWSER)
extern const base::FilePath::CharType kTrackerDatabaseFileName[];

class DatabasesTable;

// This class is used to store information about all databases in an origin.
class COMPONENT_EXPORT(STORAGE_BROWSER) OriginInfo {
 public:
  OriginInfo();
  OriginInfo(const OriginInfo& origin_info);
  ~OriginInfo();

  const std::string& GetOriginIdentifier() const { return origin_identifier_; }
  int64_t TotalSize() const { return total_size_; }
  base::Time LastModified() const { return last_modified_; }
  void GetAllDatabaseNames(std::vector<std::u16string>* databases) const;
  int64_t GetDatabaseSize(const std::u16string& database_name) const;

 protected:
  OriginInfo(const std::string& origin_identifier, int64_t total_size);

  std::string origin_identifier_;
  int64_t total_size_;
  base::Time last_modified_;
  std::map<std::u16string, int64_t> database_sizes_;
};

// This class manages the main database and keeps track of open databases.
//
// The data in this class is not thread-safe, so all methods of this class
// should be called on the task runner returned by task_runner(). The only
// exceptions are the constructor, the destructor, and the getters explicitly
// marked as thread-safe. Although the destructor itself may run on any thread,
// destruction effectively occurs in Shutdown(), which expects to be called on
// task_runner().
class COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseTracker
    : public base::RefCountedThreadSafe<DatabaseTracker> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  class Observer {
   public:
    virtual void OnDatabaseSizeChanged(const std::string& origin_identifier,
                                       const std::u16string& database_name,
                                       int64_t database_size) = 0;
    virtual void OnDatabaseScheduledForDeletion(
        const std::string& origin_identifier,
        const std::u16string& database_name) = 0;

   protected:
    virtual ~Observer() = default;
  };

  static scoped_refptr<DatabaseTracker> Create(
      const base::FilePath& profile_path,
      bool is_incognito,
      scoped_refptr<QuotaManagerProxy> quota_manager_proxy);

  // Exposed for base::MakeRefCounted. Users should call Create().
  DatabaseTracker(const base::FilePath& profile_path,
                  bool is_incognito,
                  scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
                  base::PassKey<DatabaseTracker>);

  DatabaseTracker(const DatabaseTracker&) = delete;
  DatabaseTracker& operator=(const DatabaseTracker&) = delete;

  // Methods not explicity marked thread-safe must be called on this sequence.
  //
  // Thread-safe getter.
  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

  void DatabaseOpened(const std::string& origin_identifier,
                      const std::u16string& database_name,
                      const std::u16string& database_details,
                      int64_t* database_size);
  void DatabaseModified(const std::string& origin_identifier,
                        const std::u16string& database_name);
  void DatabaseClosed(const std::string& origin_identifier,
                      const std::u16string& database_name);
  void HandleSqliteError(const std::string& origin_identifier,
                         const std::u16string& database_name,
                         int error);

  void CloseDatabases(const DatabaseConnections& connections);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void CloseTrackerDatabaseAndClearCaches();

  // Thread-safe getter.
  const base::FilePath& database_directory() const { return db_dir_; }

  base::FilePath GetFullDBFilePath(const std::string& origin_identifier,
                                   const std::u16string& database_name);

  // virtual for unit-testing only
  virtual bool GetOriginInfo(const std::string& origin_id, OriginInfo* info);
  virtual bool GetAllOriginIdentifiers(std::vector<std::string>* origin_ids);
  virtual bool GetAllOriginsInfo(std::vector<OriginInfo>* origins_info);

  // Thread-safe getter.
  const scoped_refptr<QuotaManagerProxy>& quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

  bool IsDatabaseScheduledForDeletion(const std::string& origin_identifier,
                                      const std::u16string& database_name);

  // Deletes a single database.
  //
  // `callback` must be non-null, and is invoked upon completion with a
  // net::Error, which will most likely be net::OK or net::FAILED. `callback`
  // may be called before this method returns.
  void DeleteDatabase(const std::string& origin_identifier,
                      const std::u16string& database_name,
                      net::CompletionOnceCallback callback);

  // Deletes databases touched since `cutoff`.
  //
  // `callback` must must be non-null, and is invoked upon completion with a
  // net::Error. The status will be net::OK on success, or net::FAILED if not
  // all databases could be deleted. `callback` may be called before this method
  // returns.
  void DeleteDataModifiedSince(const base::Time& cutoff,
                               net::CompletionOnceCallback callback);

  // Deletes all databases that belong to the given origin.
  //
  // `callback` must must be non-null, and is invoked upon completion with a
  // net::Error. The status will be net::OK on success, or net::FAILED if not
  // all databases could be deleted. `callback` may be called before this method
  // returns.
  virtual void DeleteDataForOrigin(const url::Origin& origin,
                                   net::CompletionOnceCallback callback);

  bool IsIncognitoProfile() const { return is_incognito_; }

  const base::File* GetIncognitoFile(const std::u16string& vfs_file_path) const;
  const base::File* SaveIncognitoFile(const std::u16string& vfs_file_path,
                                      base::File file);
  void CloseIncognitoFileHandle(const std::u16string& vfs_file_path);
  bool HasSavedIncognitoFileHandle(const std::u16string& vfs_file_path) const;

  // Shutdown the database tracker, deleting database files if the tracker is
  // used for an Incognito profile.
  void Shutdown();

 protected:
  // Subclasses need PassKeys to call the constructor.
  static base::PassKey<DatabaseTracker> CreatePassKey() {
    return base::PassKey<DatabaseTracker>();
  }

 private:
  friend class base::RefCountedThreadSafe<DatabaseTracker>;
  friend class DatabaseTracker_TestHelper_Test;
  friend class MockDatabaseTracker;  // for testing

  using DatabaseSet = std::map<std::string, std::set<std::u16string>>;

  class CachedOriginInfo : public OriginInfo {
   public:
    CachedOriginInfo() : OriginInfo(std::string(), 0) {}
    void SetOriginIdentifier(const std::string& origin_identifier) {
      origin_identifier_ = origin_identifier;
    }
    void SetDatabaseSize(const std::u16string& database_name,
                         int64_t new_size) {
      // If the name does not exist in the map, operator[] creates a new entry
      // with a default-constructed value. The default-constructed value for
      // int64_t is zero (0), which is exactly what we want `old_size` to be set
      // to in this case.
      int64_t& database_size = database_sizes_[database_name];
      int64_t old_size = database_size;

      database_size = new_size;
      if (new_size != old_size)
        total_size_ += new_size - old_size;
    }
    void UpdateLastModified(base::Time last_modified) {
      if (last_modified > last_modified_)
        last_modified_ = last_modified;
    }
  };

  virtual ~DatabaseTracker();

  // Registers WebSQL's QuotaClient with the QuotaManager.
  void RegisterQuotaClient();

  // Deletes the directory that stores all DBs in Incognito mode, if it
  // exists.
  void DeleteIncognitoDBDirectory();

  bool DeleteClosedDatabase(const std::string& origin_identifier,
                            const std::u16string& database_name);

  // Delete all files belonging to the given origin given that no database
  // connections within this origin are open, or if |force| is true, delete
  // the meta data and rename the associated directory.
  bool DeleteOrigin(const std::string& origin_identifier, bool force);
  void DeleteDatabaseIfNeeded(const std::string& origin_identifier,
                              const std::u16string& database_name);

  bool LazyInit();
  bool UpgradeToCurrentVersion();
  void InsertOrUpdateDatabaseDetails(const std::string& origin_identifier,
                                     const std::u16string& database_name,
                                     const std::u16string& database_details);

  void ClearAllCachedOriginInfo();
  CachedOriginInfo* MaybeGetCachedOriginInfo(
      const std::string& origin_identifier,
      bool create_if_needed);
  CachedOriginInfo* GetCachedOriginInfo(
      const std::string& origin_identifier) {
    return MaybeGetCachedOriginInfo(origin_identifier, true);
  }

  int64_t GetDBFileSize(const std::string& origin_identifier,
                        const std::u16string& database_name);
  int64_t SeedOpenDatabaseInfo(const std::string& origin_identifier,
                               const std::u16string& database_name,
                               const std::u16string& description);
  int64_t UpdateOpenDatabaseInfoAndNotify(
      const std::string& origin_identifier,
      const std::u16string& database_name,
      const std::u16string* opt_description);
  int64_t UpdateOpenDatabaseSizeAndNotify(const std::string& origin_identifier,
                                          const std::u16string& database_name) {
    return UpdateOpenDatabaseInfoAndNotify(origin_identifier, database_name,
                                           nullptr);
  }

  void ScheduleDatabaseForDeletion(const std::string& origin_identifier,
                                   const std::u16string& database_name);
  // Schedule a set of open databases for deletion. If non-null, callback is
  // invoked upon completion.
  void ScheduleDatabasesForDeletion(const DatabaseSet& databases,
                                    net::CompletionOnceCallback callback);

  // Returns the directory where all DB files for the given origin are stored.
  base::FilePath GetOriginDirectory(const std::string& origin_identifier);

  bool is_initialized_ = false;
  const bool is_incognito_;
  bool shutting_down_ = false;
  const base::FilePath profile_path_;

  // Can be accessed from any thread via database_directory().
  //
  // Thread-safety argument: The member is immutable.
  const base::FilePath db_dir_;

  std::unique_ptr<sql::Database> db_;
  std::unique_ptr<DatabasesTable> databases_table_;
  std::unique_ptr<sql::MetaTable> meta_table_;
  base::ObserverList<Observer, true>::Unchecked observers_;
  std::map<std::string, CachedOriginInfo> origins_info_map_;
  DatabaseConnections database_connections_;

  // The set of databases that should be deleted but are still opened
  DatabaseSet dbs_to_be_deleted_;
  std::vector<std::pair<net::CompletionOnceCallback, DatabaseSet>>
      deletion_callbacks_;

  // Can be accessed from any thread via quota_manager_proxy().
  //
  // Thread-safety argument: The reference is immutable.
  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  // Sequence where file I/O is allowed.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<DatabaseQuotaClient> quota_client_;
  std::unique_ptr<storage::QuotaClientCallbackWrapper> quota_client_wrapper_;

  // When in Incognito mode, store a DELETE_ON_CLOSE handle to each
  // main DB and journal file that was accessed. When the Incognito profile
  // goes away (or when the browser crashes), all these handles will be
  // closed, and the files will be deleted.
  std::map<std::u16string, raw_ptr<base::File, CtnExperimental>>
      incognito_file_handles_;

  // In a non-Incognito profile, all DBs in an origin are stored in a
  // directory named after the origin. In an Incognito profile though, we do
  // not want the directory structure to reveal the origins visited by the user
  // (in case the browser process crashes and those directories are not
  // deleted). So we use this map to assign directory names that do not reveal
  // this information.
  std::map<std::string, std::u16string> incognito_origin_directories_;
  int incognito_origin_directories_generator_ = 0;

  mojo::Receiver<mojom::QuotaClient> quota_client_receiver_;

  FRIEND_TEST_ALL_PREFIXES(DatabaseTracker, TestHelper);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_TRACKER_H_

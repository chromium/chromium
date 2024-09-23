// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/database/database_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"
#include "storage/browser/database/database_quota_client.h"
#include "storage/browser/database/database_util.h"
#include "storage/browser/database/databases_table.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/sqlite/sqlite3.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

const base::FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases");
const base::FilePath::CharType kIncognitoDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("databases-off-the-record");
const base::FilePath::CharType kTrackerDatabaseFileName[] =
    FILE_PATH_LITERAL("Databases.db");
static const int kDatabaseTrackerCurrentSchemaVersion = 2;
static const int kDatabaseTrackerCompatibleVersion = 1;

const base::FilePath::CharType kTemporaryDirectoryPrefix[] =
    FILE_PATH_LITERAL("DeleteMe");
const base::FilePath::CharType kTemporaryDirectoryPattern[] =
    FILE_PATH_LITERAL("DeleteMe*");

OriginInfo::OriginInfo()
    : total_size_(0) {}

OriginInfo::OriginInfo(const OriginInfo& origin_info) = default;

OriginInfo::~OriginInfo() = default;

int64_t OriginInfo::GetDatabaseSize(const std::u16string& database_name) const {
  auto it = database_sizes_.find(database_name);
  if (it != database_sizes_.end())
    return it->second;
  return 0;
}

OriginInfo::OriginInfo(const std::string& origin_identifier, int64_t total_size)
    : origin_identifier_(origin_identifier), total_size_(total_size) {}

scoped_refptr<DatabaseTracker> DatabaseTracker::Create(
    const base::FilePath& profile_path,
    bool is_incognito,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy) {
  auto database_tracker = base::MakeRefCounted<DatabaseTracker>(
      profile_path, is_incognito, std::move(quota_manager_proxy),
      base::PassKey<DatabaseTracker>());
  database_tracker->RegisterQuotaClient();
  return database_tracker;
}

DatabaseTracker::DatabaseTracker(
    const base::FilePath& profile_path,
    bool is_incognito,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    base::PassKey<DatabaseTracker>)
    : is_incognito_(is_incognito),
      profile_path_(profile_path),
      db_dir_(is_incognito_
                  ? profile_path_.Append(kIncognitoDatabaseDirectoryName)
                  : profile_path_.Append(kDatabaseDirectoryName)),
      db_(std::make_unique<sql::Database>(sql::DatabaseOptions{
          .page_size = 4096,
          .cache_size = 500,
      })),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           // SKIP_ON_SHUTDOWN cannot be used because Shutdown() needs to run
           // before the destructor, and Shutdown() is ran by PostTask()ing to
           // this sequence. See https://crbug.com/1220191.
           //
           // We may be able to switch to SKIP_ON_SHUTDOWN if we get
           // DatabaseTracker to be used entirely on the database sequence, so
           // the destructor can absorb the logic that is currently in
           // Shutdown().
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      quota_client_(std::make_unique<DatabaseQuotaClient>(*this)),
      quota_client_wrapper_(
          std::make_unique<QuotaClientCallbackWrapper>(quota_client_.get())),
      quota_client_receiver_(quota_client_wrapper_.get()) {}

DatabaseTracker::~DatabaseTracker() {
  // base::RefCountedThreadSafe inserts the appropriate barriers to ensure
  // member access in the destructor does not introduce data races.
  DCHECK(dbs_to_be_deleted_.empty());
  DCHECK(deletion_callbacks_.empty());

  DCHECK(!quota_client_);
  DCHECK(!quota_client_wrapper_);
  DCHECK(!quota_client_receiver_.is_bound());
}

void DatabaseTracker::RegisterQuotaClient() {
  if (!quota_manager_proxy_)
    return;

  // QuotaManagerProxy::RegisterClient() must be called synchronously during
  // DatabaseTracker creation until crbug.com/1182630 is fixed.
  mojo::PendingRemote<storage::mojom::QuotaClient> quota_client_remote;
  mojo::PendingReceiver<storage::mojom::QuotaClient> quota_client_receiver =
      quota_client_remote.InitWithNewPipeAndPassReceiver();
  quota_manager_proxy_->RegisterClient(std::move(quota_client_remote),
                                       storage::QuotaClientType::kDatabase,
                                       {blink::mojom::StorageType::kTemporary});

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<DatabaseTracker> self,
             mojo::PendingReceiver<storage::mojom::QuotaClient> receiver) {
            self->quota_client_receiver_.Bind(std::move(receiver));
          },
          base::RetainedRef(this), std::move(quota_client_receiver)));
}

void DatabaseTracker::DatabaseOpened(const std::string& origin_identifier,
                                     const std::u16string& database_name,
                                     const std::u16string& database_description,
                                     int64_t* database_size) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (shutting_down_ || !LazyInit()) {
    *database_size = 0;
    return;
  }

  if (quota_manager_proxy_.get())
    quota_manager_proxy_->NotifyBucketAccessed(
        BucketLocator::ForDefaultBucket(blink::StorageKey::CreateFirstParty(
            GetOriginFromIdentifier(origin_identifier))),
        base::Time::Now());

  InsertOrUpdateDatabaseDetails(origin_identifier, database_name,
                                database_description);
  if (database_connections_.AddConnection(origin_identifier, database_name)) {
    *database_size = SeedOpenDatabaseInfo(origin_identifier,
                                          database_name,
                                          database_description);
    return;
  }
  *database_size  = UpdateOpenDatabaseInfoAndNotify(origin_identifier,
                                                    database_name,
                                                    &database_description);
}

void DatabaseTracker::DatabaseModified(const std::string& origin_identifier,
                                       const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return;
  UpdateOpenDatabaseSizeAndNotify(origin_identifier, database_name);
}

void DatabaseTracker::DatabaseClosed(const std::string& origin_identifier,
                                     const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_);
    return;
  }

  // We call NotifiyStorageAccessed when a db is opened and also when
  // closed because we don't call it for read while open.
  if (quota_manager_proxy_.get())
    quota_manager_proxy_->NotifyBucketAccessed(
        BucketLocator::ForDefaultBucket(blink::StorageKey::CreateFirstParty(
            GetOriginFromIdentifier(origin_identifier))),
        base::Time::Now());

  UpdateOpenDatabaseSizeAndNotify(origin_identifier, database_name);
  if (database_connections_.RemoveConnection(origin_identifier, database_name))
    DeleteDatabaseIfNeeded(origin_identifier, database_name);
}

void DatabaseTracker::HandleSqliteError(const std::string& origin_identifier,
                                        const std::u16string& database_name,
                                        int error) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // We only handle errors that indicate corruption and we
  // do so with a heavy hand, we delete it. Any renderers/workers
  // with this database open will receive a message to close it
  // immediately, once all have closed, the files will be deleted.
  // In the interim, all attempts to open a new connection to that
  // database will fail.
  // Note: the client-side filters out all but these two errors as
  // a small optimization, see WebDatabaseObserverImpl::HandleSqliteError.
  if (error == SQLITE_CORRUPT || error == SQLITE_NOTADB) {
    DeleteDatabase(origin_identifier, database_name, base::DoNothing());
  }
}

void DatabaseTracker::CloseDatabases(const DatabaseConnections& connections) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (database_connections_.IsEmpty()) {
    DCHECK(!is_initialized_ || connections.IsEmpty());
    return;
  }

  // When being closed by this route, there's a chance that
  // the tracker missed some DatabaseModified calls. This method is used
  // when a renderer crashes to cleanup its open resources.
  // We need to examine what we have in connections for the
  // size of each open databases and notify any differences between the
  // actual file sizes now.
  for (auto& pair : connections.ListConnections())
    UpdateOpenDatabaseSizeAndNotify(pair.first, pair.second);

  for (auto& pair : database_connections_.RemoveConnections(connections))
    DeleteDatabaseIfNeeded(pair.first, pair.second);
}

void DatabaseTracker::DeleteDatabaseIfNeeded(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!database_connections_.IsDatabaseOpened(origin_identifier,
                                                 database_name));
  if (IsDatabaseScheduledForDeletion(origin_identifier, database_name)) {
    DeleteClosedDatabase(origin_identifier, database_name);
    dbs_to_be_deleted_[origin_identifier].erase(database_name);
    if (dbs_to_be_deleted_[origin_identifier].empty())
      dbs_to_be_deleted_.erase(origin_identifier);

    auto callback = deletion_callbacks_.begin();
    while (callback != deletion_callbacks_.end()) {
      auto found_origin = callback->second.find(origin_identifier);
      if (found_origin != callback->second.end()) {
        std::set<std::u16string>& databases = found_origin->second;
        databases.erase(database_name);
        if (databases.empty()) {
          callback->second.erase(found_origin);
          if (callback->second.empty()) {
            std::move(callback->first).Run(net::OK);
            callback = deletion_callbacks_.erase(callback);
            continue;
          }
        }
      }

      ++callback;
    }
  }
}

void DatabaseTracker::AddObserver(Observer* observer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  observers_.AddObserver(observer);
}

void DatabaseTracker::RemoveObserver(Observer* observer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // When we remove a listener, we do not know which cached information
  // is still needed and which information can be discarded. So we just
  // clear all caches and re-populate them as needed.
  observers_.RemoveObserver(observer);
  ClearAllCachedOriginInfo();
}

void DatabaseTracker::CloseTrackerDatabaseAndClearCaches() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  ClearAllCachedOriginInfo();

  if (!is_incognito_) {
    meta_table_.reset(nullptr);
    databases_table_.reset(nullptr);
    db_->Close();
    is_initialized_ = false;
  }
}

base::FilePath DatabaseTracker::GetOriginDirectory(
    const std::string& origin_identifier) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::u16string origin_directory;

  if (!is_incognito_) {
    origin_directory = base::UTF8ToUTF16(origin_identifier);
  } else {
    auto it = incognito_origin_directories_.find(origin_identifier);
    if (it != incognito_origin_directories_.end()) {
      origin_directory = it->second;
    } else {
      origin_directory =
          base::NumberToString16(incognito_origin_directories_generator_++);
      incognito_origin_directories_[origin_identifier] = origin_directory;
    }
  }

  return db_dir_.Append(base::FilePath::FromUTF16Unsafe(origin_directory));
}

base::FilePath DatabaseTracker::GetFullDBFilePath(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!origin_identifier.empty());
  if (!LazyInit())
    return base::FilePath();

  int64_t id =
      databases_table_->GetDatabaseID(origin_identifier, database_name);
  if (id < 0)
    return base::FilePath();

  return GetOriginDirectory(origin_identifier)
      .AppendASCII(base::NumberToString(id));
}

bool DatabaseTracker::GetOriginInfo(const std::string& origin_identifier,
                                    OriginInfo* info) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(info);
  CachedOriginInfo* cached_info = GetCachedOriginInfo(origin_identifier);
  if (!cached_info)
    return false;
  *info = OriginInfo(*cached_info);
  return true;
}

bool DatabaseTracker::GetAllOriginIdentifiers(
    std::vector<std::string>* origin_identifiers) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origin_identifiers);
  DCHECK(origin_identifiers->empty());
  if (!LazyInit())
    return false;
  return databases_table_->GetAllOriginIdentifiers(origin_identifiers);
}

bool DatabaseTracker::GetAllOriginsInfo(
    std::vector<OriginInfo>* origins_info) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origins_info);
  DCHECK(origins_info->empty());

  std::vector<std::string> origins;
  if (!GetAllOriginIdentifiers(&origins))
    return false;

  for (const auto& origin : origins) {
    CachedOriginInfo* origin_info = GetCachedOriginInfo(origin);
    if (!origin_info) {
      // Restore 'origins_info' to its initial state.
      origins_info->clear();
      return false;
    }
    origins_info->push_back(OriginInfo(*origin_info));
  }

  return true;
}

bool DatabaseTracker::DeleteClosedDatabase(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return false;

  // Check if the database is opened by any renderer.
  if (database_connections_.IsDatabaseOpened(origin_identifier, database_name))
    return false;

  int64_t db_file_size = quota_manager_proxy_.get()
                             ? GetDBFileSize(origin_identifier, database_name)
                             : 0;

  // Try to delete the file on the hard drive.
  base::FilePath db_file = GetFullDBFilePath(origin_identifier, database_name);
  if (!sql::Database::Delete(db_file))
    return false;

  if (quota_manager_proxy_.get() && db_file_size) {
    quota_manager_proxy_->NotifyBucketModified(
        QuotaClientType::kDatabase,
        BucketLocator::ForDefaultBucket(blink::StorageKey::CreateFirstParty(
            GetOriginFromIdentifier(origin_identifier))),
        -db_file_size, base::Time::Now(),
        base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
  }

  // Clean up the main database and invalidate the cached record.
  databases_table_->DeleteDatabaseDetails(origin_identifier, database_name);
  origins_info_map_.erase(origin_identifier);

  std::vector<DatabaseDetails> details;
  if (databases_table_->GetAllDatabaseDetailsForOriginIdentifier(
          origin_identifier, &details) && details.empty()) {
    // Try to delete the origin in case this was the last database.
    DeleteOrigin(origin_identifier, false);
  }
  return true;
}

bool DatabaseTracker::DeleteOrigin(const std::string& origin_identifier,
                                   bool force) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return false;

  // Check if any database in this origin is opened by any renderer.
  if (database_connections_.IsOriginUsed(origin_identifier) && !force)
    return false;

  int64_t deleted_size = 0;
  if (quota_manager_proxy_.get()) {
    CachedOriginInfo* origin_info = GetCachedOriginInfo(origin_identifier);
    if (origin_info)
      deleted_size = origin_info->TotalSize();
  }

  origins_info_map_.erase(origin_identifier);
  base::FilePath origin_dir = GetOriginDirectory(origin_identifier);

  // Create a temporary directory to move possibly still existing databases to,
  // as we can't delete the origin directory on windows if it contains opened
  // files.
  base::FilePath new_origin_dir;
  base::CreateTemporaryDirInDir(db_dir_,
                                kTemporaryDirectoryPrefix,
                                &new_origin_dir);
  base::FileEnumerator databases(
      origin_dir,
      false,
      base::FileEnumerator::FILES);
  for (base::FilePath database = databases.Next(); !database.empty();
       database = databases.Next()) {
    base::FilePath new_file = new_origin_dir.Append(database.BaseName());
    base::Move(database, new_file);
  }
  base::DeletePathRecursively(origin_dir);
  base::DeletePathRecursively(new_origin_dir);  // Might fail on windows.

  if (is_incognito_) {
    incognito_origin_directories_.erase(origin_identifier);

    // TODO(jsbell): Consider alternate data structures to avoid this
    // linear scan.
    for (auto it = incognito_file_handles_.begin();
         it != incognito_file_handles_.end();) {
      std::string id;
      if (DatabaseUtil::CrackVfsFileName(it->first, &id, nullptr, nullptr) &&
          id == origin_identifier) {
        delete it->second;
        it = incognito_file_handles_.erase(it);
      } else {
        ++it;
      }
    }
  }

  databases_table_->DeleteOriginIdentifier(origin_identifier);

  if (quota_manager_proxy_.get() && deleted_size) {
    quota_manager_proxy_->NotifyBucketModified(
        QuotaClientType::kDatabase,
        BucketLocator::ForDefaultBucket(blink::StorageKey::CreateFirstParty(
            GetOriginFromIdentifier(origin_identifier))),
        -deleted_size, base::Time::Now(),
        base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
  }

  return true;
}

bool DatabaseTracker::IsDatabaseScheduledForDeletion(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = dbs_to_be_deleted_.find(origin_identifier);
  if (it == dbs_to_be_deleted_.end())
    return false;

  const std::set<std::u16string>& databases = it->second;
  return (databases.find(database_name) != databases.end());
}

bool DatabaseTracker::LazyInit() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!is_initialized_ && !shutting_down_) {
    DCHECK(!db_->is_open());
    DCHECK(!databases_table_.get());
    DCHECK(!meta_table_.get());

    // If there are left-over directories from failed deletion attempts, clean
    // them up.
    if (base::DirectoryExists(db_dir_)) {
      base::FileEnumerator directories(
          db_dir_,
          false,
          base::FileEnumerator::DIRECTORIES,
          kTemporaryDirectoryPattern);
      for (base::FilePath directory = directories.Next(); !directory.empty();
           directory = directories.Next()) {
        base::DeletePathRecursively(directory);
      }
    }

    db_->set_histogram_tag("DatabaseTracker");

    // If the tracker database exists, but it's corrupt or doesn't
    // have a meta table, delete the database directory.
    const base::FilePath kTrackerDatabaseFullPath =
        db_dir_.Append(base::FilePath(kTrackerDatabaseFileName));
    if (base::DirectoryExists(db_dir_) &&
        base::PathExists(kTrackerDatabaseFullPath) &&
        (!db_->Open(kTrackerDatabaseFullPath) ||
         !sql::MetaTable::DoesTableExist(db_.get()))) {
      db_->Close();
      if (!base::DeletePathRecursively(db_dir_))
        return false;
    }

    databases_table_ = std::make_unique<DatabasesTable>(db_.get());
    meta_table_ = std::make_unique<sql::MetaTable>();

    is_initialized_ = base::CreateDirectory(db_dir_) &&
                      (db_->is_open() ||
                       (is_incognito_ ? db_->OpenInMemory()
                                      : db_->Open(kTrackerDatabaseFullPath))) &&
                      UpgradeToCurrentVersion();
    if (!is_initialized_) {
      databases_table_.reset(nullptr);
      meta_table_.reset(nullptr);
      db_->Close();
    }
  }
  return is_initialized_;
}

bool DatabaseTracker::UpgradeToCurrentVersion() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin() ||
      !meta_table_->Init(db_.get(), kDatabaseTrackerCurrentSchemaVersion,
                         kDatabaseTrackerCompatibleVersion) ||
      (meta_table_->GetCompatibleVersionNumber() >
       kDatabaseTrackerCurrentSchemaVersion) ||
      !databases_table_->Init())
    return false;

  if (meta_table_->GetVersionNumber() < kDatabaseTrackerCurrentSchemaVersion &&
      !meta_table_->SetVersionNumber(kDatabaseTrackerCurrentSchemaVersion)) {
    return false;
  }

  return transaction.Commit();
}

void DatabaseTracker::InsertOrUpdateDatabaseDetails(
    const std::string& origin_identifier,
    const std::u16string& database_name,
    const std::u16string& database_description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DatabaseDetails details;
  if (!databases_table_->GetDatabaseDetails(
          origin_identifier, database_name, &details)) {
    details.origin_identifier = origin_identifier;
    details.database_name = database_name;
    details.description = database_description;
    databases_table_->InsertDatabaseDetails(details);
  } else if (details.description != database_description) {
    details.description = database_description;
    databases_table_->UpdateDatabaseDetails(details);
  }
}

void DatabaseTracker::ClearAllCachedOriginInfo() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  origins_info_map_.clear();
}

DatabaseTracker::CachedOriginInfo* DatabaseTracker::MaybeGetCachedOriginInfo(
    const std::string& origin_identifier, bool create_if_needed) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!LazyInit())
    return nullptr;

  // Populate the cache with data for this origin if needed.
  if (!base::Contains(origins_info_map_, origin_identifier)) {
    if (!create_if_needed)
      return nullptr;

    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOriginIdentifier(
            origin_identifier, &details)) {
      return nullptr;
    }

    CachedOriginInfo& origin_info = origins_info_map_[origin_identifier];
    origin_info.SetOriginIdentifier(origin_identifier);
    for (const auto& db : details) {
      int64_t db_file_size;
      if (database_connections_.IsDatabaseOpened(origin_identifier,
                                                 db.database_name)) {
        db_file_size = database_connections_.GetOpenDatabaseSize(
            origin_identifier, db.database_name);
      } else {
        db_file_size = GetDBFileSize(origin_identifier, db.database_name);
      }
      origin_info.SetDatabaseSize(db.database_name, db_file_size);

      base::FilePath path =
          GetFullDBFilePath(origin_identifier, db.database_name);
      base::File::Info file_info;
      // TODO(jsbell): Avoid duplicate base::GetFileInfo calls between this and
      // the GetDBFileSize() call above.
      if (base::GetFileInfo(path, &file_info)) {
        origin_info.UpdateLastModified(file_info.last_modified);
      }
    }
  }

  return &origins_info_map_[origin_identifier];
}

int64_t DatabaseTracker::GetDBFileSize(const std::string& origin_identifier,
                                       const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::FilePath db_file_name = GetFullDBFilePath(origin_identifier,
                                                  database_name);
  int64_t db_file_size = 0;
  if (!base::GetFileSize(db_file_name, &db_file_size))
    db_file_size = 0;
  return db_file_size;
}

int64_t DatabaseTracker::SeedOpenDatabaseInfo(
    const std::string& origin_id,
    const std::u16string& name,
    const std::u16string& description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_connections_.IsDatabaseOpened(origin_id, name));
  int64_t size = GetDBFileSize(origin_id, name);
  database_connections_.SetOpenDatabaseSize(origin_id, name,  size);
  CachedOriginInfo* info = MaybeGetCachedOriginInfo(origin_id, false);
  if (info) {
    info->SetDatabaseSize(name, size);
  }
  return size;
}

int64_t DatabaseTracker::UpdateOpenDatabaseInfoAndNotify(
    const std::string& origin_id,
    const std::u16string& name,
    const std::u16string* opt_description) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_connections_.IsDatabaseOpened(origin_id, name));
  int64_t new_size = GetDBFileSize(origin_id, name);
  int64_t old_size = database_connections_.GetOpenDatabaseSize(origin_id, name);
  CachedOriginInfo* info = MaybeGetCachedOriginInfo(origin_id, false);
  if (old_size != new_size) {
    database_connections_.SetOpenDatabaseSize(origin_id, name, new_size);
    if (info)
      info->SetDatabaseSize(name, new_size);

    if (quota_manager_proxy_.get()) {
      quota_manager_proxy_->NotifyBucketModified(
          QuotaClientType::kDatabase,
          BucketLocator::ForDefaultBucket(blink::StorageKey::CreateFirstParty(
              GetOriginFromIdentifier(origin_id))),
          new_size - old_size, base::Time::Now(),
          base::SequencedTaskRunner::GetCurrentDefault(), base::DoNothing());
    }

    for (auto& observer : observers_)
      observer.OnDatabaseSizeChanged(origin_id, name, new_size);
  }
  return new_size;
}

void DatabaseTracker::ScheduleDatabaseForDeletion(
    const std::string& origin_identifier,
    const std::u16string& database_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_connections_.IsDatabaseOpened(origin_identifier,
                                                database_name));
  dbs_to_be_deleted_[origin_identifier].insert(database_name);
  for (auto& observer : observers_)
    observer.OnDatabaseScheduledForDeletion(origin_identifier, database_name);
}

void DatabaseTracker::ScheduleDatabasesForDeletion(
    const DatabaseSet& databases,
    net::CompletionOnceCallback callback) {
  DCHECK(!databases.empty());

  if (!callback.is_null())
    deletion_callbacks_.emplace_back(std::move(callback), databases);
  for (const auto& origin_dbs : databases) {
    for (const std::u16string& db : origin_dbs.second)
      ScheduleDatabaseForDeletion(origin_dbs.first, db);
  }
}

void DatabaseTracker::DeleteDatabase(const std::string& origin_identifier,
                                     const std::u16string& database_name,
                                     net::CompletionOnceCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());
  if (!LazyInit()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  if (database_connections_.IsDatabaseOpened(origin_identifier,
                                             database_name)) {
    DatabaseSet set;
    set[origin_identifier].insert(database_name);
    deletion_callbacks_.emplace_back(std::move(callback), std::move(set));
    ScheduleDatabaseForDeletion(origin_identifier, database_name);
    return;
  }

  DeleteClosedDatabase(origin_identifier, database_name);
  std::move(callback).Run(net::OK);
}

void DatabaseTracker::DeleteDataModifiedSince(
    const base::Time& cutoff,
    net::CompletionOnceCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());
  if (!LazyInit()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  std::vector<std::string> origins_identifiers;
  if (!databases_table_->GetAllOriginIdentifiers(&origins_identifiers)) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  DatabaseSet to_be_deleted;
  int rv = net::OK;
  for (const auto& origin : origins_identifiers) {
    std::vector<DatabaseDetails> details;
    if (!databases_table_->GetAllDatabaseDetailsForOriginIdentifier(origin,
                                                                    &details)) {
      rv = net::ERR_FAILED;
    }
    for (const DatabaseDetails& db : details) {
      base::FilePath db_file = GetFullDBFilePath(origin, db.database_name);
      base::File::Info file_info;
      base::GetFileInfo(db_file, &file_info);
      if (file_info.last_modified < cutoff)
        continue;

      // Check if the database is opened by any renderer.
      if (database_connections_.IsDatabaseOpened(origin, db.database_name)) {
        to_be_deleted[origin].insert(db.database_name);
      } else {
        DeleteClosedDatabase(origin, db.database_name);
      }
    }
  }

  if (rv != net::OK) {
    DCHECK_EQ(rv, net::ERR_FAILED);
    std::move(callback).Run(rv);
    return;
  }

  if (!to_be_deleted.empty()) {
    ScheduleDatabasesForDeletion(to_be_deleted, std::move(callback));
    return;
  }

  std::move(callback).Run(net::OK);
}

void DatabaseTracker::DeleteDataForOrigin(
    const url::Origin& origin,
    net::CompletionOnceCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());
  if (!LazyInit()) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  const std::string identifier = GetIdentifierFromOrigin(origin);

  std::vector<DatabaseDetails> details;
  if (!databases_table_->GetAllDatabaseDetailsForOriginIdentifier(identifier,
                                                                  &details)) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  DatabaseSet to_be_deleted;
  for (const DatabaseDetails& db : details) {
    // Check if the database is opened by any renderer.
    if (database_connections_.IsDatabaseOpened(identifier, db.database_name)) {
      to_be_deleted[identifier].insert(db.database_name);
    } else {
      DeleteClosedDatabase(identifier, db.database_name);
    }
  }

  if (!to_be_deleted.empty()) {
    ScheduleDatabasesForDeletion(to_be_deleted, std::move(callback));
    return;
  }

  std::move(callback).Run(net::OK);
}

const base::File* DatabaseTracker::GetIncognitoFile(
    const std::u16string& vfs_file_name) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_incognito_);
  auto it = incognito_file_handles_.find(vfs_file_name);
  if (it != incognito_file_handles_.end())
    return it->second;

  return nullptr;
}

const base::File* DatabaseTracker::SaveIncognitoFile(
    const std::u16string& vfs_file_name,
    base::File file) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_incognito_);
  if (!file.IsValid())
    return nullptr;

  base::File* to_insert = new base::File(std::move(file));
  auto rv =
      incognito_file_handles_.insert(std::make_pair(vfs_file_name, to_insert));
  DCHECK(rv.second);
  base::RecordAction(base::UserMetricsAction("IncognitoWebSQL_Created"));
  return rv.first->second;
}

void DatabaseTracker::CloseIncognitoFileHandle(
    const std::u16string& vfs_file_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(is_incognito_);
  CHECK(incognito_file_handles_.find(vfs_file_name) !=
            incognito_file_handles_.end(),
        base::NotFatalUntil::M130);

  auto it = incognito_file_handles_.find(vfs_file_name);
  if (it != incognito_file_handles_.end()) {
    delete it->second;
    incognito_file_handles_.erase(it);
  }
  base::RecordAction(base::UserMetricsAction("IncognitoWebSQL_Released"));
}

bool DatabaseTracker::HasSavedIncognitoFileHandle(
    const std::u16string& vfs_file_name) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return base::Contains(incognito_file_handles_, vfs_file_name);
}

void DatabaseTracker::DeleteIncognitoDBDirectory() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  is_initialized_ = false;

  for (auto& pair : incognito_file_handles_)
    delete pair.second;

  base::FilePath incognito_db_dir =
      profile_path_.Append(kIncognitoDatabaseDirectoryName);
  if (base::DirectoryExists(incognito_db_dir))
    base::DeletePathRecursively(incognito_db_dir);
}

void DatabaseTracker::Shutdown() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (shutting_down_) {
    NOTREACHED();
  }
  shutting_down_ = true;

  // The mojo receiver must be reset before the instance it calls into is
  // destroyed.
  quota_client_receiver_.reset();
  quota_client_wrapper_.reset();
  quota_client_.reset();

  if (is_incognito_)
    DeleteIncognitoDBDirectory();
  CloseTrackerDatabaseAndClearCaches();

  // Explicitly destroy `db_` on the correct sequence rather than waiting for
  // the destructor, which may run on another sequence. Destroy related fields
  // first to prevent dangling pointers. Destruction order is important.
  meta_table_.reset();
  databases_table_.reset();
  db_.reset();
}

}  // namespace storage

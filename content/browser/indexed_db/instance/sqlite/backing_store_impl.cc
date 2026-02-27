// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/backing_store_impl.h"

#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "content/browser/indexed_db/file_path_util.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/instance/backing_store_util.h"
#include "content/browser/indexed_db/instance/sqlite/backing_store_database_impl.h"
#include "content/browser/indexed_db/instance/sqlite/database_connection.h"
#include "content/browser/indexed_db/status.h"

namespace content::indexed_db::sqlite {

BackingStoreImpl::BackingStoreImpl(
    base::FilePath directory,
    storage::mojom::BlobStorageContext& blob_storage_context,
    base::RepeatingCallback<
        std::vector<PartitionedLock>(const std::u16string& name)> lock_database)
    : directory_(std::move(directory)),
      blob_storage_context_(blob_storage_context),
      lock_database_(std::move(lock_database)) {}

BackingStoreImpl::~BackingStoreImpl() = default;

// static
uint64_t BackingStoreImpl::SumSizesOfDatabaseFiles(
    const base::FilePath& directory,
    base::FunctionRef<bool(const base::FilePath&)> filter) {
  uint64_t total_size = 0;
  EnumerateDatabasesInDirectory(directory, [&](const base::FilePath& path) {
    if (filter(path)) {
      total_size += base::GetFileSize(path).value_or(0);
    }
  });
  return total_size;
}

void BackingStoreImpl::RunIdleTasks() {
  for (auto& [_, connection] : open_connections_) {
    connection->PerformIdleMaintenance();
  }
}

bool BackingStoreImpl::CanOpportunisticallyClose() const {
  // There's not much of a point in deleting `this` since it doesn't use many
  // resources (just a tiny amount of memory). But for now, match the logic of
  // the LevelDB store, where `this` is cleaned up if there are no active
  // databases and no blobs. This is as simple as checking if there are any
  // `DatabaseConnection` objects.
  return open_connections_.empty();
}

void BackingStoreImpl::OnForceClosing() {
  is_force_closing_ = true;
}

void BackingStoreImpl::SignalWhenDestructionComplete(
    base::WaitableEvent* signal_on_destruction) && {
  for (auto& [_, db] : open_connections_) {
    std::move(*db).GetCleanupTask(/*force_closing=*/true).Run();
  }
  open_connections_.clear();

  if (!cleanup_task_runner_) {
    signal_on_destruction->Signal();
    return;
  }

  // Signal when the last cleanup task completes. `signal_on_destruction` is
  // guaranteed to outlive `this`.
  cleanup_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                base::Unretained(signal_on_destruction)));
}

void BackingStoreImpl::StartPreCloseTasks(base::OnceClosure on_done) {
  // Pre-close tasks are run only when the backing store is opportunistically
  // closing with no open connections. If there are cleanups in progress, this
  // is a chance to asynchronously wait for them to complete (as opposed to the
  // synchronous wait in the `SignalWhenDestructionComplete()` path).
  // TODO(crbug.com/436880909): Consolidate with `CanOpportunisticallyClose()`
  // and `SignalWhenDestructionComplete()` for SQLite.
  CHECK(open_connections_.empty());
  if (!cleanup_task_runner_) {
    std::move(on_done).Run();
    return;
  }
  cleanup_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(on_done));
}

void BackingStoreImpl::StopPreCloseTasks() {
  // No-op since ongoing cleanups must complete anyway.
}

uint64_t BackingStoreImpl::EstimateSize(bool /*write_in_progress*/) const {
  uint64_t total_size = 0;
  std::set<base::FilePath> already_open_file_names;
  for (const auto& [name, db] : open_connections_) {
    already_open_file_names.insert(DatabaseNameToFileName(name));
    // When the database is open, querying its size directly provides a more
    // "real time" estimate.
    total_size += db->GetSize();
  }

  if (!in_memory()) {
    total_size +=
        SumSizesOfDatabaseFiles(directory_, [&](const base::FilePath& path) {
          return !already_open_file_names.contains(path.BaseName());
        });
  }

  return total_size;
}

StatusOr<bool> BackingStoreImpl::DatabaseExists(std::u16string_view name) {
  if (auto it = open_connections_.find(std::u16string(name));
      it != open_connections_.end()) {
    return !it->second->IsZygotic();
  }

  if (auto it = cached_versions_.find(std::u16string(name));
      it != cached_versions_.end()) {
    return it->second != blink::IndexedDBDatabaseMetadata::NO_VERSION;
  }

  if (in_memory()) {
    return false;
  }

  return base::PathExists(directory_.Append(DatabaseNameToFileName(name)));
}

StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
BackingStoreImpl::GetDatabaseNamesAndVersions() {
  // Though the IDB spec does not mandate sorting, the LevelDB backing store has
  // set a precedent of sorting databases by name. To avoid breaking clients
  // that may depend on this, use a map to return the results in sorted order.
  std::map<std::u16string, int64_t> names_and_versions;

  std::set<base::FilePath> already_known_file_names;

  for (const auto& [name, db] : open_connections_) {
    already_known_file_names.insert(DatabaseNameToFileName(name));
    // indexedDB.databases() is meant to return *committed* database state, i.e.
    // should not include in-progress VersionChange updates. This is verified by
    // external/wpt/IndexedDB/get-databases.any.html
    int64_t version = db->GetCommittedVersion();
    if (version == blink::IndexedDBDatabaseMetadata::NO_VERSION) {
      continue;
    }
    names_and_versions.emplace(name, version);
  }

  for (const auto& [name, version] : cached_versions_) {
    already_known_file_names.insert(DatabaseNameToFileName(name));
    if (version == blink::IndexedDBDatabaseMetadata::NO_VERSION) {
      continue;
    }
    names_and_versions.emplace(name, version);
  }

  if (!in_memory()) {
    EnumerateDatabasesInDirectory(directory_, [&](const base::FilePath& path) {
      if (already_known_file_names.contains(path.BaseName())) {
        return;
      }
      std::ignore =
          LOG_RESULT(DatabaseConnection::Open(/*name=*/{}, path, *this,
                                              /*erase_if_zygotic=*/true),
                     "IndexedDB.SQLite.OpenToReadMetadataResult", in_memory())
              .transform([&](std::unique_ptr<DatabaseConnection> connection) {
                const std::u16string& name = connection->metadata().name;
                int64_t version = connection->metadata().version;
                CHECK_NE(version, blink::IndexedDBDatabaseMetadata::NO_VERSION);
                names_and_versions.emplace(name, version);
                cached_versions_.emplace(name, version);
                // Though not really force closing, skip "optional" cleanup
                // steps since we're actively serving a frontend request.
                std::move(*connection)
                    .GetCleanupTask(/*force_closing=*/true)
                    .Run();
              });
    });
  }

  std::vector<blink::mojom::IDBNameAndVersionPtr> result;
  result.reserve(names_and_versions.size());
  for (const auto& [name, version] : names_and_versions) {
    result.emplace_back(blink::mojom::IDBNameAndVersion::New(name, version));
  }
  return result;
}

StatusOr<std::unique_ptr<BackingStore::Database>>
BackingStoreImpl::CreateOrOpenDatabase(const std::u16string& name) {
  if (auto it = open_connections_.find(name); it != open_connections_.end()) {
    return it->second->CreateDatabaseWrapper();
  }
  base::FilePath db_path =
      in_memory() ? base::FilePath()
                  : directory_.Append(DatabaseNameToFileName(name));
  return DatabaseConnection::Open(name, std::move(db_path), *this)
      .transform([&](std::unique_ptr<DatabaseConnection> connection) {
        std::unique_ptr<BackingStoreDatabaseImpl> database =
            connection->CreateDatabaseWrapper();
        open_connections_[name] = std::move(connection);
        cached_versions_.erase(name);
        return database;
      });
}

uintptr_t BackingStoreImpl::GetIdentifierForMemoryDump() {
  NOTIMPLEMENTED();
  return 0;
}

void BackingStoreImpl::FlushForTesting() {
  if (cleanup_task_runner_) {
    base::RunLoop run_loop;
    cleanup_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
}

void BackingStoreImpl::DestroyConnection(const std::u16string& name,
                                         std::vector<PartitionedLock> locks) {
  std::unique_ptr<DatabaseConnection> connection =
      std::move(open_connections_.extract(name).mapped());
  base::OnceClosure cleanup_task =
      std::move(*connection).GetCleanupTask(is_force_closing_);

  if (is_force_closing_) {
    // Run the cleanup task synchronously.
    std::move(cleanup_task).Run();
    return;
  }

  if (locks.empty()) {
    // Since the connection self-destructs only when there are no active
    // requests, locks should be granted synchronously.
    locks = lock_database_.Run(name);
    CHECK(!locks.empty());
  }

  ++cleanups_in_progress_;
  if (!cleanup_task_runner_) {
    // TODO(crbug.com/436880909): consider deduplicating task traits with the
    // bucket task runner.
    cleanup_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }

  cached_versions_[name] = connection->GetCommittedVersion();
  cleanup_task_runner_->PostTaskAndReply(
      FROM_HERE, std::move(cleanup_task),
      base::BindOnce(&BackingStoreImpl::OnCleanupComplete,
                     weak_factory_.GetWeakPtr(), name, std::move(locks)));
}

void BackingStoreImpl::OnCleanupComplete(const std::u16string& name,
                                         std::vector<PartitionedLock> locks) {
  CHECK_GT(cleanups_in_progress_, 0u);
  if (--cleanups_in_progress_ == 0) {
    cleanup_task_runner_.reset();
  }

  auto it = cached_versions_.find(name);
  CHECK(it != cached_versions_.end());
  if (it->second == blink::IndexedDBDatabaseMetadata::NO_VERSION) {
    // The database was deleted.
    cached_versions_.erase(it);
  }
}

Status BackingStoreImpl::MigrateFrom(BackingStore& source) {
  CHECK(!in_memory());
  DCHECK(GetDatabaseNamesAndVersions()->empty());

  ASSIGN_OR_RETURN(
      std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions,
      source.GetDatabaseNamesAndVersions());

  // All of the files that need to be relocated, across all databases.
  std::list<std::pair<base::FilePath, base::FilePath>>
      legacy_blob_files_to_move;

  for (const auto& name_and_version : names_and_versions) {
    std::unique_ptr<BackingStore::Database> source_db =
        source.CreateOrOpenDatabase(name_and_version->name).value();
    std::unique_ptr<BackingStore::Database> target_db =
        CreateOrOpenDatabase(name_and_version->name).value();

    auto connection_it = open_connections_.find(name_and_version->name);
    CHECK(connection_it != open_connections_.end());
    DatabaseConnection* target_connection = connection_it->second.get();
    CHECK(target_connection->IsZygotic());

    IDB_RETURN_IF_ERROR(MigrateDatabase(*source_db, *target_db));

    auto& files_to_move = target_connection->legacy_blob_files_to_move();
    if (!files_to_move.empty() &&
        !base::CreateDirectory(files_to_move.front().second.DirName())) {
      return Status::IOError("Unable to create blob directory");
    }
    legacy_blob_files_to_move.insert(legacy_blob_files_to_move.end(),
                                     files_to_move.begin(),
                                     files_to_move.end());
  }

  // Up to this point, any failure will abort the migration. After this point,
  // errors are ignored because renaming the files is destructive on `source`.

  for (const auto& [source_file_path, target_file_path] :
       legacy_blob_files_to_move) {
    // We ignore errors at this step, because
    // a) it's largely too late to gracefully go back
    // b) the most likely error is that the original file is missing or
    //    unreadable, which would mean that `source` is already in a
    //    semi-broken state, and the migrated DB will be no worse off.
    //    Traditionally this has been handled by throwing errors when the blob
    //    is actually read and letting the page delete or overwrite the
    //    record, so we maintain that behavior.
    base::File::Error error = base::File::FILE_OK;
    base::ReplaceFile(source_file_path, target_file_path, &error);
    base::UmaHistogramExactLinear("IndexedDB.SqliteMigration.RenameBlobResult",
                                  -error, -base::File::FILE_ERROR_MAX);
  }

  return Status::OK();
}

}  // namespace content::indexed_db::sqlite

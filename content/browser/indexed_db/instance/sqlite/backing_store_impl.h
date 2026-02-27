// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom-forward.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/common/content_export.h"

namespace content::indexed_db::sqlite {

class DatabaseConnection;

class CONTENT_EXPORT BackingStoreImpl : public BackingStore {
 public:
  // The store itself does not have any footprint on disk except a directory
  // where SQLite DBs will be located, so creation cannot fail. `directory` is
  // assumed to already exist. `lock_database` will be called to acquire locks
  // on the given database for cleanup purposes. It is guaranteed to be called
  // when no other operation on the database is ongoing, and hence locks are
  // expected to be granted synchronously.
  BackingStoreImpl(base::FilePath directory,
                   storage::mojom::BlobStorageContext& blob_storage_context,
                   base::RepeatingCallback<std::vector<PartitionedLock>(
                       const std::u16string& name)> lock_database);
  BackingStoreImpl(const BackingStoreImpl&) = delete;
  BackingStoreImpl& operator=(const BackingStoreImpl&) = delete;
  ~BackingStoreImpl() override;

  // Sums the sizes of all files in `directory` that appear to be SQLite
  // databases and optionally pass through `filter`. Note that free pages in the
  // database do count towards this size, unlike the more real-time estimate
  // provided by `DatabaseConnection::GetSize()`.
  static uint64_t SumSizesOfDatabaseFiles(
      const base::FilePath& directory,
      base::FunctionRef<bool(const base::FilePath&)> filter =
          [](const base::FilePath&) { return true; });

  // Writes all contents from `source` into `this`. It only works from LevelDB
  // to SQLite, as any other combination currently has unimplemented portions.
  //
  // This operation is potentially destructive on `source`, as it will take
  // (move) its standalone blob files.
  //
  // No PartitionedLockManager-level locks are taken on either backing store,
  // and it's up to the caller to ensure there will be no other simultaneous
  // operations.
  Status MigrateFrom(BackingStore& source);

  // BackingStore:
  bool CanOpportunisticallyClose() const override;
  void OnForceClosing() override;
  void SignalWhenDestructionComplete(
      base::WaitableEvent* signal_on_destruction) &&
      override;
  void StartPreCloseTasks(base::OnceClosure on_done) override;
  void StopPreCloseTasks() override;
  void RunIdleTasks() override;
  uint64_t EstimateSize(bool write_in_progress) const override;
  StatusOr<bool> DatabaseExists(std::u16string_view name) override;
  StatusOr<std::vector<blink::mojom::IDBNameAndVersionPtr>>
  GetDatabaseNamesAndVersions() override;
  StatusOr<std::unique_ptr<BackingStore::Database>> CreateOrOpenDatabase(
      const std::u16string& name) override;
  uintptr_t GetIdentifierForMemoryDump() override;
  void FlushForTesting() override;

  // Destroys the `DatabaseConnection` for `name`. If `locks` are provided, they
  // are used for cleanup, else locks are acquired.
  void DestroyConnection(const std::u16string& name,
                         std::vector<PartitionedLock> locks = {});

  storage::mojom::BlobStorageContext& blob_storage_context() {
    return *blob_storage_context_;
  }

 private:
  friend class DatabaseConnectionTest;

  bool in_memory() const { return directory_.empty(); }

  void OnCleanupComplete(const std::u16string& name,
                         std::vector<PartitionedLock> locks);

  // The directory where all databases for this backing store will live. When
  // this is empty, the backing store lives in memory.
  const base::FilePath directory_;

  // This BlobStorageContext is owned by the BucketContext that owns `this`.
  // The BlobStorageContext manages handles to web blobs (both coming from and
  // being vended to the renderer). Despite this object's name, it does not
  // store blobs. Those that are written into IndexedDB are stored in the SQLite
  // DB.
  raw_ref<storage::mojom::BlobStorageContext> blob_storage_context_;

  base::RepeatingCallback<std::vector<PartitionedLock>(
      const std::u16string& name)>
      lock_database_;

  std::unordered_map<std::u16string, std::unique_ptr<DatabaseConnection>>
      open_connections_;

  // Versions of databases that are not currently open. The committed version of
  // the database is cached here when it closes and begins async cleanup. Note
  // that databases with committed version `NO_VERSION` are "zygotic" and hence
  // deleted during cleanup.
  std::unordered_map<std::u16string, int64_t> cached_versions_;

  // The count of async database cleanups in progress.
  unsigned int cleanups_in_progress_ = 0;

  // Used to run database cleanup tasks without impacting ongoing operations on
  // other databases. Set while and only while `cleanups_in_progress_` is not 0.
  scoped_refptr<base::SequencedTaskRunner> cleanup_task_runner_;

  bool is_force_closing_ = false;

  base::WeakPtrFactory<BackingStoreImpl> weak_factory_{this};
};

}  // namespace content::indexed_db::sqlite

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_BACKING_STORE_IMPL_H_

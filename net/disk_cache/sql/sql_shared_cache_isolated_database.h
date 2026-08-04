// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"
#include "components/sqlite_vfs/vfs_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_shared_cache_blob_handle.h"
#include "sql/database.h"
#include "sql/streaming_blob_handle.h"

namespace disk_cache {

class SqlSharedCacheIsolatedDatabaseTest;

// Manages a single SQLite database shard for a specific Network Isolation Key
// (NIK).
//
// To prevent cross-site data leaks, each NIK is assigned an isolated database
// via sqlite_vfs::SandboxedVfs. In the future, a read-only PendingFileSet
// corresponding to this database will be sent to the renderer process,
// allowing direct read access to the cached resources from the renderer.
// This class orchestrates the underlying SQLite database lifecycle, initializes
// the schema and metadata tables, and automatically razes and recreates the
// database upon NIK mismatch or schema version incompatibility.
class NET_EXPORT_PRIVATE SqlSharedCacheIsolatedDatabase {
 public:
  friend class SqlSharedCacheIsolatedDatabaseTest;

  enum class Error {
    kFailedForTesting = 1,
    kFailedToOpenVfsFileSet = 2,
    kFailedToOpenDatabase = 3,
    kFailedToRazeIncompatibleVersionDatabase = 4,
    kFailedToRazeIncompatibleNikDatabase = 5,
    kFailedToStartTransaction = 6,
    kFailedToCreateResourcesTable = 7,
    kFailedToCreateResourcesTableIndex = 8,
    kFailedToInitMetaTable = 9,
    kFailedToSetNikInMetaTable = 10,
    kFailedToCommitTransaction = 11,
    kDatabaseNotOpen = 12,
    kFailedToExecuteStatement = 13,
    kEntryNotFound = 14,
    kFailedToGetBlob = 15,
    kFailedToWriteBlob = 16,
    kFailedToSetReady = 17,
    kBodyTooLarge = 18,
    kInvalidWriteRange = 19,
    kInvalidReadRange = 20,
    kFailedToReadBlob = 21,
    kFailedToShareConnection = 22,
    kIsolatedDatabaseNotAvailable = 23,
    kBodySizeMismatch = 24,
  };

  using ReadResult = SqlPersistentStore::ReadResult;
  using ReadResultOrError = base::expected<ReadResult, Error>;

  SqlSharedCacheIsolatedDatabase(
      std::string nik_string,
      const base::FilePath& directory,
      SqlSharedCacheDbId shared_cache_db_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SqlSharedCacheIsolatedDatabase();

  // Returns a PendingFileSet that represents a read-only connection to the
  // database. This PendingFileSet can be passed to another process (like a
  // sandboxed renderer) to allow it to read from the database directly.
  base::expected<sqlite_vfs::PendingFileSet, Error>
  GetSharedReadOnlyConnection();

  base::expected<void, Error> Init();

  // Inserts a new resource with the serialized headers and total body size.
  // `total_body_size` is restricted by the maximum value of int32_t.
  // The entry is immediately marked as `ready` if `total_body_size` is 0, or if
  // `body` is provided and its size matches `total_body_size`.
  // Otherwise, the entry is not ready and subsequent calls to `WriteBody` are
  // required to append the body data. Finally, a call to `WriteBody` with
  // `set_ready = true` makes it `ready` and accessible via `Read`.
  base::expected<SqlSharedCacheRowId, Error> Insert(
      const CacheEntryKey& entry_key,
      scoped_refptr<net::IOBuffer> headers,
      uint32_t total_body_size,
      scoped_refptr<net::IOBuffer> body);

  // Appends data from `buffer` (at the given `offset`) to the entry's
  // body. If `set_ready` is true, the entry is marked as `ready` and will
  // become accessible to `Read`.
  base::expected<void, Error> WriteBody(const CacheEntryKey& entry_key,
                                        SqlSharedCacheRowId shared_cache_row_id,
                                        int offset,
                                        scoped_refptr<net::IOBuffer> buffer,
                                        bool set_ready);

  // Reads data from the entry's body into `buffer` starting at `offset`. The
  // total body size must be specified in `body_size` to validate that the read
  // range (`offset` + `buffer->size()`) does not exceed `body_size`. The entry
  // must be in the `ready` state, otherwise this operation will fail.
  ReadResultOrError Read(const CacheEntryKey& entry_key,
                         SqlSharedCacheRowId shared_cache_row_id,
                         int body_size,
                         int offset,
                         scoped_refptr<net::IOBuffer> buffer);

  // Returns a handle to the entry's body blob, allowing streaming reads.
  // The entry must be in the `ready` state, otherwise this operation will fail
  // and return an Error.
  base::expected<scoped_refptr<SqlSharedCacheBlobHandle>, Error> GetBlobHandle(
      const CacheEntryKey& entry_key,
      SqlSharedCacheRowId shared_cache_row_id,
      int body_size);

  // Deletes an entry by its row ID. Used for cleaning up partial entries.
  void DeleteEntry(SqlSharedCacheRowId shared_cache_row_id);

  // Deletes entries specified by `shared_cache_row_ids` from the database.
  base::expected<void, Error> DeleteEntries(
      const std::vector<SqlSharedCacheRowId>& shared_cache_row_ids);

  // Releases `db_assets_`.
  void Cleanup();

  enum class OperationForTesting {
    kInit,
    kInsert,
    kWriteBody,
    kRead,
    kDeleteEntry,
    kDeleteEntries,
  };

  using SimFailedCallback =
      base::RepeatingCallback<bool(OperationForTesting op)>;

  void SetSimulateDbFailureCallbackForTesting(SimFailedCallback callback);
  bool HasRowForTesting(SqlSharedCacheRowId shared_cache_row_id);

 private:
  class DatabaseAssets {
   public:
    static std::unique_ptr<DatabaseAssets> MaybeCreate(
        const base::FilePath& directory,
        SqlSharedCacheDbId shared_cache_db_id);

    DatabaseAssets(const base::FilePath& directory,
                   SqlSharedCacheDbId shared_cache_db_id,
                   sqlite_vfs::SqliteVfsFileSet vfs_file_set,
                   base::PassKey<DatabaseAssets>);
    DatabaseAssets(const DatabaseAssets&) = delete;
    DatabaseAssets& operator=(const DatabaseAssets&) = delete;

    ~DatabaseAssets();

    sql::Database& db() { return db_; }
    base::FilePath GetDbVirtualFilePath() const;
    base::expected<sqlite_vfs::PendingFileSet, sqlite_vfs::FileSetError>
    ShareConnection();
    void AbandonAndDeleteFiles();

   private:
    const base::FilePath directory_;
    const SqlSharedCacheDbId shared_cache_db_id_;
    sqlite_vfs::SqliteVfsFileSet vfs_file_set_;
    sqlite_vfs::SqliteSandboxedVfsDelegate::UnregisterRunner unregister_runner_;
    sql::Database db_;
  };

  class SqlSharedCacheBlobHandleImpl : public SqlSharedCacheBlobHandle {
   public:
    SqlSharedCacheBlobHandleImpl(
        base::OnceClosure decrement_closure,
        scoped_refptr<base::SequencedTaskRunner> task_runner);

   private:
    ~SqlSharedCacheBlobHandleImpl() override;

    base::OnceClosure decrement_closure_;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
  };

  class BlobHandleHolder {
   public:
    BlobHandleHolder(sql::StreamingBlobHandle blob_handle,
                     const CacheEntryKey& entry_key,
                     int body_size);
    ~BlobHandleHolder();

    BlobHandleHolder(const BlobHandleHolder&) = delete;
    BlobHandleHolder& operator=(const BlobHandleHolder&) = delete;

    sql::StreamingBlobHandle& blob_handle() { return blob_handle_; }

    void IncrementRefCount() { ++ref_count_; }
    void DecrementRefCount() { --ref_count_; }
    size_t ref_count() const { return ref_count_; }

    const std::string_view resource_url() const {
      return entry_key_.resource_url();
    }
    int body_size() const { return body_size_; }

   private:
    sql::StreamingBlobHandle blob_handle_;
    const CacheEntryKey entry_key_;
    const int body_size_;
    size_t ref_count_ = 1;
  };

  void DecrementBlobHandleRefCount(SqlSharedCacheRowId shared_cache_row_id);

  base::expected<BlobHandleHolder*, Error> GetCachedBlobHandleHolder(
      const CacheEntryKey& entry_key,
      SqlSharedCacheRowId shared_cache_row_id,
      int body_size);

  base::expected<sql::StreamingBlobHandle, Error> GetStreamingBlobHandle(
      const CacheEntryKey& entry_key,
      SqlSharedCacheRowId shared_cache_row_id,
      int body_size);

  base::expected<void, Error> WriteBodyInternal(
      const CacheEntryKey& entry_key,
      SqlSharedCacheRowId shared_cache_row_id,
      int offset,
      scoped_refptr<net::IOBuffer> buffer,
      bool set_ready,
      bool in_transaction);

  bool ShouldSimulateFailure(OperationForTesting op) const;

  std::string nik_string_;
  std::unique_ptr<DatabaseAssets> db_assets_;
  SimFailedCallback simulate_db_failure_callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  absl::flat_hash_map<SqlSharedCacheRowId, std::unique_ptr<BlobHandleHolder>>
      blob_handle_holders_;

  base::WeakPtrFactory<SqlSharedCacheIsolatedDatabase> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_H_

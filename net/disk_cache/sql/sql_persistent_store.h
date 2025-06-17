// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/cache_type.h"
#include "net/base/net_export.h"
#include "net/disk_cache/buildflags.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace disk_cache {

// This class manages the persistence layer for the SQL-based disk cache.
// It handles all database operations, including initialization, schema
// management, and data access. All database I/O is performed asynchronously on
// a provided background task runner.
class NET_EXPORT_PRIVATE SqlPersistentStore {
 public:
  // Represents the error of SqlPersistentStore operation.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SqlDiskCacheStoreError)
  enum class Error {
    kOk = 0,
    kFailedToCreateDirectory = 1,
    kFailedToOpenDatabase = 2,
    kFailedToRazeIncompatibleDatabase = 3,
    kFailedToStartTransaction = 4,
    kFailedToCommitTransaction = 5,
    kFailedToInitializeMetaTable = 6,
    kFailedToInitializeSchema = 7,
    kFailedToSetEntryCountMetadata = 8,
    kFailedToSetTotalSizeMetadata = 9,
    kMaxValue = kFailedToSetTotalSizeMetadata
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SqlDiskCacheStoreError)

  using ErrorCallback = base::OnceCallback<void(Error)>;
  using Int32Callback = base::OnceCallback<void(int32_t)>;
  using Int64Callback = base::OnceCallback<void(int64_t)>;

  // Creates a new instance of the persistent store. The returned object must be
  // initialized by calling `Initialize()`. This function never returns a null
  // pointer.
  static std::unique_ptr<SqlPersistentStore> Create(
      const base::FilePath& path,
      int64_t max_bytes,
      net::CacheType type,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  virtual ~SqlPersistentStore() = default;

  SqlPersistentStore(const SqlPersistentStore&) = delete;
  SqlPersistentStore& operator=(const SqlPersistentStore&) = delete;

  // Initializes the store. `callback` will be invoked upon completion.
  virtual void Initialize(ErrorCallback callback) = 0;

  // The maximum size of an individual cache entry's data stream.
  virtual int64_t MaxFileSize() const = 0;

  // The maximum total size of the cache.
  virtual int64_t MaxSize() const = 0;

  // Asynchronously retrieves the count of entries.
  virtual void GetEntryCount(Int32Callback callback) const = 0;

  // Asynchronously retrieves the total size of all entries.
  virtual void GetSizeOfAllEntries(Int64Callback callback) const = 0;

 protected:
  SqlPersistentStore() = default;
};
}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_

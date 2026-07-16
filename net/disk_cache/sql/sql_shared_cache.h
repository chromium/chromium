// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"
#include "net/disk_cache/sql/sql_tracked_sequence_bound.h"

namespace disk_cache {

class BackendCleanupTracker;
class SqlPersistentStore;
class SqlSharedCacheHandle;

// Represents an isolated SQL disk cache instance shared across requests
// matching a specific NetworkIsolationKey (or string representation).
//
// Managed by `SqlSharedCacheManager`, and reference-counted via
// `SqlSharedCacheHandle`. When all handles (`SqlSharedCacheHandle`) referencing
// this cache are destroyed, `SqlSharedCacheManager` cleans up and deletes this
// cache object.
class NET_EXPORT_PRIVATE SqlSharedCache {
 public:
  SqlSharedCache(
      std::string nik_string,
      SqlPersistentStore& store,
      const base::FilePath& directory,
      base::RepeatingCallback<void(SqlSharedCache&)> on_unreferenced_callback,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner,
      scoped_refptr<BackendCleanupTracker> cleanup_tracker);
  ~SqlSharedCache();

  SqlSharedCache(const SqlSharedCache&) = delete;
  SqlSharedCache& operator=(const SqlSharedCache&) = delete;

  // Asynchronously cleans up resources and notifies `callback` when complete.
  void Cleanup(base::OnceClosure callback);

  // Initializes the underlying isolated database instance for this shared cache
  // associated with `shared_cache_db_id`.
  void InitIsolatedDatabase(SqlSharedCacheDbId shared_cache_db_id,
                            base::OnceCallback<void(bool)> callback);

  // Creates a new reference-counted `SqlSharedCacheHandle` targeting this
  // cache.
  scoped_refptr<SqlSharedCacheHandle> CreateHandle();

  // Returns true if there are any active handles referencing this cache.
  bool IsReferenced() const { return handle_count_ != 0; }

  // Increments/decrements the count of active `SqlSharedCacheHandle` instances.
  // Restricted via `base::PassKey` to `SqlSharedCacheHandle`.
  void IncrementHandleCount(base::PassKey<SqlSharedCacheHandle>);
  void DecrementHandleCount(base::PassKey<SqlSharedCacheHandle>);

  // Returns the string key derived from NetworkIsolationKey identifying this
  // cache.
  const std::string& nik_string() const { return nik_string_; }

  // Returns the database ID associated with this shared cache, if initialized.
  std::optional<SqlSharedCacheDbId> shared_cache_db_id() const {
    return shared_cache_db_id_;
  }

  // Returns the task runner used for DB operations.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner() const {
    return db_task_runner_;
  }

 private:
  const std::string nik_string_;
  const raw_ref<SqlPersistentStore> store_;
  const base::FilePath directory_;

  base::RepeatingCallback<void(SqlSharedCache&)> on_unreferenced_callback_;
  int handle_count_ = 0;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  scoped_refptr<BackendCleanupTracker> cleanup_tracker_;

  std::optional<SqlSharedCacheDbId> shared_cache_db_id_;

  SqlTrackedSequenceBound<SqlSharedCacheIsolatedDatabase> isolated_database_;

  base::WeakPtrFactory<SqlSharedCache> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_H_

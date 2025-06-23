// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "net/base/cache_type.h"
#include "net/base/net_export.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/sql/cache_entry_key.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class GrowableIOBuffer;
}  // namespace net

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
    kFailedToExecute = 10,
    kInvalidData = 11,
    kAlreadyExists = 12,
    kNotFound = 13,
    kMaxValue = kNotFound
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SqlDiskCacheStoreError)

  // Holds information about a specific cache entry.
  struct NET_EXPORT_PRIVATE EntryInfo {
    EntryInfo();
    ~EntryInfo();
    EntryInfo(EntryInfo&&);
    EntryInfo& operator=(EntryInfo&&);

    // A unique identifier for this entry instance, used for safe data access.
    base::UnguessableToken token;
    // The last time this entry was used.
    base::Time last_used;
    // The total size of the entry's body (all data streams).
    int64_t body_end = 0;
    // The entry's header data (stream 0).
    scoped_refptr<net::GrowableIOBuffer> head;
    // True if the entry was opened, false if it was newly created.
    bool opened = false;
  };

  using ErrorCallback = base::OnceCallback<void(Error)>;
  using Int32Callback = base::OnceCallback<void(int32_t)>;
  using Int64Callback = base::OnceCallback<void(int64_t)>;
  using EntryInfoOrError = base::expected<EntryInfo, Error>;
  using EntryInfoOrErrorCallback = base::OnceCallback<void(EntryInfoOrError)>;
  using OptionalEntryInfoOrError =
      base::expected<std::optional<EntryInfo>, Error>;
  using OptionalEntryInfoOrErrorCallback =
      base::OnceCallback<void(OptionalEntryInfoOrError)>;

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

  // Opens an entry with the given `key`. If the entry does not exist, it is
  // created. `callback` is invoked with the entry information on success or
  // an error code on failure.
  virtual void OpenOrCreateEntry(const CacheEntryKey& key,
                                 EntryInfoOrErrorCallback callback) = 0;

  // Opens an existing entry with the given `key`.
  // The `callback` is invoked with the entry's information on success. If the
  // entry does not exist, the `callback` is invoked with a `kNotFound` error.
  virtual void OpenEntry(const CacheEntryKey& key,
                         OptionalEntryInfoOrErrorCallback callback) = 0;

  // Creates a new entry with the given `key`.
  // The `callback` is invoked with the new entry's information on success. If
  // an entry with this key already exists, the callback is invoked with a
  // `kAlreadyExists` error.
  virtual void CreateEntry(const CacheEntryKey& key,
                           EntryInfoOrErrorCallback callback) = 0;

  // Marks an entry for future deletion. When an entry is "doomed", it is
  // immediately removed from the cache's entry count and total size, but its
  // data remains on disk until `DeleteDoomedEntry()` is called. The `token`
  // ensures that only the correct instance of an entry is doomed.
  virtual void DoomEntry(const CacheEntryKey& key,
                         const base::UnguessableToken& token,
                         ErrorCallback callback) = 0;

  // Physically deletes an entry that has been previously marked as doomed. This
  // operation completes the deletion process by removing the entry's data from
  // the database. The `token` ensures that only a specific, doomed instance of
  // the entry is deleted.
  virtual void DeleteDoomedEntry(const CacheEntryKey& key,
                                 const base::UnguessableToken& token,
                                 ErrorCallback callback) = 0;

  // Deletes a "live" entry, i.e., an entry whose `doomed` flag is not set.
  // This is for use for entries which are not open; open entries should have
  // `DoomEntry()` called, and then `DeleteDoomedEntry()` once they're no longer
  // in used.
  virtual void DeleteLiveEntry(const CacheEntryKey& key,
                               ErrorCallback callback) = 0;

  // Deletes all entries from the cache. `callback` is invoked on completion.
  virtual void DeleteAllEntries(ErrorCallback callback) = 0;

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

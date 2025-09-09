// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_

#include <optional>
#include <set>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "net/base/cache_type.h"
#include "net/base/net_export.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/sql/cache_entry_key.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class GrowableIOBuffer;
class IOBuffer;
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
    kInvalidArgument = 14,
    kBodyEndMismatch = 15,
    kMaxValue = kBodyEndMismatch
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

  // Holds information about a specific cache entry, including its `res_id` and
  // `key`. This is used when iterating through entries.
  struct NET_EXPORT_PRIVATE EntryInfoWithIdAndKey {
    EntryInfoWithIdAndKey();
    ~EntryInfoWithIdAndKey();
    EntryInfoWithIdAndKey(EntryInfoWithIdAndKey&&);
    EntryInfoWithIdAndKey& operator=(EntryInfoWithIdAndKey&&);

    EntryInfo info;
    int64_t res_id;
    CacheEntryKey key;
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
  using OptionalEntryInfoWithIdAndKey = std::optional<EntryInfoWithIdAndKey>;
  using OptionalEntryInfoWithIdAndKeyCallback =
      base::OnceCallback<void(OptionalEntryInfoWithIdAndKey)>;
  using IntOrError = base::expected<int, Error>;
  using IntOrErrorCallback = base::OnceCallback<void(IntOrError)>;
  using Int64OrError = base::expected<int64_t, Error>;
  using Int64OrErrorCallback = base::OnceCallback<void(Int64OrError)>;

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

  // Physically deletes all entries that have been marked as doomed, except for
  // those whose tokens are in `excluded_tokens`. This is typically used for
  // background cleanup of doomed entries that are no longer in use. `callback`
  // is invoked upon completion.
  virtual void DeleteDoomedEntries(
      base::flat_set<base::UnguessableToken> excluded_tokens,
      ErrorCallback callback) = 0;

  // Deletes a "live" entry, i.e., an entry whose `doomed` flag is not set.
  // This is for use for entries which are not open; open entries should have
  // `DoomEntry()` called, and then `DeleteDoomedEntry()` once they're no longer
  // in used.
  virtual void DeleteLiveEntry(const CacheEntryKey& key,
                               ErrorCallback callback) = 0;

  // Deletes all entries from the cache. `callback` is invoked on completion.
  virtual void DeleteAllEntries(ErrorCallback callback) = 0;

  // Deletes all "live" (not doomed) entries whose `last_used` time falls
  // within the range [`initial_time`, `end_time`), excluding any entries whose
  // keys are present in `excluded_keys`. `callback` is invoked on completion.
  virtual void DeleteLiveEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      base::flat_set<CacheEntryKey> excluded_keys,
      ErrorCallback callback) = 0;

  // Updates the `last_used` timestamp for the entry with the specified `key`.
  // `callback` is invoked with `kOk` on success, or `kNotFound` if the entry
  // does not exist or is already doomed.
  virtual void UpdateEntryLastUsed(const CacheEntryKey& key,
                                   base::Time last_used,
                                   ErrorCallback callback) = 0;

  // Updates the header data (stream 0) and the `last_used` timestamp for a
  // specific cache entry. The `bytes_usage` for the entry is adjusted based
  // on `header_size_delta`. `callback` is invoked with `kOk` on success,
  // `kNotFound` if the entry (matching `key` and `token`) is not found or is
  // doomed, or `kInvalidData` if internal data consistency checks fail.
  // `buffer` must not be null. `header_size_delta` is the change in the size
  // of the header data.
  virtual void UpdateEntryHeaderAndLastUsed(const CacheEntryKey& key,
                                            const base::UnguessableToken& token,
                                            base::Time last_used,
                                            scoped_refptr<net::IOBuffer> buffer,
                                            int64_t header_size_delta,
                                            ErrorCallback callback) = 0;

  // Writes data to an entry's body. This can be used to write new data,
  // overwrite existing data, or append to the entry.
  // `key` and `token` identify the target entry.
  // `old_body_end` is the expected current size of the body. It is used to
  // determine whether to trim or truncate existing data, and for consistency
  // checks.
  // `offset` is the position within the entry's body to start writing.
  // `buffer` contains the data to be written. This can be null for truncation.
  // `buf_len` is the size of `buffer`.
  // If `truncate` is true, the entry's body will be truncated to the end of
  // this write. Otherwise, the body size will grow if the write extends past
  // the current end.
  // `callback` is invoked upon completion with an error code.
  virtual void WriteEntryData(const CacheEntryKey& key,
                              const base::UnguessableToken& token,
                              int64_t old_body_end,
                              int64_t offset,
                              scoped_refptr<net::IOBuffer> buffer,
                              int buf_len,
                              bool truncate,
                              ErrorCallback callback) = 0;

  // Reads data from an entry's body.
  // `token` identifies the entry to read from.
  // `offset` is the position within the entry's body to start reading.
  // `buffer` is the destination for the read data.
  // `buf_len` is the size of `buffer`.
  // `body_end` is the logical size of the entry's body.
  // If `sparse_reading` is true, the read will stop at the first gap in the
  // stored data. If false, gaps will be filled with zeros.
  // `callback` is invoked with the number of bytes read on success, or an error
  // code on failure.
  virtual void ReadEntryData(const base::UnguessableToken& token,
                             int64_t offset,
                             scoped_refptr<net::IOBuffer> buffer,
                             int buf_len,
                             int64_t body_end,
                             bool sparse_reading,
                             IntOrErrorCallback callback) = 0;

  // Finds the available contiguous range of data for a given entry.
  // `token` identifies the entry.
  // `offset` is the starting position of the range to check.
  // `len` is the length of the range to check.
  // `callback` is invoked with the result. The `RangeResult` will contain the
  // starting offset and length of the first contiguous block of data found
  // within the requested range `[offset, offset + len)`. If no data is found
  // in the requested range, the `available_len` in the result will be 0.
  virtual void GetEntryAvailableRange(const base::UnguessableToken& token,
                                      int64_t offset,
                                      int len,
                                      RangeResultCallback callback) = 0;

  // Calculates the total size of all entries whose `last_used` time falls
  // within the range [`initial_time`, `end_time`). The size includes the key,
  // header, body data, and a static overhead per entry. `callback` is invoked
  // with the total size on success, or an error code on failure.
  virtual void CalculateSizeOfEntriesBetween(base::Time initial_time,
                                             base::Time end_time,
                                             Int64OrErrorCallback callback) = 0;

  // Opens the latest (highest `res_id`) cache entry that has a `res_id` less
  // than `res_id_cursor`. This method is used for iterating through entries
  // in reverse `res_id` order. To fetch all entries, start with
  // `res_id_cursor` set to `std::numeric_limits<int64_t>::max()`. `callback`
  // receives the entry (or `std::nullopt` if no more entries exist).
  virtual void OpenLatestEntryBeforeResId(
      int64_t res_id_cursor,
      OptionalEntryInfoWithIdAndKeyCallback callback) = 0;

  // Checks if cache eviction should be initiated. This is typically called by
  // the backend after an operation that increases the cache size. Returns true
  // if the cache size has exceeded the high watermark and an eviction is not
  // already in progress.
  virtual bool ShouldStartEviction() = 0;

  // Starts the eviction process to reduce the cache size. This method removes
  // the least recently used entries until the total cache size is below the
  // low watermark. Entries with keys in `excluded_keys` (typically active
  // entries) will not be evicted. `callback` is invoked upon completion.
  virtual void StartEviction(base::flat_set<CacheEntryKey> excluded_keys,
                             ErrorCallback callback) = 0;

  // The maximum size of an individual cache entry's data stream.
  virtual int64_t MaxFileSize() const = 0;

  // The maximum total size of the cache.
  virtual int64_t MaxSize() const = 0;

  // Asynchronously retrieves the count of entries.
  virtual void GetEntryCount(Int32Callback callback) const = 0;

  // Asynchronously retrieves the total size of all entries.
  virtual void GetSizeOfAllEntries(Int64Callback callback) const = 0;

  // Enables a strict corruption checking mode for testing purposes.
  virtual void EnableStrictCorruptionCheckForTesting() = 0;

 protected:
  SqlPersistentStore() = default;
};
}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_H_

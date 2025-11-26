// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_
#define NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_

#include <queue>
#include <variant>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/log/net_log_with_source.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace net {
class GrowableIOBuffer;
}  // namespace net

namespace disk_cache {

class SqlBackendImpl;

// Represents a single entry in the SQL-based disk cache.
// This class implements the `disk_cache::Entry` interface and is responsible
// for managing the data and metadata of a cache entry.
class NET_EXPORT_PRIVATE SqlEntryImpl final
    : public Entry,
      public base::RefCounted<SqlEntryImpl> {
 public:
  // For a speculatively created entry, this holds `std::nullopt` initially, and
  // when the entry creation task is complete, it will hold either the `ResId`
  // on success or an `Error` on failure. Otherwise, it just holds a `ResId`.
  using ResIdOrErrorHolder = base::RefCountedData<std::optional<
      std::variant<SqlPersistentStore::ResId, SqlPersistentStore::Error>>>;

  // Constructs a SqlEntryImpl.
  SqlEntryImpl(base::WeakPtr<SqlBackendImpl> backend,
               CacheEntryKey key,
               scoped_refptr<ResIdOrErrorHolder> res_id_or_error,
               base::Time last_used,
               int64_t body_end,
               scoped_refptr<net::GrowableIOBuffer> head);

  // From disk_cache::Entry:
  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  base::Time GetLastUsed() const override;
  int64_t GetDataSize(int index) const override;
  int ReadData(int index,
               int64_t offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override;
  int WriteData(int index,
                int64_t offset,
                IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override;
  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override;
  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override;
  RangeResult GetAvailableRange(int64_t offset,
                                int len,
                                RangeResultCallback callback) override;
  bool CouldBeSparse() const override;
  void CancelSparseIO() override;
  net::Error ReadyForSparseIO(CompletionOnceCallback callback) override;
  void SetEntryInMemoryData(uint8_t data) override;
  void SetLastUsedTimeForTest(base::Time time) override;

  // Returns the cache key of the entry.
  const CacheEntryKey& cache_key() const { return key_; }

  // Returns the holder for the resource ID or an error.
  const scoped_refptr<ResIdOrErrorHolder>& res_id_or_error() const {
    return res_id_or_error_;
  }

  // Marks the entry as doomed. This is called by the backend when an
  // active entry is doomed.
  void MarkAsDoomed();

  bool doomed() const { return doomed_; }

  // Updates the `last_used_` timestamp to the current time.
  void UpdateLastUsed();

 private:
  friend class base::RefCounted<SqlEntryImpl>;
  ~SqlEntryImpl() override;

  // Internal implementation for writing data to stream 1. This is called by
  // both `WriteData` and `WriteSparseData`. It forwards the write operation to
  // the backend.
  int WriteDataInternal(int64_t offset,
                        IOBuffer* buf,
                        int buf_len,
                        CompletionOnceCallback callback,
                        bool truncate,
                        bool sparse_write);
  // Internal implementation for reading data from stream 1. This is called by
  // both `ReadData` and `ReadSparseData`. It forwards the read operation to the
  // backend.
  int ReadDataInternal(int64_t offset,
                       IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback,
                       bool sparse_reading);

  base::WeakPtr<SqlBackendImpl> backend_;

  // The key for this cache entry.
  const CacheEntryKey key_;

  // Holds the ResId of the entry or an error if the speculative creation
  // failed.
  const scoped_refptr<ResIdOrErrorHolder> res_id_or_error_;

  // The last time this entry was accessed.
  base::Time last_used_;

  // Flag indicating if `last_used_` has been modified since the entry was
  // opened.
  bool last_used_modified_ = false;

  // The end offset of the entry's body data (stream 1).
  int64_t body_end_;

  // The entry's header data (stream 0).
  scoped_refptr<net::GrowableIOBuffer> head_;

  // Stores the new hints value if it has been modified. This is used to
  // determine if the hints need to be persisted to the database when the entry
  // is destructed.
  std::optional<MemoryEntryDataHints> new_hints_;

  // Stores the original size of the header (stream 0) before it was first
  // modified. `std::nullopt` indicates that the header has not been written to
  // since the entry was opened. This is used in the destructor to determine if
  // the header needs to be persisted to storage.
  std::optional<int64_t> previous_header_size_in_storage_;

  // True if this entry has been marked for deletion.
  bool doomed_ = false;

  base::WeakPtrFactory<SqlEntryImpl> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_

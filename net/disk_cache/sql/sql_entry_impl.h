// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_
#define NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_

#include <queue>
#include <variant>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/sql/cache_entry_key.h"
#include "net/disk_cache/sql/entry_db_handle.h"
#include "net/disk_cache/sql/entry_write_buffer.h"
#include "net/disk_cache/sql/sql_persistent_store.h"
#include "net/disk_cache/sql/sql_write_buffer_memory_monitor.h"
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
  // Constructs a SqlEntryImpl.
  SqlEntryImpl(base::WeakPtr<SqlBackendImpl> backend,
               CacheEntryKey key,
               scoped_refptr<EntryDbHandle> db_handle,
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

  net::IOBuffer* read_cache_buffer_for_test() const {
    return read_cache_buffer_.get();
  }
  int64_t read_cache_buffer_offset_for_test() const {
    return read_cache_buffer_offset_;
  }

  // Returns the cache key of the entry.
  const CacheEntryKey& cache_key() const { return key_; }

  // Returns the holder for the resource ID or an error.
  const scoped_refptr<EntryDbHandle>& db_handle() const { return db_handle_; }

  // Returns the new hints value if it has been modified via
  // SetEntryInMemoryData(). This value might not yet be persisted to the
  // database.
  const std::optional<MemoryEntryDataHints>& new_hints() const {
    return new_hints_;
  }

  bool doomed() const;

  // Updates the `last_used_` timestamp to the current time.
  void UpdateLastUsed();

  // Flushes the write buffer to the backend.
  // When `force_flush_for_creation` is true, this flushes even when the write
  // buffer is empty to create an entry in the DB.
  void FlushBuffer(bool force_flush_for_creation);

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

  // Retrieves the write buffer and returns true if successful. If the buffer
  // is empty, returns false. The `reservation` will be populated with the
  // scoped reservation for the write buffer, which should be kept alive until
  // the buffer is written to the backend.
  bool TakeWriteBuffer(
      EntryWriteBuffer& buffer,
      SqlWriteBufferMemoryMonitor::ScopedReservation& reservation);

  base::WeakPtr<SqlBackendImpl> backend_;
  // The key for this cache entry.
  const CacheEntryKey key_;

  // Holds the ResId of the entry or an error if the speculative creation
  // failed.
  const scoped_refptr<EntryDbHandle> db_handle_;

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

  // Buffers data for stream 1 writes.
  EntryWriteBuffer write_buffer_;

  // A scoped reservation that holds the memory usage of `write_buffer_` in the
  // `SqlWriteBufferMemoryMonitor`.
  SqlWriteBufferMemoryMonitor::ScopedReservation write_buffer_reservation_;

  // A buffer containing data read beyond the requested range.
  scoped_refptr<net::IOBuffer> read_cache_buffer_;
  // The offset within the entry's body where `read_cache_buffer_` starts.
  int64_t read_cache_buffer_offset_ = -1;

  base::WeakPtrFactory<SqlEntryImpl> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_

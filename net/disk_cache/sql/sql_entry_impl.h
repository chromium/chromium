// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_
#define NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_

#include <queue>
#include <variant>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
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
  // Constructs a SqlEntryImpl.
  SqlEntryImpl(base::WeakPtr<SqlBackendImpl> backend,
               CacheEntryKey key,
               const base::UnguessableToken& token,
               base::Time last_used,
               int64_t body_end,
               scoped_refptr<net::GrowableIOBuffer> head);

  // From disk_cache::Entry:
  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  base::Time GetLastUsed() const override;
  int32_t GetDataSize(int index) const override;
  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override;
  int WriteData(int index,
                int offset,
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
  void SetLastUsedTimeForTest(base::Time time) override;

  // Returns the last time the entry was used.
  base::Time LastUsedTime() const { return last_used_; }

  // Returns the cache key of the entry.
  const CacheEntryKey& cache_key() const { return key_; }

  // Returns the unique token for this entry instance.
  const base::UnguessableToken& token() const { return token_; }

  // Marks the entry as doomed. This is called by the backend when an
  // active entry is doomed.
  void MarkAsDoomed();

 private:
  friend class base::RefCounted<SqlEntryImpl>;
  ~SqlEntryImpl() override;

  base::WeakPtr<SqlBackendImpl> backend_;

  // The key for this cache entry.
  const CacheEntryKey key_;

  // A unique token identifying this specific instance of the entry.
  // This is used to ensure that operations (like dooming or deleting)
  // target the correct version of an entry if it's reopened.
  const base::UnguessableToken token_;

  // The last time this entry was accessed.
  base::Time last_used_;

  // The end offset of the entry's body data (stream 1).
  int64_t body_end_;

  // The entry's header data (stream 0).
  scoped_refptr<net::GrowableIOBuffer> head_;

  // True if this entry has been marked for deletion.
  bool doomed_ = false;

  base::WeakPtrFactory<SqlEntryImpl> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_ENTRY_IMPL_H_

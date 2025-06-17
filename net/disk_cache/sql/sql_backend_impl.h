// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_
#define NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/disk_cache/buildflags.h"
#include "net/disk_cache/disk_cache.h"

// This backend is experimental and only available when the build flag is set.
static_assert(BUILDFLAG(ENABLE_DISK_CACHE_SQL_BACKEND));

namespace disk_cache {

// Provides a concrete implementation of the disk cache backend that stores
// entries in a SQLite database. This class is responsible for all operations
// related to creating, opening, dooming, and enumerating cache entries.
//
// NOTE: This is currently a skeleton implementation, and most methods are not
// yet implemented, returning `net::ERR_NOT_IMPLEMENTED`.
class NET_EXPORT_PRIVATE SqlBackendImpl final : public Backend {
 public:
  explicit SqlBackendImpl(net::CacheType cache_type);

  SqlBackendImpl(const SqlBackendImpl&) = delete;
  SqlBackendImpl& operator=(const SqlBackendImpl&) = delete;

  ~SqlBackendImpl() override;

  // Backend interface.
  int64_t MaxFileSize() const override;
  int32_t GetEntryCount(
      net::Int32CompletionOnceCallback callback) const override;
  EntryResult OpenOrCreateEntry(const std::string& key,
                                net::RequestPriority priority,
                                EntryResultCallback callback) override;
  EntryResult OpenEntry(const std::string& key,
                        net::RequestPriority priority,
                        EntryResultCallback callback) override;
  EntryResult CreateEntry(const std::string& key,
                          net::RequestPriority priority,
                          EntryResultCallback callback) override;
  net::Error DoomEntry(const std::string& key,
                       net::RequestPriority priority,
                       CompletionOnceCallback callback) override;
  net::Error DoomAllEntries(CompletionOnceCallback callback) override;
  net::Error DoomEntriesBetween(base::Time initial_time,
                                base::Time end_time,
                                CompletionOnceCallback callback) override;
  net::Error DoomEntriesSince(base::Time initial_time,
                              CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfAllEntries(
      Int64CompletionOnceCallback callback) override;
  int64_t CalculateSizeOfEntriesBetween(
      base::Time initial_time,
      base::Time end_time,
      Int64CompletionOnceCallback callback) override;
  std::unique_ptr<Iterator> CreateIterator() override;
  void GetStats(base::StringPairs* stats) override;
  void OnExternalCacheHit(const std::string& key) override;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_IMPL_H_

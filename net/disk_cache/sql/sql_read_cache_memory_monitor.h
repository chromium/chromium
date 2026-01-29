// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_READ_CACHE_MEMORY_MONITOR_H_
#define NET_DISK_CACHE_SQL_SQL_READ_CACHE_MEMORY_MONITOR_H_

#include <atomic>
#include <cstdint>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace disk_cache {

// A helper class to monitor and limit the total memory usage of read cache
// buffers.
class NET_EXPORT_PRIVATE SqlReadCacheMemoryMonitor
    : public base::RefCountedThreadSafe<SqlReadCacheMemoryMonitor> {
 public:
  explicit SqlReadCacheMemoryMonitor(int64_t max_size);

  // Tries to allocate `size` bytes. Returns true if successful, false
  // otherwise.
  bool Allocate(int size);

  // Releases `size` bytes.
  void ReleaseBytes(int size);

 private:
  friend class base::RefCountedThreadSafe<SqlReadCacheMemoryMonitor>;
  ~SqlReadCacheMemoryMonitor();

  const int64_t max_size_;
  std::atomic<int64_t> current_size_{0};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_READ_CACHE_MEMORY_MONITOR_H_

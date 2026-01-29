// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_read_cache_memory_monitor.h"

#include <atomic>

namespace disk_cache {

SqlReadCacheMemoryMonitor::SqlReadCacheMemoryMonitor(int64_t max_size)
    : max_size_(max_size) {}

SqlReadCacheMemoryMonitor::~SqlReadCacheMemoryMonitor() = default;

bool SqlReadCacheMemoryMonitor::Allocate(int size) {
  int64_t current = current_size_.load(std::memory_order_relaxed);
  while (true) {
    if (current + size > max_size_) {
      return false;
    }
    if (current_size_.compare_exchange_weak(current, current + size,
                                            std::memory_order_relaxed)) {
      return true;
    }
  }
}

void SqlReadCacheMemoryMonitor::ReleaseBytes(int size) {
  current_size_.fetch_sub(size, std::memory_order_relaxed);
}

}  // namespace disk_cache

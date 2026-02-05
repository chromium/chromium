// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_ENTRY_HASHER_H_
#define NET_DISK_CACHE_CACHE_ENTRY_HASHER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "net/base/net_export.h"

namespace disk_cache {

// Provides an interface for hashing cache entry keys. This is used to map a
// string key to a 64-bit hash value used to locate the entry on disk.
class NET_EXPORT CacheEntryHasher {
 public:
  CacheEntryHasher() = default;
  virtual ~CacheEntryHasher() = default;

  // Returns a 64-bit hash of the given key.
  virtual uint64_t GetEntryHashKey(const std::string& key) const = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_ENTRY_HASHER_H_

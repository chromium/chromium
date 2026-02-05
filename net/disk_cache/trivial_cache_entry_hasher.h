// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_TRIVIAL_CACHE_ENTRY_HASHER_H_
#define NET_DISK_CACHE_TRIVIAL_CACHE_ENTRY_HASHER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "net/base/net_export.h"
#include "net/disk_cache/cache_entry_hasher.h"

namespace disk_cache {

// A trivial implementation of CacheEntryHasher that uses the default hashing
// function from simple_util.
class NET_EXPORT TrivialCacheEntryHasher : public CacheEntryHasher {
 public:
  TrivialCacheEntryHasher();

  TrivialCacheEntryHasher(const TrivialCacheEntryHasher&) = delete;
  TrivialCacheEntryHasher& operator=(const TrivialCacheEntryHasher&) = delete;
  ~TrivialCacheEntryHasher() override;

  uint64_t GetEntryHashKey(const std::string& key) const override;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_TRIVIAL_CACHE_ENTRY_HASHER_H_

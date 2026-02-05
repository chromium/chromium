// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/trivial_cache_entry_hasher.h"

#include "net/disk_cache/simple/simple_util.h"

namespace disk_cache {

TrivialCacheEntryHasher::TrivialCacheEntryHasher() = default;
TrivialCacheEntryHasher::~TrivialCacheEntryHasher() = default;

uint64_t TrivialCacheEntryHasher::GetEntryHashKey(
    const std::string& key) const {
  return simple_util::GetEntryHashKey(key);
}

}  // namespace disk_cache

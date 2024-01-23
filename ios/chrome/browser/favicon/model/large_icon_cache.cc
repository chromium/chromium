// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/large_icon_cache.h"

#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "url/gurl.h"

namespace {

const int kMaxCacheSize = 12;

}  // namespace

struct LargeIconCacheEntry {
  LargeIconCacheEntry() {}
  ~LargeIconCacheEntry() {}

  std::unique_ptr<favicon_base::LargeIconResult> result;
};

LargeIconCache::LargeIconCache() : cache_(kMaxCacheSize) {}

LargeIconCache::~LargeIconCache() {}

void LargeIconCache::SetCachedResult(
    const GURL& url,
    const favicon_base::LargeIconResult& result) {
  std::unique_ptr<LargeIconCacheEntry> entry(new LargeIconCacheEntry);
  entry->result = CloneLargeIconResult(result);
  cache_.Put(url, std::move(entry));
}

std::unique_ptr<favicon_base::LargeIconResult> LargeIconCache::GetCachedResult(
    const GURL& url) {
  auto iter = cache_.Get(url);
  if (iter != cache_.end()) {
    DCHECK(iter->second->result);
    return CloneLargeIconResult(*iter->second->result.get());
  }

  return nullptr;
}

std::unique_ptr<favicon_base::LargeIconResult>
LargeIconCache::CloneLargeIconResult(
    const favicon_base::LargeIconResult& large_icon_result) {
  std::unique_ptr<favicon_base::LargeIconResult> clone;
  if (large_icon_result.bitmap.is_valid()) {
    clone.reset(new favicon_base::LargeIconResult(large_icon_result.bitmap));
  } else {
    clone.reset(
        new favicon_base::LargeIconResult(new favicon_base::FallbackIconStyle(
            *large_icon_result.fallback_icon_style.get())));
  }
  return clone;
}

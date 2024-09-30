// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_LARGE_ICON_CACHE_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_LARGE_ICON_CACHE_H_

#include <memory>

#include "base/containers/lru_cache.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
struct LargeIconCacheEntry;

namespace favicon_base {
struct LargeIconResult;
}

// Provides a cache of most recently used LargeIconResult.
//
// Example usage:
//   LargeIconCache* large_icon_cache =
//       IOSChromeLargeIconServiceFactory::GetForProfile(profile);
//   std::unique_ptr<favicon_base::LargeIconResult> icon =
//       large_icon_cache->GetCachedResult(...);
//
class LargeIconCache : public KeyedService {
 public:
  LargeIconCache();

  LargeIconCache(const LargeIconCache&) = delete;
  LargeIconCache& operator=(const LargeIconCache&) = delete;

  ~LargeIconCache() override;

  // `LargeIconService` does everything on callbacks, and iOS needs to load the
  // icons immediately on page load. This caches the LargeIconResult so we can
  // immediately load.
  void SetCachedResult(const GURL& url, const favicon_base::LargeIconResult&);

  // Returns a cached LargeIconResult.
  std::unique_ptr<favicon_base::LargeIconResult> GetCachedResult(
      const GURL& url);

 private:
  // Clones a LargeIconResult.
  std::unique_ptr<favicon_base::LargeIconResult> CloneLargeIconResult(
      const favicon_base::LargeIconResult& large_icon_result);

  base::LRUCache<GURL, std::unique_ptr<LargeIconCacheEntry>> cache_;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_LARGE_ICON_CACHE_H_

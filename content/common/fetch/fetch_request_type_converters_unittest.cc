// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fetch/fetch_request_type_converters.h"

#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::FetchCacheMode;

namespace content {

TEST(FetchRequestTypeConvertersTest, CacheModeTest) {
  EXPECT_EQ(FetchCacheMode::kDefault, GetFetchCacheModeFromLoadFlagsForTest(0));
  EXPECT_EQ(FetchCacheMode::kNoStore,
            GetFetchCacheModeFromLoadFlagsForTest(net::LOAD_DISABLE_CACHE));
  EXPECT_EQ(FetchCacheMode::kValidateCache,
            GetFetchCacheModeFromLoadFlagsForTest(net::LOAD_VALIDATE_CACHE));
  EXPECT_EQ(FetchCacheMode::kBypassCache,
            GetFetchCacheModeFromLoadFlagsForTest(net::LOAD_BYPASS_CACHE));
  EXPECT_EQ(FetchCacheMode::kForceCache, GetFetchCacheModeFromLoadFlagsForTest(
                                             net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(FetchCacheMode::kOnlyIfCached,
            GetFetchCacheModeFromLoadFlagsForTest(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(FetchCacheMode::kUnspecifiedOnlyIfCachedStrict,
            GetFetchCacheModeFromLoadFlagsForTest(net::LOAD_ONLY_FROM_CACHE));
  EXPECT_EQ(FetchCacheMode::kUnspecifiedForceCacheMiss,
            GetFetchCacheModeFromLoadFlagsForTest(net::LOAD_ONLY_FROM_CACHE |
                                                  net::LOAD_BYPASS_CACHE));
}

}  // namespace content

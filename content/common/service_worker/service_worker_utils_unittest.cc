// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_utils.h"

#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::FetchCacheMode;

namespace content {

TEST(ServiceWorkerFetchRequestTest, CacheModeTest) {
  EXPECT_EQ(FetchCacheMode::kDefault,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(0));
  EXPECT_EQ(
      FetchCacheMode::kNoStore,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_DISABLE_CACHE));
  EXPECT_EQ(
      FetchCacheMode::kValidateCache,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_VALIDATE_CACHE));
  EXPECT_EQ(
      FetchCacheMode::kBypassCache,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_BYPASS_CACHE));
  EXPECT_EQ(FetchCacheMode::kForceCache,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(
                net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(FetchCacheMode::kOnlyIfCached,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(
      FetchCacheMode::kUnspecifiedOnlyIfCachedStrict,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_ONLY_FROM_CACHE));
  EXPECT_EQ(FetchCacheMode::kUnspecifiedForceCacheMiss,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_BYPASS_CACHE));
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::FetchCacheMode;

namespace content {

TEST(ServiceWorkerUtilsTest, AllOriginsMatchAndCanAccessServiceWorkers) {
  std::vector<GURL> https_same_origin = {GURL("https://example.com/1"),
                                         GURL("https://example.com/2"),
                                         GURL("https://example.com/3")};
  EXPECT_TRUE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      https_same_origin));

  std::vector<GURL> http_same_origin = {GURL("http://example.com/1"),
                                        GURL("http://example.com/2")};
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      http_same_origin));

  std::vector<GURL> localhost_same_origin = {GURL("http://localhost/1"),
                                             GURL("http://localhost/2")};
  EXPECT_TRUE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      localhost_same_origin));

  std::vector<GURL> filesystem_same_origin = {
      GURL("https://example.com/1"), GURL("https://example.com/2"),
      GURL("filesystem:https://example.com/3")};
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      filesystem_same_origin));

  std::vector<GURL> https_cross_origin = {GURL("https://example.com/1"),
                                          GURL("https://example.org/2"),
                                          GURL("https://example.com/3")};
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      https_cross_origin));

  // Cross-origin access is permitted with --disable-web-security.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisableWebSecurity);
  EXPECT_TRUE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      https_cross_origin));

  // Disallowed schemes are not permitted even with --disable-web-security.
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      filesystem_same_origin));
}

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

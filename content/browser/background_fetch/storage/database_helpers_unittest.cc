// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/database_helpers.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace background_fetch {
namespace {

constexpr char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";

bool CacheUrlRoundTrip(const std::string& url) {
  GURL gurl(url);
  GURL round_trip_url = RemoveUniqueParamFromCacheURL(
      MakeCacheUrlUnique(gurl, kExampleUniqueId, 0), kExampleUniqueId);
  return round_trip_url == gurl;
}

TEST(BackgroundFetchDatabaseHelpers, CacheUrlRoundTrip) {
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com?a=b"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com?a=b&c=d"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path/"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path1/path2"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path?a=b&c=d"));
  EXPECT_TRUE(CacheUrlRoundTrip("https://example.com/path/?a=b&c=d"));
}

}  // namespace
}  // namespace background_fetch
}  // namespace content

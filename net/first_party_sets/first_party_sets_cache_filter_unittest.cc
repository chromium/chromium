// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_cache_filter.h"

#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

TEST(FirstPartySetsCacheFilterTest, GetMatchInfo_EmptyFilter) {
  EXPECT_EQ(FirstPartySetsCacheFilter().GetMatchInfo(
                SchemefulSite(GURL("https://example.test"))),
            FirstPartySetsCacheFilter::MatchInfo());
}

TEST(FirstPartySetsCacheFilterTest, GetMatchInfo_NotMatch) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite foo(GURL("https://foo.test"));
  const int64_t kBrowserRunId = 3;

  FirstPartySetsCacheFilter cache_filter(
      /*filter_=*/{{example, 2}}, kBrowserRunId);
  FirstPartySetsCacheFilter::MatchInfo match_info;
  match_info.browser_run_id = kBrowserRunId;
  EXPECT_EQ(cache_filter.GetMatchInfo(foo), match_info);
}

TEST(FirstPartySetsCacheFilterTest, GetMatchInfo_Match) {
  SchemefulSite example(GURL("https://example.test"));
  const int64_t kBrowserRunId = 3;

  FirstPartySetsCacheFilter cache_filter(
      /*filter_=*/{{example, 2}}, kBrowserRunId);
  FirstPartySetsCacheFilter::MatchInfo match_info;
  match_info.clear_at_run_id = 2;
  match_info.browser_run_id = kBrowserRunId;
  EXPECT_EQ(cache_filter.GetMatchInfo(example), match_info);
}

}  // namespace net

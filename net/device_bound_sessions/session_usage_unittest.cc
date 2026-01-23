// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_usage.h"

#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

TEST(SessionUsageTest, GetMaxUsage) {
  base::flat_map<SessionKey, SessionUsage> usage_map;

  // Empty map returns kNoSiteMatchNotInScope.
  EXPECT_EQ(GetMaxUsage(usage_map), SessionUsage::kNoSiteMatchNotInScope);

  auto key1 =
      SessionKey{SchemefulSite(GURL("https://a.com")), SessionKey::Id("1")};
  auto key2 =
      SessionKey{SchemefulSite(GURL("https://b.com")), SessionKey::Id("2")};
  auto key3 =
      SessionKey{SchemefulSite(GURL("https://c.com")), SessionKey::Id("3")};

  // Max usage of one entry is that entry.
  usage_map[key1] = SessionUsage::kInScopeProactiveRefreshAttempted;
  EXPECT_EQ(GetMaxUsage(usage_map),
            SessionUsage::kInScopeProactiveRefreshAttempted);

  // Adding a lower value should not change the max.
  usage_map[key2] = SessionUsage::kInScopeRefreshNotYetNeeded;
  EXPECT_EQ(GetMaxUsage(usage_map),
            SessionUsage::kInScopeProactiveRefreshAttempted);

  // Adding a higher value should change the max.
  usage_map[key3] = SessionUsage::kDeferred;
  EXPECT_EQ(GetMaxUsage(usage_map), SessionUsage::kDeferred);
}

}  // namespace net::device_bound_sessions

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "base/time/time.h"
#import "testing/platform_test.h"

using tabs_closure_util::GetTabsToClose;
using tabs_closure_util::WebStateIDToTime;

using TabsClosureUtilTest = PlatformTest;

// Tests `GetTabsToClose` with several time ranges.
TEST_F(TabsClosureUtilTest, GetCountOfTabsToClose) {
  base::Time now = base::Time::Now();  // Current time for reference

  const std::pair<web::WebStateID, base::Time> tab0 = {
      web::WebStateID::NewUnique(),
      now - base::Hours(1)};  // Tab 0: Active 1 hour ago.
  const std::pair<web::WebStateID, base::Time> tab1 = {
      web::WebStateID::NewUnique(),
      now - base::Hours(3)};  // Tab 1: Active 3 hours ago.
  const std::pair<web::WebStateID, base::Time> tab2 = {
      web::WebStateID::NewUnique(),
      now - base::Minutes(15)};  // Tab 2: Active 15 minutes ago.
  const std::pair<web::WebStateID, base::Time> tab3 = {
      web::WebStateID::NewUnique(),
      now - base::Days(2)};  // Tab 3: Active 2 days ago.
  const std::pair<web::WebStateID, base::Time> tab4 = {
      web::WebStateID::NewUnique(),
      now + base::Hours(1)};  // Tab 4: Active in the future.

  const WebStateIDToTime tabs = {{tab0}, {tab1}, {tab2}, {tab3}, {tab4}};

  EXPECT_EQ(GetTabsToClose(tabs, now - base::Hours(4), now - base::Hours(2)),
            WebStateIDToTime({{tab1}}));
  EXPECT_EQ(GetTabsToClose(tabs, now - base::Hours(3), now),
            WebStateIDToTime({{tab0}, {tab1}, {tab2}}));
  EXPECT_EQ(GetTabsToClose(tabs, now - base::Days(3), now),
            WebStateIDToTime({{tab0}, {tab1}, {tab2}, {tab3}}));
  EXPECT_EQ(GetTabsToClose(tabs, now, now + base::Hours(2)),
            WebStateIDToTime({{tab4}}));
  EXPECT_EQ(GetTabsToClose({}, now, now + base::Hours(2)),
            WebStateIDToTime({}));
}

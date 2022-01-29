// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"

#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class SnapshotBrowserAgentTest : public PlatformTest {
 public:
  SnapshotBrowserAgentTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(SnapshotBrowserAgentTest, SnapshotCacheCreatedAfterSettingSessionID) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent* agent =
      SnapshotBrowserAgent::FromBrowser(browser_.get());
  EXPECT_NE(nullptr, agent);
  EXPECT_EQ(nil, agent->snapshot_cache());
  agent->SetSessionID([[NSUUID UUID] UUIDString]);
  EXPECT_NE(nil, agent->snapshot_cache());
}

}  // anonymous namespace

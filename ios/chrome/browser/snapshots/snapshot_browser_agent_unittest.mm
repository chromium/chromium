// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "testing/platform_test.h"

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

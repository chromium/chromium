// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

namespace {

// Name of the directory where snapshots are saved.
const char kIdentifier[] = "Identifier";

}  // anonymous namespace

class SnapshotBrowserAgentTest : public PlatformTest {
 public:
  SnapshotBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(SnapshotBrowserAgentTest, SnapshotStorageCreatedAfterSettingSessionID) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent* agent =
      SnapshotBrowserAgent::FromBrowser(browser_.get());
  EXPECT_NE(nullptr, agent);
  EXPECT_EQ(nil, agent->snapshot_storage());
  agent->SetSessionID(kIdentifier);
  EXPECT_NE(nil, agent->snapshot_storage());
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/device_sharing/device_sharing_manager.h"

#include <memory>

#include "base/mac/foundation_util.h"
#import "components/handoff/handoff_manager.h"
#include "components/handoff/pref_names_ios.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestDeviceSharingManager : DeviceSharingManager
+ (HandoffManager*)createHandoffManager;
@end

@implementation TestDeviceSharingManager
+ (HandoffManager*)createHandoffManager {
  return [OCMockObject niceMockForClass:[HandoffManager class]];
}
@end

namespace {

class DeviceSharingManagerTest : public PlatformTest {
 protected:
  DeviceSharingManagerTest()
      : PlatformTest(),
        kTestURL1("http://test_sharing_1.html"),
        kTestURL2("http://test_sharing_2.html") {}

  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder mainBrowserStateBuilder;
    chrome_browser_state_ = mainBrowserStateBuilder.Build();
    sharing_manager_ = [[TestDeviceSharingManager alloc] init];
  }

  void TearDown() override {
    [sharing_manager_ updateBrowserState:NULL];
    sharing_manager_ = nil;
  }

  const GURL kTestURL1;
  const GURL kTestURL2;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  DeviceSharingManager* sharing_manager_;
};

TEST_F(DeviceSharingManagerTest, NoMainBrowserState) {
  EXPECT_FALSE([sharing_manager_ handoffManager]);
  // Updating the active URL should be a no-op.
  [sharing_manager_ updateActiveURL:GURL("http://test")];
  EXPECT_FALSE([sharing_manager_ handoffManager]);
}

TEST_F(DeviceSharingManagerTest, ShareOneUrl) {
  [sharing_manager_ updateBrowserState:chrome_browser_state_.get()];
  EXPECT_TRUE([sharing_manager_ handoffManager]);
  OCMockObject* mock_handoff_manager =
      (OCMockObject*)[sharing_manager_ handoffManager];

  [[mock_handoff_manager expect] updateActiveURL:kTestURL1];
  [sharing_manager_ updateActiveURL:kTestURL1];
  EXPECT_OCMOCK_VERIFY(mock_handoff_manager);
}

TEST_F(DeviceSharingManagerTest, ShareTwoUrls) {
  [sharing_manager_ updateBrowserState:chrome_browser_state_.get()];
  EXPECT_TRUE([sharing_manager_ handoffManager]);
  OCMockObject* mock_handoff_manager =
      (OCMockObject*)[sharing_manager_ handoffManager];

  [[mock_handoff_manager expect] updateActiveURL:kTestURL1];
  [[mock_handoff_manager expect] updateActiveURL:kTestURL2];
  [sharing_manager_ updateActiveURL:kTestURL1];
  [sharing_manager_ updateActiveURL:kTestURL2];
  EXPECT_OCMOCK_VERIFY(mock_handoff_manager);
}

TEST_F(DeviceSharingManagerTest, ResetMainBrowserStateAfterShare) {
  [sharing_manager_ updateBrowserState:chrome_browser_state_.get()];
  EXPECT_TRUE([sharing_manager_ handoffManager]);
  OCMockObject* mock_handoff_manager =
      (OCMockObject*)[sharing_manager_ handoffManager];

  [[mock_handoff_manager expect] updateActiveURL:kTestURL1];
  [sharing_manager_ updateActiveURL:kTestURL1];
  EXPECT_OCMOCK_VERIFY(mock_handoff_manager);

  [sharing_manager_ updateBrowserState:NULL];
  EXPECT_FALSE([sharing_manager_ handoffManager]);
}

TEST_F(DeviceSharingManagerTest, DisableHandoffViaPrefs) {
  [sharing_manager_ updateBrowserState:chrome_browser_state_.get()];
  EXPECT_TRUE([sharing_manager_ handoffManager]);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      chrome_browser_state_->GetTestingPrefService();
  prefs->SetBoolean(prefs::kIosHandoffToOtherDevices, false);
  EXPECT_FALSE([sharing_manager_ handoffManager]);
}

}  // namespace

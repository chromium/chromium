// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"

#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace tab_groups {

class TabGroupSyncServiceFactoryTest : public PlatformTest {
 public:
  TabGroupSyncServiceFactoryTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests the creation of the service in regular.
TEST_F(TabGroupSyncServiceFactoryTest, ServiceCreatedInRegularProfile) {
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForBrowserState(browser_state_.get());
  EXPECT_TRUE(service);
}

// Tests that the factory is returning a nil pointer for incognito.
TEST_F(TabGroupSyncServiceFactoryTest, ServiceNotCreatedInIncognito) {
  TabGroupSyncService* service = TabGroupSyncServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState());
  EXPECT_FALSE(service);
}

}  // namespace tab_groups

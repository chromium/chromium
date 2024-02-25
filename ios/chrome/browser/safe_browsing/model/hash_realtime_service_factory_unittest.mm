// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/hash_realtime_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class HashRealTimeServiceFactoryTest : public PlatformTest {
 protected:
  HashRealTimeServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Checks that HashRealTimeServiceFactory returns null for an
// off-the-record browser state.
TEST_F(HashRealTimeServiceFactoryTest, DisabledForIncognitoMode) {
  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(HashRealTimeServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));
}

// Checks that HashRealTimeServiceFactory returns a non-null instance for a
// regular browser state.
TEST_F(HashRealTimeServiceFactoryTest, EnabledForRegularMode) {
  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(
      HashRealTimeServiceFactory::GetForBrowserState(browser_state_.get()));
}

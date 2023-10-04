// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"

#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Class used to test TailoredSecurityServiceFactory initialization.
class TailoredSecurityServiceFactoryTest : public PlatformTest {
 protected:
  TailoredSecurityServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Checks that TailoredSecurityServiceFactory returns a null for an
// off-the-record browser state, but returns a non-null instance for a regular
// browser state.
TEST_F(TailoredSecurityServiceFactoryTest, OffTheRecordReturnsNull) {
  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(TailoredSecurityServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(
      TailoredSecurityServiceFactory::GetForBrowserState(browser_state_.get()));
}

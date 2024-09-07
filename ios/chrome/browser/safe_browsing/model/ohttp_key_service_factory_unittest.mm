// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class OhttpKeyServiceFactoryTest : public PlatformTest {
 protected:
  OhttpKeyServiceFactoryTest() = default;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<ChromeBrowserState> browser_state_;

 private:
  OhttpKeyServiceAllowerForTesting allow_ohttp_key_service_;
};

// Checks that OhttpKeyServiceFactory returns a null for an
// off-the-record browser state, but returns a non-null instance for a regular
// browser state.
TEST_F(OhttpKeyServiceFactoryTest, GetForBrowserState) {
  browser_state_ = TestChromeBrowserState::Builder().Build();

  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(OhttpKeyServiceFactory::GetForBrowserState(browser_state_.get()));
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/real_time_url_lookup_service_factory.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class RealTimeUrlLookupServiceFactoryTest : public PlatformTest {
 protected:
  RealTimeUrlLookupServiceFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Checks that RealTimeUrlLookupServiceFactory returns a null for an
// off-the-record browser state, but returns a non-null instance for a regular
// browser state.
TEST_F(RealTimeUrlLookupServiceFactoryTest, OffTheRecordReturnsNull) {
  // The factory should return null for an off-the-record browser state.
  EXPECT_FALSE(RealTimeUrlLookupServiceFactory::GetForBrowserState(
      browser_state_->GetOffTheRecordChromeBrowserState()));

  // There should be a non-null instance for a regular browser state.
  EXPECT_TRUE(RealTimeUrlLookupServiceFactory::GetForBrowserState(
      browser_state_.get()));
}

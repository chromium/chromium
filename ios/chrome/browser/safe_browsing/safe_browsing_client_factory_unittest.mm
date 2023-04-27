// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_client_factory.h"

#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SafeBrowsingClientFactoryTest : public PlatformTest {
 protected:
  SafeBrowsingClientFactoryTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()) {}

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
};

// Checks that different instances are returned for recording and off the record
// browser states.
TEST_F(SafeBrowsingClientFactoryTest, DifferentClientInstances) {
  SafeBrowsingClient* recording_client =
      SafeBrowsingClientFactory::GetForBrowserState(browser_state_.get());
  SafeBrowsingClient* off_the_record_client =
      SafeBrowsingClientFactory::GetForBrowserState(
          browser_state_->GetOffTheRecordChromeBrowserState());
  EXPECT_TRUE(recording_client);
  EXPECT_TRUE(off_the_record_client);
  EXPECT_NE(recording_client, off_the_record_client);
}

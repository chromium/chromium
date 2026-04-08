// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_learn_more/coordinator/age_mismatch_learn_more_coordinator.h"

#import <WebKit/WebKit.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AgeMismatchLearnMoreTest : public PlatformTest {
 public:
  AgeMismatchLearnMoreTest()
      : PlatformTest(),
        profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())) {}

 private:
  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

@interface AgeMismatchLearnMoreCoordinatorTests
    : AgeMismatchLearnMoreCoordinator
@property(readonly, nonatomic) NSUInteger numberOfFail;
@end

@implementation AgeMismatchLearnMoreCoordinatorTests

- (void)failedToLoad {
  _numberOfFail++;
}

@end

// Test that a navigation error loading an element of the page calls
// failedToLoad.
TEST_F(AgeMismatchLearnMoreTest, TestFailureInNavigationCallsFailedToLoad) {
  AgeMismatchLearnMoreCoordinatorTests* coordinator =
      [[AgeMismatchLearnMoreCoordinatorTests alloc]
          initWithBaseViewController:nil
                             browser:browser_.get()];

  WKWebView* webView = OCMStrictClassMock([WKWebView class]);
  NSError* error = [NSError errorWithDomain:@"Hello" code:42 userInfo:nil];

  [(id<WKNavigationDelegate>)coordinator webView:webView
                               didFailNavigation:nil
                                       withError:error];

  EXPECT_EQ(coordinator.numberOfFail, 1u);
  EXPECT_OCMOCK_VERIFY((id)webView);
}

// Tests that a navigation error on the loaded page itself calls failedToLoad.
TEST_F(AgeMismatchLearnMoreTest, TestProvisionalFailureCallsFailedToLoad) {
  AgeMismatchLearnMoreCoordinatorTests* coordinator =
      [[AgeMismatchLearnMoreCoordinatorTests alloc]
          initWithBaseViewController:nil
                             browser:browser_.get()];

  WKWebView* webView = OCMStrictClassMock([WKWebView class]);
  NSError* error = [NSError errorWithDomain:@"Hello" code:42 userInfo:nil];

  [(id<WKNavigationDelegate>)coordinator webView:webView
                    didFailProvisionalNavigation:nil
                                       withError:error];

  EXPECT_EQ(coordinator.numberOfFail, 1u);
  EXPECT_OCMOCK_VERIFY((id)webView);
}

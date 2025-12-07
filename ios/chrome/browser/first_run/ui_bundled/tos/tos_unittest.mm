// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>
#import <gtest/gtest.h>

#import "base/test/task_environment.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class TOSTest : public PlatformTest {
 public:
  TOSTest()
      : PlatformTest(),
        profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())) {}

 private:
  base::test::TaskEnvironment task_environment_;

 protected:
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

@interface TOSCoordinatorTests : TOSCoordinator <WKNavigationDelegate>
@property(readonly, nonatomic) NSUInteger numberOfFail;

@end

@implementation TOSCoordinatorTests

- (void)failedToLoad {
  _numberOfFail++;
}

@end

// Test that a navigation error loading an element of the page calls
// failedToLoad
TEST_F(TOSTest, TestFailureIsCalledOnFailingElement) {
  TOSCoordinatorTests* tos_test =
      [[TOSCoordinatorTests alloc] initWithBaseViewController:nil
                                                      browser:browser_.get()];
  WKWebView* webView = OCMStrictClassMock([WKWebView class]);
  NSError* error = [NSError errorWithDomain:@"Hello" code:42 userInfo:nil];
  [tos_test webView:webView didFailNavigation:nil withError:error];
  EXPECT_EQ(tos_test.numberOfFail, 1u);
  EXPECT_OCMOCK_VERIFY((id)webView);
}

// Tests that a navigation error on the loaded page itself calls failedToLoad
TEST_F(TOSTest, TestFailureIsCalledOnFailingPage) {
  TOSCoordinatorTests* tos_test =
      [[TOSCoordinatorTests alloc] initWithBaseViewController:nil
                                                      browser:browser_.get()];
  WKWebView* webView = OCMStrictClassMock([WKWebView class]);
  NSError* error = [NSError errorWithDomain:@"Hello" code:42 userInfo:nil];
  [tos_test webView:webView didFailProvisionalNavigation:nil withError:error];
  EXPECT_EQ(tos_test.numberOfFail, 1u);
  EXPECT_OCMOCK_VERIFY((id)webView);
}

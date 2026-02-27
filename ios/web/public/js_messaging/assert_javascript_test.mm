// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/javascript_test.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

namespace {

static NSString* kAssertTestsApi = @"__gCrWeb.getRegisteredApi('assert_tests')";

}  // namespace

@interface WindowErrorScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property(nonatomic, strong) WKScriptMessage* lastReceivedMessage;
@end

@implementation WindowErrorScriptMessageHandler

- (void)configureForWebView:(WKWebView*)webView {
  [webView.configuration.userContentController
      addScriptMessageHandler:self
                         name:@"WindowErrorResultHandler"];
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  self.lastReceivedMessage = message;
}

@end

// Test fixture for assert.ts.
class AssertJavaScriptTest : public web::JavascriptTest {
 protected:
  AssertJavaScriptTest()
      : message_handler_([[WindowErrorScriptMessageHandler alloc] init]) {}
  ~AssertJavaScriptTest() override {}

  void SetUp() override {
    JavascriptTest::SetUp();
    AddUserScript(@"assert_test_api");

    [message_handler_ configureForWebView:web_view()];

    ASSERT_TRUE(LoadHtml(@"<p>"));
  }

  bool WaitForScriptMessageReceived() {
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^{
          return message_handler().lastReceivedMessage != nil;
        });
  }

  WindowErrorScriptMessageHandler* message_handler() {
    return message_handler_;
  }

 private:
  WindowErrorScriptMessageHandler* message_handler_;
};

// Tests that assert errors reach the native error handler.
TEST_F(AssertJavaScriptTest, Assert) {
  NSString* execute_assert = [NSString
      stringWithFormat:@"%@.getFunction('assert')()", kAssertTestsApi];
  web::test::ExecuteJavaScriptInWebView(web_view(), execute_assert);

  ASSERT_TRUE(WaitForScriptMessageReceived());

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"CrWeb Assertion: Fatal assertion.", body[@"message"]);
}

// Tests that assert type errors reach the native error handler.
TEST_F(AssertJavaScriptTest, AssertType) {
  NSString* execute_assert = [NSString
      stringWithFormat:@"%@.getFunction('assertType')()", kAssertTestsApi];
  web::test::ExecuteJavaScriptInWebView(web_view(), execute_assert);

  ASSERT_TRUE(WaitForScriptMessageReceived());

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"CrWeb Assertion: Value [object HTMLBodyElement] is not of type "
              @"HTMLInputElement",
              body[@"message"]);
}

// Tests that assert type errors with custom messages reach the native error
// handler.
TEST_F(AssertJavaScriptTest, AssertTypeWithCustomMessage) {
  NSString* execute_assert = [NSString
      stringWithFormat:@"%@.getFunction('assertTypeWithCustomMessage')()",
                       kAssertTestsApi];
  web::test::ExecuteJavaScriptInWebView(web_view(), execute_assert);

  ASSERT_TRUE(WaitForScriptMessageReceived());

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(
      @"CrWeb Assertion: Element is not of expected type, HTMLInputElement.",
      body[@"message"]);
}

// Tests that assertNotReached errors reach the native error handler.
TEST_F(AssertJavaScriptTest, AssertNotReached) {
  NSString* execute_assert =
      [NSString stringWithFormat:@"%@.getFunction('assertNotReached')()",
                                 kAssertTestsApi];
  web::test::ExecuteJavaScriptInWebView(web_view(), execute_assert);

  ASSERT_TRUE(WaitForScriptMessageReceived());

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"CrWeb Assertion: Unreachable code hit", body[@"message"]);
}

// Tests that assertNotReached errors with custom messages reach the native
// error handler.
TEST_F(AssertJavaScriptTest, AssertNotReachedWithCustomMessage) {
  NSString* execute_assert = [NSString
      stringWithFormat:@"%@.getFunction('assertNotReachedWithCustomMessage')()",
                       kAssertTestsApi];
  web::test::ExecuteJavaScriptInWebView(web_view(), execute_assert);

  ASSERT_TRUE(WaitForScriptMessageReceived());

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"CrWeb Assertion: This code should never hit.",
              body[@"message"]);
}

// Tests that assertNonNull errors reach the native error handler.
TEST_F(AssertJavaScriptTest, AssertNonNull) {
  NSString* execute_assert = [NSString
      stringWithFormat:@"%@.getFunction('assertNonNull')()", kAssertTestsApi];
  web::test::ExecuteJavaScriptInWebView(web_view(), execute_assert);

  ASSERT_TRUE(WaitForScriptMessageReceived());

  NSDictionary* body = message_handler().lastReceivedMessage.body;
  EXPECT_NSEQ(@"CrWeb Assertion: Object can not be null", body[@"message"]);
}

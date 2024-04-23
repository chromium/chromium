// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>

#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest_mac.h"

namespace {
NSString* const kMessageHandlerCommandName = @"messageHandlerCommand";
}

namespace ios_web_view {

// Tests the script handler feature in CWVWebView.
class WebViewScriptMessageHandlerTest : public WebViewInttestBase {
 public:
  void SetUp() override {
    WebViewInttestBase::SetUp();

    ASSERT_TRUE(test_server_->Start());

    LoadTestPage();
  }

  // Uses GetUrlForPageWithHtmlBody() instead of simply using about:blank
  // because it looks __gCrWeb may not be available on about:blank.
  // TODO(crbug.com/40573199): Analyze why.
  void LoadTestPage() {
    NSURL* url = net::NSURLWithGURL(GetUrlForPageWithHtmlBody(""));
    ASSERT_TRUE(test::LoadUrl(web_view_, url));
    ASSERT_TRUE(test::WaitForWebViewLoadCompletionOrTimeout(web_view_));
  }
};

// Tests that a handler added by -[CWVWebView
// addMessageHandler:forCommand:] is invoked by JavaScript.
TEST_F(WebViewScriptMessageHandlerTest, MessageReceived) {
  __block NSDictionary* last_received_payload = nil;
  [web_view_
      addMessageHandler:^(NSDictionary* payload) {
        last_received_payload = [payload copy];
      }
             forCommand:kMessageHandlerCommandName];

  NSString* script =
      @"let payload = {'key1':'value1', 'key2':42};"
      @"__gCrWeb.cwvMessaging.messageHost('messageHandlerCommand', payload);";
  NSError* error;
  test::EvaluateJavaScript(web_view_, script, &error);
  ASSERT_FALSE(error);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return last_received_payload != nil;
      }));

  NSDictionary* expected_payload = @{@"key1" : @"value1", @"key2" : @42};
  EXPECT_NSEQ(expected_payload, last_received_payload);

  [web_view_ removeMessageHandlerForCommand:kMessageHandlerCommandName];
}

// Tests that added script commands are still valid after state restoration.
TEST_F(WebViewScriptMessageHandlerTest, MessageReceivedAfterStateRestoration) {
  __block NSDictionary* last_received_payload = nil;
  [web_view_
      addMessageHandler:^(NSDictionary* payload) {
        last_received_payload = [payload copy];
      }
             forCommand:kMessageHandlerCommandName];

  CWVWebView* source_web_view = test::CreateWebView();
  test::CopyWebViewState(source_web_view, web_view_);

  LoadTestPage();

  NSString* script =
      @"let payload = {'key1':'value1', 'key2':42};"
      @"__gCrWeb.cwvMessaging.messageHost('messageHandlerCommand', payload);";
  NSError* error;
  test::EvaluateJavaScript(web_view_, script, &error);
  ASSERT_FALSE(error);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return last_received_payload != nil;
      }));

  NSDictionary* expected_payload = @{@"key1" : @"value1", @"key2" : @42};
  EXPECT_NSEQ(expected_payload, last_received_payload);

  [web_view_ removeMessageHandlerForCommand:kMessageHandlerCommandName];
}

// Tests that script commands are not received after unregistering messages and
// that sending messages to command names which were never registered are
// ignored.
TEST_F(WebViewScriptMessageHandlerTest, NonregisteredMessagesIgnored) {
  __block bool received_command1 = false;
  [web_view_
      addMessageHandler:^(NSDictionary* payload) {
        received_command1 = true;
      }
             forCommand:@"command1"];

  __block bool received_command2 = false;
  [web_view_
      addMessageHandler:^(NSDictionary* payload) {
        received_command2 = true;
      }
             forCommand:@"command2"];

  [web_view_ removeMessageHandlerForCommand:@"command1"];

  NSString* script =
      @"let payload = {'key1':'value1', 'key2':42};"
      @"__gCrWeb.cwvMessaging.messageHost('invalidCommand', payload);"
      @"__gCrWeb.cwvMessaging.messageHost('command1', payload);"
      @"__gCrWeb.cwvMessaging.messageHost('command2', payload);";
  NSError* error;
  test::EvaluateJavaScript(web_view_, script, &error);
  ASSERT_FALSE(error);

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return received_command2;
      }));

  EXPECT_FALSE(received_command1);

  [web_view_ removeMessageHandlerForCommand:@"command2"];
}

}  // namespace ios_web_view

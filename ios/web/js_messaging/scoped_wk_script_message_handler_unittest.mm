// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/test/js_test_util_internal.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// The test message handler name.
static NSString* kScriptHandlerName = @"FakeHandlerName";

// Script which sends a post message back to the native message handlers.
// Evaluation will result in true on success, or false if the messageHandler
// was not registered.
static NSString* kPostMessageScriptFormat =
    @"try {"
    @"  window.webkit.messageHandlers['%@'].postMessage(\"10\");"
    @"  true;"
    @"} catch (err) {"
    @"  false;"
    @"}";
}

namespace web {

class ScopedWKScriptMessageHandlerTest : public WebTestWithWebState {
 public:
  ScopedWKScriptMessageHandlerTest() = default;
  ~ScopedWKScriptMessageHandlerTest() override = default;

  // Returns the user content controller associated with `GetBrowserState()`.
  WKUserContentController* GetUserContentController() {
    WKWebViewConfigurationProvider& configuration_provider =
        WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
    return configuration_provider.GetWebViewConfiguration()
        .userContentController;
  }

  NSString* GetPostMessageScript() {
    return [NSString
        stringWithFormat:kPostMessageScriptFormat, kScriptHandlerName];
  }
};

// Tests that the ScopedWKScriptMessageHandler block is called for the main
// content world.
TEST_F(ScopedWKScriptMessageHandlerTest, ScriptMessageReceived) {
  __block bool handler_called = false;
  __block WKScriptMessage* message = nil;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerName,
          base::BindRepeating(^(WKScriptMessage* callback_message) {
            message = callback_message;
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  id result = ExecuteJavaScript(GetPostMessageScript());
  ASSERT_TRUE([result isKindOfClass:[NSNumber class]]);
  ASSERT_TRUE([result boolValue]);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return handler_called;
  }));

  ASSERT_TRUE(message);
  EXPECT_NSEQ(message.name, kScriptHandlerName);
  EXPECT_NSEQ(message.body, @"10");
}

// Tests that the ScopedWKScriptMessageHandler block is not called after
// deconstruction.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageNotReceivedAfterDeconstruction) {
  __block int handler_called_count = 0;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerName,
          base::BindRepeating(^(WKScriptMessage* callback_message) {
            handler_called_count++;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  id result = ExecuteJavaScript(GetPostMessageScript());
  ASSERT_TRUE([result isKindOfClass:[NSNumber class]]);
  ASSERT_TRUE([result boolValue]);

  scoped_handler.reset();
  // JavaScript exception should be thrown if script message handler was
  // removed.
  result = ExecuteJavaScript(GetPostMessageScript());
  ASSERT_FALSE([result boolValue]);

  EXPECT_EQ(1, handler_called_count);
}

// Tests that the ScopedWKScriptMessageHandler block is called for an isolated
// content world.
TEST_F(ScopedWKScriptMessageHandlerTest, ScriptMessageReceivedIsolatedWorld) {
  __block bool handler_called = false;
  __block WKScriptMessage* message = nil;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message) {
            message = callback_message;
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteJavaScript(
      web_view, WKContentWorld.defaultClientWorld, GetPostMessageScript());
  ASSERT_TRUE([result isKindOfClass:[NSNumber class]]);
  ASSERT_TRUE([result boolValue]);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return handler_called;
  }));

  ASSERT_TRUE(message);
  EXPECT_NSEQ(message.name, kScriptHandlerName);
  EXPECT_NSEQ(message.body, @"10");
}

// Tests that a script message handler registered in the page content world
// does not receive messages from an isolated world.
TEST_F(ScopedWKScriptMessageHandlerTest, ScriptMessageCrossWorldPageContent) {
  __block bool handler_called = false;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerName,
          WKContentWorld.pageWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message) {
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteJavaScript(
      web_view, WKContentWorld.defaultClientWorld, GetPostMessageScript());
  // JavaScript exception should be thrown and false value returned if script
  // message handler was not registered.
  ASSERT_FALSE([result boolValue]);
  EXPECT_FALSE(handler_called);
}

// Tests that a script message handler registered on an isolated world does not
// receive messages from the page content world.
TEST_F(ScopedWKScriptMessageHandlerTest, ScriptMessageCrossWorldIsolated) {
  __block bool handler_called = false;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message) {
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteJavaScript(web_view, WKContentWorld.pageWorld,
                                           GetPostMessageScript());
  // JavaScript exception should be thrown and false value returned if script
  // message handler was not registered.
  ASSERT_FALSE([result boolValue]);
  EXPECT_FALSE(handler_called);
}

}  // namespace web

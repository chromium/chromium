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

// The test asynchronous message handler name.
static NSString* kScriptHandlerWithReplyName = @"FakeHandlerWithReplyName";

// Fake result that native reply to JavaScript.
const int kScriptHandlerReplyResult = 42;

// A part of message of the error which will be hit when a messageHandler is
// used before registration. Full error message should be `undefined is not an
// object (evaluating
// 'window.webkit.messageHandlers['FakeHandlerWithReplyName'].postMessage')`.
static NSString* kScriptUndefinedObjectErrorMessage =
    @"undefined is not an object";

// Error message JavaScript will catch when `replyHandler` is deallocated before
// it is called.
static NSString* kScriptMessageNoReplyErrorMessage =
    @"WKWebView API client did not respond to this postMessage";

// Script which sends a post message to the native message handlers which can
// reply to JavaScript asynchronously. Evaluation will result in value of
// `kScriptHandlerReplyResult` on success, or the error message of the
// JavaScript Error caught.
static NSString* kPostMessageWithReplyHandlerScriptFormat =
    @"try {"
    @"  return await window.webkit.messageHandlers['%@'].postMessage(%d);"
    @"} catch (err) {"
    @"  return err.message;"
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

  NSString* GetPostMessageWithReplyHandlerScript() {
    return [NSString stringWithFormat:kPostMessageWithReplyHandlerScriptFormat,
                                      kScriptHandlerWithReplyName,
                                      kScriptHandlerReplyResult];
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

// Tests that the ScopedWKScriptMessageHandler block with reply handler is
// called and reply with correct result for an isolated content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerReceivedIsolatedWorld) {
  __block WKScriptMessage* message = nil;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            message = callback_message;
            auto reply =
                std::make_unique<base::Value>(kScriptHandlerReplyResult);
            reply_handler(reply.get(), /*error_message=*/nil);
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  ASSERT_TRUE([result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ([result intValue], kScriptHandlerReplyResult);

  ASSERT_TRUE(message);
  EXPECT_NSEQ(message.name, kScriptHandlerWithReplyName);

  ASSERT_TRUE([message.body isKindOfClass:[NSNumber class]]);
  EXPECT_EQ([message.body intValue], kScriptHandlerReplyResult);
}

// Tests that the ScopedWKScriptMessageHandler block with reply handler is
// called and reply with correct result for the page content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerReceivedPageWorld) {
  __block WKScriptMessage* message = nil;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.pageWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            message = callback_message;
            auto reply =
                std::make_unique<base::Value>(kScriptHandlerReplyResult);
            reply_handler(reply.get(), /*error_message=*/nil);
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result =
      web::test::ExecuteAsyncJavaScript(web_view, WKContentWorld.pageWorld,
                                        GetPostMessageWithReplyHandlerScript());
  ASSERT_TRUE([result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ([result intValue], kScriptHandlerReplyResult);

  ASSERT_TRUE(message);
  EXPECT_NSEQ(message.name, kScriptHandlerWithReplyName);

  ASSERT_TRUE([message.body isKindOfClass:[NSNumber class]]);
  EXPECT_EQ([message.body intValue], kScriptHandlerReplyResult);
}

// Tests that the ScopedWKScriptMessageHandler block with reply handler is
// called and reply with `undefined` result for the page content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerReceivedAndReplyUndefinedResult) {
  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            reply_handler(/*reply=*/nullptr, /*error_message=*/nil);
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  ASSERT_FALSE(result);
}

// Tests that the ScopedWKScriptMessageHandler block with reply handler is
// called and reply with `none` result for the page content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerReceivedAndReplyNoneResult) {
  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            auto reply = std::make_unique<base::Value>();
            reply_handler(reply.get(), /*error_message=*/nil);
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  ASSERT_TRUE([result isKindOfClass:[NSNull class]]);
}

// Tests that the ScopedWKScriptMessageHandler block with reply handler is
// called and reply with error message when native can't reply a specific result
// for an isolated content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerReceivedAndReplyWithErrorMessage) {
  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            reply_handler(/*reply=*/nullptr, @"FakeReplyErrorMessage");
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  ASSERT_TRUE([result isKindOfClass:[NSString class]]);
  EXPECT_NSEQ(result, @"FakeReplyErrorMessage");
}

// Tests that the ScopedWKScriptMessageHandler block with reply handler is
// called but no reply to received message for an isolated content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerReceivedAndNoReply) {
  __block bool handler_called = false;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  ASSERT_TRUE([result isKindOfClass:[NSString class]]);
  EXPECT_NSEQ(result, kScriptMessageNoReplyErrorMessage);

  EXPECT_TRUE(handler_called);
}

// Tests that the ScopedWKScriptMessageHandler block with reply handler
// is not called after deconstruction.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerNotReceivedAfterDeconstruction) {
  __block int handler_called_count = 0;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            handler_called_count++;
            auto reply =
                std::make_unique<base::Value>(kScriptHandlerReplyResult);
            reply_handler(reply.get(), /*error_message=*/nil);
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  ASSERT_TRUE([result isKindOfClass:[NSNumber class]]);
  EXPECT_EQ([result intValue], kScriptHandlerReplyResult);

  scoped_handler_with_reply.reset();
  // JavaScript exception should be thrown if script message handler was
  // removed.
  result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  // `result` should be `err.message` of catched `err` now.
  ASSERT_TRUE([result isKindOfClass:[NSString class]]);
  // `undefined is not an object` error should be hit as message handler
  // `FakeHandlerWithReplyName` was removed.
  EXPECT_TRUE([result containsString:kScriptUndefinedObjectErrorMessage]);

  EXPECT_EQ(1, handler_called_count);
}

// Tests that a script message handler with reply registered in the page content
// world does not receive messages from an isolated world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerCrossWorldPageContent) {
  __block bool handler_called = false;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.pageWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            reply_handler(/*reply=*/nullptr, /*error_message=*/nil);
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result = web::test::ExecuteAsyncJavaScript(
      web_view, WKContentWorld.defaultClientWorld,
      GetPostMessageWithReplyHandlerScript());
  // `result` should be `err.message` of catched `err` now.
  ASSERT_TRUE([result isKindOfClass:[NSString class]]);
  // `undefined is not an object` error should be hit as message handler
  // `FakeHandlerWithReplyName` is not aded to corresponding content world.
  EXPECT_TRUE([result containsString:kScriptUndefinedObjectErrorMessage]);
  EXPECT_FALSE(handler_called);
}

// Tests that a script message handler with reply registered on an isolated
// world does not receive messages from the page content world.
TEST_F(ScopedWKScriptMessageHandlerTest,
       ScriptMessageWithReplyHandlerCrossWorldIsolated) {
  __block bool handler_called = false;

  std::unique_ptr<ScopedWKScriptMessageHandler> scoped_handler_with_reply =
      std::make_unique<ScopedWKScriptMessageHandler>(
          GetUserContentController(), kScriptHandlerWithReplyName,
          WKContentWorld.defaultClientWorld,
          base::BindRepeating(^(WKScriptMessage* callback_message,
                                ScriptMessageReplyHandler reply_handler) {
            reply_handler(/*reply=*/nullptr, /*error_message=*/nil);
            handler_called = true;
          }));

  ASSERT_TRUE(LoadHtml("<p>"));

  WKWebView* web_view =
      [web::test::GetWebController(web_state()) ensureWebViewCreated];
  id result =
      web::test::ExecuteAsyncJavaScript(web_view, WKContentWorld.pageWorld,
                                        GetPostMessageWithReplyHandlerScript());
  // `result` should be `err.message` of catched `err` now.
  ASSERT_TRUE([result isKindOfClass:[NSString class]]);
  // `undefined is not an object` error should be hit as message handler
  // `FakeHandlerWithReplyName` is not aded to corresponding content world.
  EXPECT_TRUE([result containsString:kScriptUndefinedObjectErrorMessage]);
  EXPECT_FALSE(handler_called);
}
}  // namespace web

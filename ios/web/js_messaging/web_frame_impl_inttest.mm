// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frame_impl.h"

#include "base/bind.h"
#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Returns the first WebFrame found which is not the main frame in the given
// |web_state|. Does not wait and returns null if such a frame is not found.
web::WebFrame* GetChildWebFrameForWebState(web::WebState* web_state) {
  __block web::WebFramesManager* manager = web_state->GetWebFramesManager();
  web::WebFrame* iframe = nullptr;
  for (web::WebFrame* frame : manager->GetAllWebFrames()) {
    if (!frame->IsMainFrame()) {
      iframe = frame;
      break;
    }
  }
  return iframe;
}
}

namespace web {

// Test fixture to test WebFrameImpl with a real JavaScript context.
typedef WebTestWithWebState WebFrameImplIntTest;

// Tests that the expected result is received from executing a JavaScript
// function via |CallJavaScriptFunction| on the main frame.
TEST_F(WebFrameImplIntTest, CallJavaScriptFunctionOnMainFrame) {
  ASSERT_TRUE(LoadHtml("<p>"));

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  ASSERT_TRUE(main_frame);

  NSTimeInterval js_timeout = kWaitForJSCompletionTimeout;

  __block bool called = false;
  std::vector<base::Value> params;
  main_frame->CallJavaScriptFunction(
      "message.getFrameId", params, base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_string());
        EXPECT_EQ(value->GetString(), main_frame->GetFrameId());
        called = true;
      }),
      // Increase feature timeout in order to fail on test specific timeout.
      base::TimeDelta::FromSeconds(2 * js_timeout));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(js_timeout, ^bool {
    return called;
  }));
}

TEST_F(WebFrameImplIntTest, CallJavaScriptFunctionOnIframe) {
  ASSERT_TRUE(LoadHtml("<p><iframe srcdoc='<p>'/>"));

  __block WebFramesManager* manager = web_state()->GetWebFramesManager();
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return manager->GetAllWebFrames().size() == 2;
      }));

  NSTimeInterval js_timeout = kWaitForJSCompletionTimeout;
  WebFrame* iframe = GetChildWebFrameForWebState(web_state());
  ASSERT_TRUE(iframe);

  __block bool called = false;
  std::vector<base::Value> params;
  iframe->CallJavaScriptFunction(
      "message.getFrameId", params, base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_string());
        EXPECT_EQ(value->GetString(), iframe->GetFrameId());
        called = true;
      }),
      // Increase feature timeout in order to fail on test specific timeout.
      base::TimeDelta::FromSeconds(2 * js_timeout));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(js_timeout, ^bool {
    return called;
  }));
}

TEST_F(WebFrameImplIntTest, CallJavaScriptFunctionTimeout) {
  ASSERT_TRUE(LoadHtml("<p>"));

  // Inject a function which will never return in order to test feature timeout.
  ExecuteJavaScript(@"__gCrWeb.testFunctionNeverReturns = function() {"
                     "  while(true) {}"
                     "};");

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  ASSERT_TRUE(main_frame);

  __block bool called = false;
  std::vector<base::Value> params;
  main_frame->CallJavaScriptFunction(
      "testFunctionNeverReturns", params,
      base::BindOnce(^(const base::Value* value) {
        EXPECT_FALSE(value);
        called = true;
      }),
      // A small timeout less than kWaitForJSCompletionTimeout. Since this test
      // case tests the timeout, it will take at least this long to execute.
      // This value should be very small to avoid increasing test suite
      // execution time, but long enough to avoid flake.
      base::TimeDelta::FromMilliseconds(5));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return called;
  }));
}

// Tests that messages routed through CallJavaScriptFunction cannot be replayed.
TEST_F(WebFrameImplIntTest, PreventMessageReplay) {
  ASSERT_TRUE(LoadHtml("<p>"));

  WebFrame* main_frame = web_state()->GetWebFramesManager()->GetMainWebFrame();
  ASSERT_TRUE(main_frame);

  // Inject function into main frame to intercept encrypted message targeted for
  // the iframe.
  ExecuteJavaScript(@"var sensitiveValue = 0;"
                     "__gCrWeb.message.incrementSensitiveValue = function() {"
                     "  sensitiveValue = sensitiveValue + 1;"
                     "  return sensitiveValue;"
                     "};"

                     "var originalRouteMessage = __gCrWeb.message.routeMessage;"
                     "var interceptedMessagePayload = '';"
                     "var interceptedMessageIv = '';"
                     "var interceptedMessageFrameId = '';"

                     "var replayInterceptedMessage = function() {"
                     "  originalRouteMessage("
                     "    interceptedMessagePayload,"
                     "    interceptedMessageIv,"
                     "    interceptedMessageFrameId"
                     "  );"
                     "};"

                     "__gCrWeb.message.routeMessage ="
                     "    function(payload, iv, target_frame_id) {"
                     "  interceptedMessagePayload = payload;"
                     "  interceptedMessageIv = iv;"
                     "  interceptedMessageFrameId = target_frame_id;"
                     "  replayInterceptedMessage();"
                     "};");

  NSTimeInterval js_timeout = kWaitForJSCompletionTimeout;

  EXPECT_EQ(0, [ExecuteJavaScript(@"sensitiveValue") intValue]);

  __block bool called = false;
  std::vector<base::Value> params;
  main_frame->CallJavaScriptFunction(
      "message.incrementSensitiveValue", params,
      base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_double());
        EXPECT_EQ(1, static_cast<int>(value->GetDouble()));
        called = true;
      }),
      // Increase feature timeout in order to fail on test specific timeout.
      base::TimeDelta::FromSeconds(2 * js_timeout));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(js_timeout, ^bool {
    return called;
  }));

  EXPECT_NSEQ(@1, ExecuteJavaScript(@"sensitiveValue"));

  ExecuteJavaScript(@"replayInterceptedMessage()");

  // Value should not increase because replaying message should not re-execute
  // the called function.
  EXPECT_NSEQ(@1, ExecuteJavaScript(@"sensitiveValue"));
}

// Tests that the main WebFrame is passed to the callback when sending a
// JS -> native message.
TEST_F(WebFrameImplIntTest, JavaScriptMessageFromMainFrame) {
  ASSERT_TRUE(LoadHtml("<p>"));
  __block bool command_received = false;
  // The callback doesn't care about any of the parameters not related to
  // frames.
  auto callback = base::BindRepeating(
      ^(const base::DictionaryValue& /* json */, const GURL& /* origin_url */,
        bool /* user_is_interacting */, WebFrame* sender_frame) {
        command_received = true;
        EXPECT_TRUE(sender_frame->IsMainFrame());
        EXPECT_EQ(web_state()->GetWebFramesManager()->GetMainWebFrame(),
                  sender_frame);
      });

  auto subscription =
      web_state()->AddScriptCommandCallback(callback, "senderFrameTestCommand");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 1;
  }));
  ExecuteJavaScript(@"__gCrWeb.message.invokeOnHost({'command':"
                    @"'senderFrameTestCommand.mainframe'});");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return command_received;
  }));
}

// Tests that an iframe WebFrame is passed to the callback when sending a
// JS -> native message.
TEST_F(WebFrameImplIntTest, JavaScriptMessageFromFrame) {
  ASSERT_TRUE(LoadHtml("<p><iframe>"));
  __block bool command_received = false;
  // The callback doesn't care about any of the parameters not related to
  // frames.
  auto callback = base::BindRepeating(
      ^(const base::DictionaryValue& /* json */, const GURL& /* origin_url */,
        bool /* user_is_interacting */, WebFrame* sender_frame) {
        command_received = true;
        EXPECT_FALSE(sender_frame->IsMainFrame());
        EXPECT_EQ(GetChildWebFrameForWebState(web_state()), sender_frame);
      });

  auto subscription =
      web_state()->AddScriptCommandCallback(callback, "senderFrameTestCommand");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return web_state()->GetWebFramesManager()->GetAllWebFrames().size() == 2;
  }));
  ExecuteJavaScript(
      @"window.frames[0].__gCrWeb.message.invokeOnHost({'command':'"
      @"senderFrameTestCommand.iframe'});");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return command_received;
  }));
}
}

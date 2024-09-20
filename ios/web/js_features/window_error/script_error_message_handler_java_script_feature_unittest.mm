// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/script_error_message_handler_java_script_feature.h"

#import <memory>
#import <optional>

#import "base/test/ios/wait_util.h"
#import "ios/web/js_features/window_error/error_event_listener_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

class ScriptErrorMessageHandlerJavaScriptFeatureTest
    : public WebTestWithWebState {
 protected:
  ScriptErrorMessageHandlerJavaScriptFeatureTest()
      : WebTestWithWebState(),
        feature_(base::BindRepeating(
            ^(ScriptErrorMessageHandlerJavaScriptFeature::ErrorDetails
                  error_details) {
              error_details_ = error_details;
            })) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    OverrideJavaScriptFeatures(
        {ErrorEventListenerJavaScriptFeature::GetInstance(),
         web::java_script_features::GetMessageJavaScriptFeature(), &feature_});
  }

  std::optional<ScriptErrorMessageHandlerJavaScriptFeature::ErrorDetails>
  error_details() {
    return error_details_;
  }

 private:
  ScriptErrorMessageHandlerJavaScriptFeature feature_;
  std::optional<ScriptErrorMessageHandlerJavaScriptFeature::ErrorDetails>
      error_details_;
};

// Tests that error details are received for a script error occurring in the
// head of the main frame.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromMainFramePageHead) {
  ASSERT_FALSE(error_details());

  NSString* html = @"<html><head>"
                    "<script>nonexistentFunction();</script>"
                    "</head><body></body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details();
  }));

  EXPECT_EQ(1, error_details()->line_number);
  EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
              error_details()->message);
  EXPECT_EQ("https://chromium.test/", error_details()->url.spec());
  EXPECT_TRUE(error_details()->is_main_frame);
}

// Tests that error details are received for a script error occurring in the
// body of the main frame.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromMainFramePageBody) {
  ASSERT_FALSE(error_details());

  NSString* html = @"<html><body>"
                    "<script>nonexistentFunction();</script>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details();
  }));

  EXPECT_EQ(1, error_details()->line_number);
  EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
              error_details()->message);
  EXPECT_EQ("https://chromium.test/", error_details()->url.spec());
  EXPECT_TRUE(error_details()->is_main_frame);
}

// Tests that error details are received for a script error occurring in the
// head of an iframe.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromIframePageHead) {
  ASSERT_FALSE(error_details());

  NSString* html = @"<html><body>"
                    "<iframe "
                    "srcdoc='<html><head><script>nonexistentFunction();</"
                    "script></head></html>'/>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details();
  }));

  EXPECT_EQ(0, error_details()->line_number);
  EXPECT_NSEQ(@"Script error.", error_details()->message);
  EXPECT_EQ("about:srcdoc", error_details()->url.spec());
  EXPECT_FALSE(error_details()->is_main_frame);
}

// Tests that error details are received for a script error occurring in the
// body of an iframe.
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorFromIframePageBody) {
  ASSERT_FALSE(error_details());
  NSString* html = @"<html><body>"
                    "<iframe "
                    "srcdoc='<html><body><script>nonexistentFunction();</"
                    "script></body></html>'/>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details();
  }));

  EXPECT_EQ(0, error_details()->line_number);
  EXPECT_NSEQ(@"Script error.", error_details()->message);
  EXPECT_EQ("about:srcdoc", error_details()->url.spec());
  EXPECT_FALSE(error_details()->is_main_frame);
}

// Ensures that error details are still retreived after a document is recreated.
// (Since event listeners are removed and need to be reinjected after a set of
// calls to document.open/write/close.)
TEST_F(ScriptErrorMessageHandlerJavaScriptFeatureTest,
       ReceiveErrorAfterDocumentRecreated) {
  ASSERT_FALSE(error_details());
  LoadHtml(@"<html></html>");

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"nonexistentFunction();");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return !!error_details();
  }));

  EXPECT_EQ(1, error_details()->line_number);
  EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
              error_details()->message);
  EXPECT_EQ("https://chromium.test/", error_details()->url.spec());
  EXPECT_TRUE(error_details()->is_main_frame);
}

}  // namespace web

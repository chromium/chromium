// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/window_error_java_script_feature.h"

#import <memory>

#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest_mac.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace web {

class WindowErrorJavaScriptFeatureTest : public WebTestWithWebState {
 protected:
  WindowErrorJavaScriptFeatureTest()
      : WebTestWithWebState(),
        feature_(base::BindRepeating(
            ^(WindowErrorJavaScriptFeature::ErrorDetails error_details) {
              error_details_ = error_details;
            })) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    OverrideJavaScriptFeatures(
        {web::java_script_features::GetMessageJavaScriptFeature(), &feature_});
  }

  absl::optional<WindowErrorJavaScriptFeature::ErrorDetails> error_details() {
    return error_details_;
  }

 private:
  WindowErrorJavaScriptFeature feature_;
  absl::optional<WindowErrorJavaScriptFeature::ErrorDetails> error_details_;
};

// Tests that error details are received for a script error occurring in the
// head of the main frame.
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromMainFramePageHead) {
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
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromMainFramePageBody) {
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
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromIframePageHead) {
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
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromIframePageBody) {
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
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorAfterDocumentRecreated) {
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

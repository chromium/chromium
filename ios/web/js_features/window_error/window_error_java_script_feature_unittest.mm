// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/window_error/window_error_java_script_feature.h"

#import <WebKit/WebKit.h>

#include "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace web {

typedef WebTestWithWebState WindowErrorJavaScriptFeatureTest;

// Tests that error details are received for a script error occurring in the
// head of the main frame.
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromMainFramePageHead) {
  __block bool error_details_received = false;
  WindowErrorJavaScriptFeature feature(base::BindRepeating(
      ^(WindowErrorJavaScriptFeature::ErrorDetails error_details) {
        EXPECT_EQ(1, error_details.line_number);
        EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
                    error_details.message);
        EXPECT_EQ("https://chromium.test/", error_details.url.spec());
        EXPECT_TRUE(error_details.is_main_frame);
        error_details_received = true;
      }));

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  NSString* html = @"<html><head>"
                    "<script>nonexistentFunction();</script>"
                    "</head><body></body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_details_received;
  }));
}

// Tests that error details are received for a script error occurring in the
// body of the main frame.
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromMainFramePageBody) {
  __block bool error_details_received = false;
  WindowErrorJavaScriptFeature feature(base::BindRepeating(
      ^(WindowErrorJavaScriptFeature::ErrorDetails error_details) {
        EXPECT_EQ(1, error_details.line_number);
        EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
                    error_details.message);
        EXPECT_EQ("https://chromium.test/", error_details.url.spec());
        EXPECT_TRUE(error_details.is_main_frame);
        error_details_received = true;
      }));

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  NSString* html = @"<html><body>"
                    "<script>nonexistentFunction();</script>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_details_received;
  }));
}

// Tests that error details are received for a script error occurring in the
// head of an iframe.
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromIframePageHead) {
  __block bool error_details_received = false;
  WindowErrorJavaScriptFeature feature(base::BindRepeating(
      ^(WindowErrorJavaScriptFeature::ErrorDetails error_details) {
        EXPECT_EQ(0, error_details.line_number);
        EXPECT_NSEQ(@"Script error.", error_details.message);
        EXPECT_EQ("about:srcdoc", error_details.url.spec());
        EXPECT_FALSE(error_details.is_main_frame);
        error_details_received = true;
      }));

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  NSString* html = @"<html><body>"
                    "<iframe "
                    "srcdoc='<html><head><script>nonexistentFunction();</"
                    "script></head></html>'/>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_details_received;
  }));
}

// Tests that error details are received for a script error occurring in the
// body of an iframe.
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorFromIframePageBody) {
  __block bool error_details_received = false;
  WindowErrorJavaScriptFeature feature(base::BindRepeating(
      ^(WindowErrorJavaScriptFeature::ErrorDetails error_details) {
        EXPECT_EQ(0, error_details.line_number);
        EXPECT_NSEQ(@"Script error.", error_details.message);
        EXPECT_EQ("about:srcdoc", error_details.url.spec());
        EXPECT_FALSE(error_details.is_main_frame);
        error_details_received = true;
      }));

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  NSString* html = @"<html><body>"
                    "<iframe "
                    "srcdoc='<html><body><script>nonexistentFunction();</"
                    "script></body></html>'/>"
                    "</body></html>";
  LoadHtml(html);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_details_received;
  }));
}

// Ensures that error details are still retreived after a document is recreated.
// (Since event listeners are removed and need to be reinjected after a set of
// calls to document.open/write/close.)
TEST_F(WindowErrorJavaScriptFeatureTest, ReceiveErrorAfterDocumentRecreated) {
  __block bool error_details_received = false;
  WindowErrorJavaScriptFeature feature(base::BindRepeating(
      ^(WindowErrorJavaScriptFeature::ErrorDetails error_details) {
        EXPECT_EQ(1, error_details.line_number);
        EXPECT_NSEQ(@"ReferenceError: Can't find variable: nonexistentFunction",
                    error_details.message);
        EXPECT_EQ("https://chromium.test/", error_details.url.spec());
        EXPECT_TRUE(error_details.is_main_frame);
        error_details_received = true;
      }));

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(@"<html></html>");

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"nonexistentFunction();");

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return error_details_received;
  }));
}

}  // namespace web

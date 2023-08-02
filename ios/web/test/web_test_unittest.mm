// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>

#import "ios/web/public/test/fakes/crw_fake_web_view_content_view.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#import "testing/gtest/include/gtest/gtest-spi.h"

namespace {

// Fixture to test that the WebTest fixture properly fails tests when the render
// process crashes.
class WebTestFixtureTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    web_view_ = web::BuildTerminatedWKWebView();
    CRWFakeWebViewContentView* web_view_content_view =
        [[CRWFakeWebViewContentView alloc]
            initWithMockWebView:web_view_
                     scrollView:[web_view_ scrollView]];
    [web_controller() injectWebViewContentView:web_view_content_view];
  }

  WKWebView* web_view_;
};

// Tests that the WebTest fixture triggers a test failure when a render process
// crashes during the test.
TEST_F(WebTestFixtureTest, FailsOnRenderCrash) {
  // EXPECT_FATAL_FAILURE() uses a local class, which cannot access non-static
  // variables of the enclosing function.
  static WKWebView* web_view = web_view_;

  EXPECT_FATAL_FAILURE(web::SimulateWKWebViewCrash(web_view),
                       "Renderer process died unexpectedly");
}

// Tests that `SetIgnoreRenderProcessCrashesDuringTesting()` properly ignores
// intentional render process crashes.
TEST_F(WebTestFixtureTest, SucceedsOnRenderCrash) {
  SetIgnoreRenderProcessCrashesDuringTesting(true);
  web::SimulateWKWebViewCrash(web_view_);
}

}  // namespace

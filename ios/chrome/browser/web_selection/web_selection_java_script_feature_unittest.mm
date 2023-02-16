// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web_selection/web_selection_java_script_feature.h"

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web_selection/web_selection_response.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* kPageHTML =
    @"<html>"
     "  <body>"
     "    This text contains a <span id='selectid'>selection</span>."
     "    <iframe id='frame' srcdoc='"
     "      <html>"
     "        <body>"
     "          This frame contains another "
     "          <span id=\"frameselectid\">frame selection</span>."
     "        </body>"
     "      </html>"
     "    '/>"
     "  </body>"
     "</html>";
}

// Tests for the WebSelectionJavaScriptFeature.
class WebSelectionJavaScriptFeatureTest : public PlatformTest {
 public:
  WebSelectionJavaScriptFeatureTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    feature_list_.InitAndEnableFeature(kIOSEditMenuPartialTranslate);
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    web::test::LoadHtml(kPageHTML, web_state());
  }

  web::WebState* web_state() { return web_state_.get(); }

 private:
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that no selection is returned if nothing is selected.
TEST_F(WebSelectionJavaScriptFeatureTest, GetNoSelection) {
  __block WebSelectionResponse* response = nil;
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(
      web_state(), base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return response != nil;
      }));
  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(response.selectedText, @"");
  EXPECT_TRUE(CGRectEqualToRect(response.sourceRect, CGRectZero));
}

// Tests that selection in main frame is returned correctly.
TEST_F(WebSelectionJavaScriptFeatureTest, GetSelectionMainFrame) {
  web::test::ExecuteJavaScript(@"window.getSelection().selectAllChildren("
                                "document.getElementById('selectid'));",
                               web_state());
  __block WebSelectionResponse* response = nil;
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(
      web_state(), base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return response != nil;
      }));
  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(response.selectedText, @"selection");
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));
}

// Tests that selection in iframe is returned correctly.
TEST_F(WebSelectionJavaScriptFeatureTest, GetSelectionIFrame) {
  web::test::ExecuteJavaScript(
      @"subWindow = document.getElementById('frame').contentWindow;"
       "subWindow.document.getSelection().selectAllChildren("
       "  subWindow.document.getElementById('frameselectid'));",
      web_state());
  __block WebSelectionResponse* response = nil;
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(
      web_state(), base::BindOnce(^(WebSelectionResponse* block_response) {
        response = block_response;
      }));

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return response != nil;
      }));
  EXPECT_TRUE(response.valid);
  EXPECT_NSEQ(response.selectedText, @"frame selection");
  EXPECT_FALSE(CGRectEqualToRect(response.sourceRect, CGRectZero));
}

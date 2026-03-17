// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool_java_script_feature.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"

using optimization_guide::proto::ClickAction;

namespace {

// Constants for the standard HTML page.
constexpr int kHtmlWidth = 500;
constexpr int kHtmlHeight = 500;
constexpr int kMidPointX = kHtmlWidth / 2;
constexpr int kMidPointY = kHtmlHeight / 2;

class ClickToolJavaScriptFeatureTest : public IOSChromeTestWithWebState {
 protected:
  ClickToolJavaScriptFeatureTest() {
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
  }

  void SetUp() override {
    IOSChromeTestWithWebState::SetUp();
    fake_web_client()->SetJavaScriptFeatures({feature()});
    NSString* html = [NSString
        stringWithFormat:
            @"<html><body style='width: %dpx; height: %dpx;'></body></html>",
            kHtmlWidth, kHtmlHeight];
    LoadHtml(html);
  }

  ClickToolJavaScriptFeature* feature() {
    return ClickToolJavaScriptFeature::GetInstance();
  }

  ClickAction CreateClickAction(int x = kMidPointX, int y = kMidPointY) {
    ClickAction action;
    action.mutable_target()->mutable_coordinate()->set_x(x);
    action.mutable_target()->mutable_coordinate()->set_y(y);
    action.set_click_type(ClickAction::LEFT);
    action.set_click_count(ClickAction::SINGLE);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
    return action;
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsNonDict) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return a string.
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.getRegisteredApi('click_tool').addFunction('clickByCoordinate'"
      @", "
      @"  function() { return 'unexpected type'; }"
      @"); true;",
      feature());

  ClickAction action = CreateClickAction();

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActuationErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsError) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return an error dictionary.
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.getRegisteredApi('click_tool').addFunction('clickByCoordinate'"
      @", "
      @"  function() { return {success: false, message: 'Custom JS Error'}; }"
      @"); true;",
      feature());

  ClickAction action = CreateClickAction();

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "Custom JS Error");
}

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsErrorWithoutMessage) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return an error dictionary without message.
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.getRegisteredApi('click_tool').addFunction('clickByCoordinate'"
      @", "
      @"  function() { return {success: false}; }"
      @"); true;",
      feature());

  ClickAction action = CreateClickAction();

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "Unknown error in JS.");
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickFailure) {
  int kIframeSize = 100;
  // Load HTML with an iframe at specific coordinates.
  NSString* html =
      [NSString stringWithFormat:@"<html><body>"
                                 @"<iframe style='position:absolute; left:0px; "
                                 @"top:0px; width:%dpx; height:%dpx;'></iframe>"
                                 @"</body></html>",
                                 kIframeSize, kIframeSize];
  web::test::LoadHtml(html, web_state());
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  ClickAction action = CreateClickAction(kIframeSize / 2, kIframeSize / 2);

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "iframe found at the target coordinates.");
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickSuccess) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  ClickAction action = CreateClickAction();

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

}  // namespace

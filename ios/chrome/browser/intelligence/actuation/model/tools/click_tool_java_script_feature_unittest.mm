// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool_java_script_feature.h"

#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using optimization_guide::proto::ClickAction;

namespace {

// Constants for the standard HTML page.
constexpr int kHtmlWidth = 500;
constexpr int kHtmlHeight = 500;
constexpr int kMidPointX = kHtmlWidth / 2;
constexpr int kMidPointY = kHtmlHeight / 2;

// TODO(crbug.com/472289820): Use ChromeTestWithWebState once it's introduced.
class ClickToolJavaScriptFeatureTest : public PlatformTest {
 protected:
  ClickToolJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    // TODO(crbug.com/483433952): Remove this once it's enabled by default.
    scoped_feature_list_.InitAndEnableFeature(
        web::features::kAssertOnJavaScriptErrors);
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    web::test::OverrideJavaScriptFeatures(profile_.get(), {feature()});
  }

  void SetUp() override {
    PlatformTest::SetUp();
    NSString* html = [NSString
        stringWithFormat:
            @"<html><body style='width: %dpx; height: %dpx;'></body></html>",
            kHtmlWidth, kHtmlHeight];
    web::test::LoadHtml(html, web_state());
  }

  web::WebState* web_state() { return web_state_.get(); }

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

  web::WebFrame* WaitForMainFrame() {
    __block web::WebFrame* main_frame = nullptr;
    CHECK(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForJSCompletionTimeout, ^bool {
          web::WebFramesManager* frames_manager =
              feature()->GetWebFramesManager(web_state());
          main_frame = frames_manager->GetMainWebFrame();
          return main_frame != nullptr;
        }));
    return main_frame;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
};

TEST_F(ClickToolJavaScriptFeatureTest, JsReturnsNonDict) {
  web::WebFrame* main_frame = WaitForMainFrame();

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
  web::WebFrame* main_frame = WaitForMainFrame();

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
  web::WebFrame* main_frame = WaitForMainFrame();

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
  web::WebFrame* main_frame = WaitForMainFrame();

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
  web::WebFrame* main_frame = WaitForMainFrame();
  ClickAction action = CreateClickAction();

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

}  // namespace

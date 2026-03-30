// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/click_tool_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_error.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/common/features.h"
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
  ClickToolJavaScriptFeatureTest()
      : IOSChromeTestWithWebState(WebClientMode::kChromeWebClient) {
    scoped_feature_list_.InitWithFeatures(
        {web::features::kAssertOnJavaScriptErrors, kActorTools}, {});
  }

  void SetUp() override {
    IOSChromeTestWithWebState::SetUp();
    NSString* html = [NSString
        stringWithFormat:
            @"<html><body style='width: %dpx; height: %dpx;'></body></html>",
            kHtmlWidth, kHtmlHeight];
    LoadHtml(html);
  }

  ClickToolJavaScriptFeature* feature() {
    return ClickToolJavaScriptFeature::GetInstance();
  }

  ClickAction CreateClickActionWithCoordinates(int x = kMidPointX,
                                               int y = kMidPointY) {
    ClickAction action;
    action.mutable_target()->mutable_coordinate()->set_x(x);
    action.mutable_target()->mutable_coordinate()->set_y(y);
    action.set_click_type(ClickAction::LEFT);
    action.set_click_count(ClickAction::SINGLE);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
    return action;
  }

  ClickAction CreateClickActionWithNodeId() {
    ClickAction action;
    // Use arbitrary values since the JS function is mocked.
    action.mutable_target()->set_content_node_id(123);
    action.mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token("doc_id");
    action.set_click_type(ClickAction::LEFT);
    action.set_click_count(ClickAction::SINGLE);
    return action;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ClickToolJavaScriptFeatureTest, ClickByCoordinate_JsReturnsNonDict) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return a string.
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('click_tool').addFunction('clickByCoordinate',
          function() { return 'unexpected type'; }
        ); true;
      )"),
                                         feature());

  ClickAction action = CreateClickActionWithCoordinates();

  base::test::TestFuture<ActorTool::ActorResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByCoordinate_JsReturnsError) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return an error dictionary.
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('click_tool').addFunction('clickByCoordinate',
          function() { return {success: false, message: 'Custom JS Error'}; }
        ); true;
      )"),
                                         feature());

  ClickAction action = CreateClickActionWithCoordinates();

  base::test::TestFuture<ActorTool::ActorResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "Custom JS Error");
}

TEST_F(ClickToolJavaScriptFeatureTest,
       ClickByCoordinate_JsReturnsErrorWithoutMessage) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return an error dictionary without message.
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('click_tool').addFunction('clickByCoordinate',
          function() { return {success: false}; }
        ); true;
      )"),
                                         feature());

  ClickAction action = CreateClickActionWithCoordinates();

  base::test::TestFuture<ActorTool::ActorResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "Unknown error in JS.");
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByCoordinate_Success) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  ClickAction action = CreateClickActionWithCoordinates();

  base::test::TestFuture<ActorTool::ActorResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByNodeId_JsReturnsError) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return an error dictionary.
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('click_tool').addFunction('clickByNodeId',
          function() { return {success: false, message: 'Custom JS Error'}; }
        ); true;
      )"),
                                         feature());

  ClickAction action = CreateClickActionWithNodeId();

  base::test::TestFuture<ActorTool::ActorResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "Custom JS Error");
}

TEST_F(ClickToolJavaScriptFeatureTest, ClickByNodeId_Success) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Override the JS function to return a success dictionary.
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('click_tool').addFunction('clickByNodeId',
          function() { return {success: true}; }
        ); true;
      )"),
                                         feature());

  ClickAction action = CreateClickActionWithNodeId();

  base::test::TestFuture<ActorTool::ActorResult> future;
  feature()->Click(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

}  // namespace

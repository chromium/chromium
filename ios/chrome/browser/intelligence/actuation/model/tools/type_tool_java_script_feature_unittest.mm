// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/type_tool_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
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

using optimization_guide::proto::TypeAction;

namespace {

constexpr int kHtmlWidth = 500;
constexpr int kHtmlHeight = 500;
constexpr int kMidPointX = kHtmlWidth / 2;
constexpr int kMidPointY = kHtmlHeight / 2;

class TypeToolJavaScriptFeatureTest : public IOSChromeTestWithWebState {
 protected:
  TypeToolJavaScriptFeatureTest()
      : IOSChromeTestWithWebState(WebClientMode::kChromeWebClient) {
    scoped_feature_list_.InitWithFeatures(
        {web::features::kAssertOnJavaScriptErrors, kActuationTools}, {});
  }

  void SetUp() override {
    IOSChromeTestWithWebState::SetUp();
    NSString* html =
        base::SysUTF8ToNSString(base::StringPrintf(R"(
          <html>
            <body style='width: %dpx; height: %dpx;'>
              <input style='width: 100%%; height: 100%%;'>
            </body>
          </html>
        )",
                                                   kHtmlWidth, kHtmlHeight));
    LoadHtml(html);
  }

  TypeToolJavaScriptFeature* feature() {
    return TypeToolJavaScriptFeature::GetInstance();
  }

  TypeAction CreateTypeActionWithCoordinates(
      const std::string& text = "default",
      TypeAction::TypeMode mode = TypeAction::DELETE_EXISTING,
      bool follow_by_enter = false) {
    TypeAction action;
    action.mutable_target()->mutable_coordinate()->set_x(kMidPointX);
    action.mutable_target()->mutable_coordinate()->set_y(kMidPointY);
    action.mutable_target()->mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
    action.set_text(text);
    action.set_mode(mode);
    action.set_follow_by_enter(follow_by_enter);
    return action;
  }

  TypeAction CreateTypeActionWithIdentifiers(
      uint32_t node_id,
      const std::string& serialized_token,
      const std::string& text = "default",
      TypeAction::TypeMode mode = TypeAction::DELETE_EXISTING,
      bool follow_by_enter = false) {
    TypeAction action;
    action.mutable_target()->set_content_node_id(node_id);
    action.mutable_target()
        ->mutable_document_identifier()
        ->set_serialized_token(serialized_token);
    action.set_text(text);
    action.set_mode(mode);
    action.set_follow_by_enter(follow_by_enter);
    return action;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TypeToolJavaScriptFeatureTest, JsReturnsNonDict) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('type_tool').addFunction('typeByCoordinate',
          function() { return 'unexpected type'; }
        );
        __gCrWeb.getRegisteredApi('type_tool').addFunction('typeByNodeId',
          function() { return 'unexpected type'; }
        );
        true;
      )"),
                                         feature());

  TypeAction type_by_coordinate = CreateTypeActionWithCoordinates();
  TypeAction type_by_node_id = CreateTypeActionWithIdentifiers(
      /*node_id=*/1, /*serialized_token=*/"token");

  base::test::TestFuture<ActuationTool::ActuationResult> coordinate_future;
  base::test::TestFuture<ActuationTool::ActuationResult> node_id_future;

  feature()->Type(main_frame, type_by_coordinate,
                  coordinate_future.GetCallback());
  feature()->Type(main_frame, type_by_node_id, node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.has_value());
  EXPECT_EQ(coordinate_result.error().code,
            ActuationErrorCode::kJavascriptFeatureGotInvalidResult);

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.has_value());
  EXPECT_EQ(node_id_result.error().code,
            ActuationErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(TypeToolJavaScriptFeatureTest, JsReturnsError) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('type_tool').addFunction('typeByCoordinate',
          function() { return {success: false, message: 'Custom JS Error'}; }
        );
        __gCrWeb.getRegisteredApi('type_tool').addFunction('typeByNodeId',
          function() { return {success: false, message: 'Custom JS Error'}; }
        );
        true;
      )"),
                                         feature());

  TypeAction type_by_coordinate = CreateTypeActionWithCoordinates();
  TypeAction type_by_node_id = CreateTypeActionWithIdentifiers(
      /*node_id=*/1, /*serialized_token=*/"token");

  base::test::TestFuture<ActuationTool::ActuationResult> coordinate_future;
  base::test::TestFuture<ActuationTool::ActuationResult> node_id_future;

  feature()->Type(main_frame, type_by_coordinate,
                  coordinate_future.GetCallback());
  feature()->Type(main_frame, type_by_node_id, node_id_future.GetCallback());

  auto coordinate_result = coordinate_future.Get();
  EXPECT_FALSE(coordinate_result.has_value());
  EXPECT_EQ(coordinate_result.error().code,
            ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(coordinate_result.error().message, "Custom JS Error");

  auto node_id_result = node_id_future.Get();
  EXPECT_FALSE(node_id_result.has_value());
  EXPECT_EQ(node_id_result.error().code,
            ActuationErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(node_id_result.error().message, "Custom JS Error");
}

TEST_F(TypeToolJavaScriptFeatureTest, TypeByCoordinate_Success) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  TypeAction action = CreateTypeActionWithCoordinates();

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Type(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(TypeToolJavaScriptFeatureTest, TypeByIdentifier_Success) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());

  // Mock out the JS call since we don't have a way to get the DOM Node ID
  // in this test.
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('type_tool').addFunction('typeByNodeId',
          function() { return {success: true, message: "fake success!"}; }
        ); true;
      )"),
                                         feature());

  // Neither identifier is actually relevant for the test since we mock out the
  // JS call.
  std::string document_identifier = "arbitrary_id";
  int node_id = 123;
  TypeAction action =
      CreateTypeActionWithIdentifiers(node_id, document_identifier);

  base::test::TestFuture<ActuationTool::ActuationResult> future;
  feature()->Type(main_frame, action, future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

}  // namespace

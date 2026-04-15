// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/autofill_form_features_java_script_feature.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

constexpr int kHtmlWidth = 500;
constexpr int kHtmlHeight = 500;
constexpr int kMidPointX = kHtmlWidth / 2;
constexpr int kMidPointY = kHtmlHeight / 2;

class ActionTargetJavaScriptFeatureTest : public IOSChromeTestWithWebState {
 protected:
  ActionTargetJavaScriptFeatureTest()
      : IOSChromeTestWithWebState(WebClientMode::kChromeWebClient) {
    scoped_feature_list_.InitWithFeatures(
        {web::features::kAssertOnJavaScriptErrors, kActorTools}, {});
  }

  void SetUp() override {
    IOSChromeTestWithWebState::SetUp();
    LoadHtml(base::StringPrintf(
        R"(<html><body style='width: %dpx; height: %dpx;'></body></html>)",
        kHtmlWidth, kHtmlHeight));
  }

  ActionTargetJavaScriptFeature* feature() {
    return ActionTargetJavaScriptFeature::GetInstance();
  }

  // Defaults to clicking in the center of the page.
  optimization_guide::proto::ActionTarget CreateTargetWithCoordinates(
      int x = kMidPointX,
      int y = kMidPointY) {
    optimization_guide::proto::ActionTarget target;
    target.mutable_coordinate()->set_x(x);
    target.mutable_coordinate()->set_y(y);
    target.mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
    return target;
  }

  optimization_guide::proto::ActionTarget CreateTargetWithDocumentIdentifier(
      const std::string& document_identifier) {
    optimization_guide::proto::ActionTarget target;
    target.mutable_document_identifier()->set_serialized_token(
        document_identifier);
    return target;
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ActionTargetJavaScriptFeatureTest, JsReturnsUnexpectedType) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() { return 'unexpected type'; }
        ); true;
      )"),
                                         feature());
  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ActionTargetJavaScriptFeatureTest, JsReturnsError) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {success: false, message: 'Custom JS Error'};
          }
        ); true;
      )"),
                                         feature());

  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureFailedInJavaScriptExecution);
  EXPECT_EQ(result.error().message, "Custom JS Error");
}

TEST_F(ActionTargetJavaScriptFeatureTest, TargetsMainFrame_Success) {
  // Intentionally don't load a page with an iframe so that we target the main
  // frame.
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {success: true};
          }
        ); true;
      )"),
                                         feature());
  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->frame, main_frame);
}

TEST_F(ActionTargetJavaScriptFeatureTest,
       TargetsIframeByCoordinates_NeedsFullChildFrameDataFromJs) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Case 1: JS just returns the remoteFrameToken.
  base::UnguessableToken fake_remote_token = base::UnguessableToken::Create();
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      base::SysUTF8ToNSString(
          base::StringPrintf(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {
              'success': true,
              'childFrame': {
                'remoteFrameToken': '%s'
              }
            };
          }
        ); true;
      )",
                             fake_remote_token.ToString().c_str())),
      feature());
  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      first_call;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            first_call.GetCallback());

  auto result = first_call.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult);

  web::test::ExecuteJavaScriptForFeature(web_state(),
                                         base::SysUTF8ToNSString(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {
              'success': true,
              'childFrame': {
                'frameX': 10.0,
                'frameY': 10.0
              }
            };
          }
        ); true;
      )"),
                                         feature());
  target = CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      second_call;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            second_call.GetCallback());

  result = second_call.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

TEST_F(ActionTargetJavaScriptFeatureTest,
       TargetsIframe_NoLocalFrameForRemoteFrameId) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Create a remote frame token that doesn't have an associated local frame
  // token.
  base::UnguessableToken remote_token = base::UnguessableToken::Create();

  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      base::SysUTF8ToNSString(
          base::StringPrintf(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {
              'success': true,
              'childFrame': {
                'remoteFrameToken': '%s',
                'frameX': 10.0,
                'frameY': 10.0
              }
            };
          }
        ); true;
      )",
                             remote_token.ToString().c_str())),
      feature());

  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kActorTargetFrameNotRegistered);
}

TEST_F(ActionTargetJavaScriptFeatureTest, TargetsIframe_FrameIdNotRegistered) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  // Create a mapping to a local frame ID that doesn't exist in the
  // WebFramesManager.
  base::UnguessableToken remote_token = base::UnguessableToken::Create();
  base::UnguessableToken fake_local_token = base::UnguessableToken::Create();

  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  registrar->RegisterMapping(autofill::RemoteFrameToken(remote_token),
                             autofill::LocalFrameToken(fake_local_token));

  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      base::SysUTF8ToNSString(
          base::StringPrintf(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {
              'success': true,
              'childFrame': {
                'remoteFrameToken': '%s',
                'frameX': 10.0,
                'frameY': 10.0
              }
            };
          }
        ); true;
      )",
                             remote_token.ToString().c_str())),
      feature());

  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kActorTargetFrameNotFoundById);
}

TEST_F(ActionTargetJavaScriptFeatureTest, TargetIframe_ByCoordinate_Success) {
  int kIframeSize = 100;
  LoadHtml(base::StringPrintf(R"(
      <html>
        <head><title>Main</title></head>
        <body>
          <iframe src="about:blank"
          style='position:absolute; left:0px; top:0px;
          width:%dpx; height:%dpx;'></iframe>
        </body>
      </html>)",
                              kIframeSize, kIframeSize));

  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(WaitForFrameCount(feature(), 2));

  web::WebFramesManager* frames_manager =
      feature()->GetWebFramesManager(web_state());
  web::WebFrame* iframe = nullptr;
  for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
    if (frame->GetFrameId() != main_frame->GetFrameId()) {
      iframe = frame;
      break;
    }
  }
  ASSERT_NE(iframe, nullptr);

  base::UnguessableToken remote_token = base::UnguessableToken::Create();
  std::optional<base::UnguessableToken> local_token =
      autofill::DeserializeJavaScriptFrameId(iframe->GetFrameId());

  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  registrar->RegisterMapping(autofill::RemoteFrameToken(remote_token),
                             autofill::LocalFrameToken(*local_token));

  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      base::SysUTF8ToNSString(
          base::StringPrintf(R"(
        __gCrWeb.getRegisteredApi('action_target').addFunction(
          'resolveTargetIframe', function() {
            return {
              'success': true,
              'childFrame': {
                'remoteFrameToken': '%s',
                'frameX': 10.0,
                'frameY': 10.0
              }
            };
          }
        ); true;
      )",
                             remote_token.ToString().c_str())),
      feature());

  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates(/*x=*/kIframeSize / 2, /*y=*/kIframeSize / 2);

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->frame, iframe);
}

TEST_F(ActionTargetJavaScriptFeatureTest,
       TargetIframe_ByDocumentIdentifier_Success) {
  int kIframeSize = 100;
  LoadHtml(base::StringPrintf(R"(
      <html>
        <head><title>Main</title></head>
        <body>
          <iframe src="about:blank"
          style='position:absolute; left:0px; top:0px;
          width:%dpx; height:%dpx;'>
          </iframe>
        </body>
      </html>)",
                              kIframeSize, kIframeSize));
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(WaitForFrameCount(feature(), 2));

  web::WebFramesManager* frames_manager =
      feature()->GetWebFramesManager(web_state());
  web::WebFrame* iframe = nullptr;
  for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
    if (frame->GetFrameId() != main_frame->GetFrameId()) {
      iframe = frame;
      break;
    }
  }
  // Generate and register a remote frame ID for the iframe.
  autofill::ChildFrameRegistrar* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state());
  base::UnguessableToken remote_token = base::UnguessableToken::Create();
  std::optional<base::UnguessableToken> local_token =
      autofill::DeserializeJavaScriptFrameId(iframe->GetFrameId());
  registrar->RegisterMapping(autofill::RemoteFrameToken(remote_token),
                             autofill::LocalFrameToken(*local_token));

  optimization_guide::proto::ActionTarget target =
      CreateTargetWithDocumentIdentifier(remote_token.ToString());

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(web_state(), main_frame, target,
                            future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->frame, iframe);
}

TEST_F(ActionTargetJavaScriptFeatureTest, MaxDepthExceeded) {
  web::WebFrame* main_frame = WaitForMainFrame(feature());
  ASSERT_TRUE(main_frame);

  optimization_guide::proto::ActionTarget target =
      CreateTargetWithCoordinates();

  base::test::TestFuture<base::expected<
      ActionTargetJavaScriptFeature::TargetFrameResult, ActorToolError>>
      future;
  feature()->GetTargetFrame(
      web_state(), main_frame, target, future.GetCallback(),
      ActionTargetJavaScriptFeature::kMaxTargetIframeDepth);

  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            ActorToolErrorCode::kActorTargetMaxDepthExceeded);
}

}  // namespace

}  // namespace actor

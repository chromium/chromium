// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/type_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake `UIView<UIKeyInput>` that reports itself as first responder and records
// inserted text for testing native keystroke auto-submission.
@interface FakeKeyInputView : UIView <UIKeyInput>
@property(nonatomic, assign) BOOL forceFirstResponder;
@property(nonatomic, copy) NSString* insertedText;
@end

@implementation FakeKeyInputView

- (BOOL)isFirstResponder {
  return self.forceFirstResponder;
}

- (BOOL)hasText {
  return self.insertedText.length > 0;
}

- (void)insertText:(NSString*)text {
  self.insertedText = text;
}

- (void)deleteBackward {
}

@end

namespace actor {

namespace {

constexpr int kTargetNodeId = 123;
constexpr int kTestFieldRendererId = 42;
constexpr int kOkResultCode = 0;

// Returns true if `call` contains `substring`.
bool JsCallContains(std::u16string_view call, std::u16string_view substring) {
  return call.find(substring) != std::u16string_view::npos;
}

// Returns true if `frame`'s recorded JavaScript call history contains a call
// matching `substring`. Returns false if `frame` is null or no matching call
// was recorded.
bool HasJavaScriptCallContaining(web::FakeWebFrame* frame,
                                 std::u16string_view substring) {
  if (!frame) {
    return false;
  }
  for (const std::u16string& call : frame->GetJavaScriptCallHistory()) {
    if (JsCallContains(call, substring)) {
      return true;
    }
  }
  return false;
}

}  // namespace

class TypeToolTest : public PlatformTest {
 public:
  TypeToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
  }

  void TearDown() override {
    if (key_input_view_) {
      key_input_view_.forceFirstResponder = NO;
      [key_input_view_ removeFromSuperview];
      key_input_view_ = nil;
    }
    PlatformTest::TearDown();
  }

 protected:
  std::pair<std::unique_ptr<web::FakeWebState>, web::FakeWebFrame*>
  CreateWebStateWithFeatures(
      std::vector<web::JavaScriptFeature*> features,
      std::optional<int> resolve_target_result_code = kOkResultCode,
      std::optional<int> type_result_code = kOkResultCode,
      std::optional<int> field_renderer_id = kTestFieldRendererId) {
    web::test::OverrideJavaScriptFeatures(profile_.get(), std::move(features));
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    std::unique_ptr<web::FakeWebFrame> main_frame =
        web::FakeWebFrame::CreateMainWebFrame();
    main_frame->set_browser_state(profile_.get());
    web::FakeWebFrame* main_frame_ptr = main_frame.get();

    if (resolve_target_result_code.has_value()) {
      resolve_iframe_js_result_ = base::Value(
          base::DictValue().Set("resultCode", *resolve_target_result_code));
      main_frame_ptr->AddJsResultForFunctionCall(
          &resolve_iframe_js_result_, "action_target.resolveTargetIframe");
    }

    if (type_result_code.has_value()) {
      base::DictValue type_dict;
      type_dict.Set("resultCode", *type_result_code);
      if (field_renderer_id.has_value()) {
        type_dict.Set("fieldRendererId", *field_renderer_id);
      }
      type_js_result_ = base::Value(std::move(type_dict));
      main_frame_ptr->AddJsResultForFunctionCall(&type_js_result_,
                                                 "type_tool.type");
    }

    frames_manager->AddWebFrame(std::move(main_frame));
    web_state->SetWebFramesManager(
        TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
        std::move(frames_manager));
    return {std::move(web_state), main_frame_ptr};
  }

  FakeKeyInputView* AttachFakeKeyInputView(web::FakeWebState* web_state) {
    // Arbitrary non-empty frame so the fake key input view has valid bounds in
    // the window hierarchy.
    static constexpr CGRect kFakeKeyInputViewFrame = {{0, 0}, {100, 40}};
    if (!key_input_view_) {
      key_input_view_ =
          [[FakeKeyInputView alloc] initWithFrame:kFakeKeyInputViewFrame];
      key_input_view_.forceFirstResponder = YES;
      UIWindow* key_window = GetAnyKeyWindow();
      EXPECT_TRUE(key_window);
      [key_window addSubview:key_input_view_];
    }
    web_state->SetView(key_input_view_);
    return key_input_view_;
  }

  web::WebState* InsertActiveWebState(
      std::unique_ptr<web::FakeWebState> web_state) {
    int index = browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
    return browser_->GetWebStateList()->GetWebStateAt(index);
  }

  optimization_guide::proto::TypeAction CreateDefaultTypeAction(
      web::WebState* web_state,
      bool follow_by_enter = false) {
    static constexpr int kTargetX = 50;
    static constexpr int kTargetY = 50;
    optimization_guide::proto::TypeAction type_action;
    if (web_state) {
      type_action.set_tab_id(web_state->GetUniqueIdentifier().identifier());
    }
    type_action.mutable_target()->mutable_coordinate()->set_x(kTargetX);
    type_action.mutable_target()->mutable_coordinate()->set_y(kTargetY);
    type_action.set_text("hello");
    type_action.set_mode(optimization_guide::proto::TypeAction::APPEND);
    type_action.set_follow_by_enter(follow_by_enter);
    return type_action;
  }

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult>
  CreateToolAndValidate(const optimization_guide::proto::TypeAction& action,
                        web::WebState* web_state) {
    std::unique_ptr<TypeTool> tool =
        TypeTool::Create(web_state ? web_state->GetWeakPtr() : nullptr, action);
    CHECK(tool);
    base::test::TestFuture<ToolExecutionResult> validate_future;
    tool->Validate(validate_future.GetCallback());
    if (!validate_future.Get().IsOk()) {
      return base::unexpected(validate_future.Get());
    }
    return tool;
  }

  ToolExecutionResult CreateValidateAndExecuteTool(
      web::WebState* web_state,
      bool follow_by_enter = false) {
    optimization_guide::proto::TypeAction action =
        CreateDefaultTypeAction(web_state, follow_by_enter);
    base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult>
        create_result = CreateToolAndValidate(action, web_state);
    if (!create_result.has_value()) {
      return create_result.error();
    }
    base::test::TestFuture<ToolExecutionResult> future;
    create_result.value()->Execute(future.GetCallback());
    return future.Get();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeKeyInputView* key_input_view_ = nil;
  base::Value resolve_iframe_js_result_;
  base::Value type_js_result_;
};

TEST_F(TypeToolTest, Validate_MissingText) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  action.mutable_type()->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  action.mutable_type()->set_mode(
      optimization_guide::proto::TypeAction::APPEND);
  action.mutable_type()->mutable_target()->set_content_node_id(kTargetNodeId);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), inserted_web_state);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(TypeToolTest, Validate_MissingMode) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  action.mutable_type()->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  action.mutable_type()->set_text("test");
  action.mutable_type()->mutable_target()->set_content_node_id(kTargetNodeId);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), inserted_web_state);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(TypeToolTest, Validate_MissingTarget) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  action.mutable_type()->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  action.mutable_type()->set_text("test");
  action.mutable_type()->set_mode(
      optimization_guide::proto::TypeAction::APPEND);

  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action.type(), inserted_web_state);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

TEST_F(TypeToolTest, Execute_WebStateDestroyed_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  optimization_guide::proto::TypeAction action =
      CreateDefaultTypeAction(inserted_web_state);
  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> create_result =
      CreateToolAndValidate(action, inserted_web_state);
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<TypeTool> tool = std::move(create_result.value());

  CloseAllWebStates(*browser_->GetWebStateList(),
                    WebStateList::ClosingReason::kDefault);
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kTabWentAway);
}

TEST_F(TypeToolTest, Execute_NoWebFramesManager_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(
      web_state->GetWebFramesManager(
          TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld()),
      nullptr);
  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  ToolExecutionResult result = CreateValidateAndExecuteTool(inserted_web_state);
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(TypeToolTest, Execute_NoMainFrame_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_NE(
      web_state->GetWebFramesManager(
          TypeToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld()),
      nullptr);
  ASSERT_EQ(web_state
                ->GetWebFramesManager(TypeToolJavaScriptFeature::GetInstance()
                                          ->GetSupportedContentWorld())
                ->GetMainWebFrame(),
            nullptr);

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  ToolExecutionResult result = CreateValidateAndExecuteTool(inserted_web_state);
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

TEST_F(TypeToolTest, GetToolType) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  optimization_guide::proto::TypeAction action =
      CreateDefaultTypeAction(inserted_web_state);
  base::expected<std::unique_ptr<TypeTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action, inserted_web_state);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetToolType(), ToolType::kType);
}

// Tests that `Execute()` with `follow_by_enter = false` skips both native
// keystroke auto-submission and the JS Enter fallback after typing completes.
TEST_F(TypeToolTest, Execute_FollowByEnterFalse_DoesNotCallFallback) {
  auto [web_state, main_frame_ptr] =
      CreateWebStateWithFeatures({ActionTargetJavaScriptFeature::GetInstance(),
                                  TypeToolJavaScriptFeature::GetInstance()});

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/false);
  EXPECT_TRUE(result.IsOk());
  ASSERT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 2u);
  EXPECT_TRUE(
      JsCallContains(main_frame_ptr->GetJavaScriptCallHistory()[1],
                     u"__gCrWeb.callFunctionInGcrWeb('type_tool', 'type'"));
  EXPECT_FALSE(
      HasJavaScriptCallContaining(main_frame_ptr, u"submitWithEnterFallback"));
}

// Tests that `Execute()` with `follow_by_enter = true` falls back to calling
// `type_tool.submitWithEnterFallback()` with the resolved `fieldRendererId`
// when native keystroke auto-submission cannot dispatch.
TEST_F(TypeToolTest,
       Execute_FollowByEnterTrue_NativeFails_CallsSubmitWithEnterFallback) {
  auto [web_state, main_frame_ptr] =
      CreateWebStateWithFeatures({ActionTargetJavaScriptFeature::GetInstance(),
                                  TypeToolJavaScriptFeature::GetInstance()});

  base::Value fallback_js_result(
      base::DictValue().Set("resultCode", kOkResultCode));
  main_frame_ptr->AddJsResultForFunctionCall(
      &fallback_js_result, "type_tool.submitWithEnterFallback");

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/true);
  EXPECT_TRUE(result.IsOk());
  ASSERT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 3u);
  EXPECT_TRUE(
      JsCallContains(main_frame_ptr->GetJavaScriptCallHistory()[1],
                     u"__gCrWeb.callFunctionInGcrWeb('type_tool', 'type'"));
  EXPECT_TRUE(JsCallContains(main_frame_ptr->GetJavaScriptCallHistory()[1],
                             u"\"hello\", 3]);"));
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory()[2],
            u"__gCrWeb.callFunctionInGcrWeb('type_tool', "
            u"'submitWithEnterFallback', [42.0]);");
}

// Tests that `Execute()` with `follow_by_enter = true` dispatches a native
// Return keystroke to the first responder and does not invoke
// `type_tool.submitWithEnterFallback()` when native auto-submission succeeds.
TEST_F(
    TypeToolTest,
    Execute_FollowByEnterTrue_NativeSucceeds_DoesNotCallSubmitWithEnterFallback) {
  auto [web_state, main_frame_ptr] = CreateWebStateWithFeatures(
      {ActionTargetJavaScriptFeature::GetInstance(),
       TypeToolJavaScriptFeature::GetInstance(),
       password_manager::PasswordManagerJavaScriptFeature::GetInstance()});

  FakeKeyInputView* key_input_view = AttachFakeKeyInputView(web_state.get());

  base::Value bool_true_result(true);
  main_frame_ptr->AddJsResultForFunctionCall(&bool_true_result,
                                             "passwords.focusElement");
  main_frame_ptr->AddJsResultForFunctionCall(
      &bool_true_result, "passwords.setUpRendererKeystrokeShield");
  main_frame_ptr->AddJsResultForFunctionCall(
      &bool_true_result, "passwords.removeRendererKeystrokeShield");

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));
  inserted_web_state->WasShown();

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/true);
  EXPECT_TRUE(result.IsOk());
  EXPECT_NSEQ(key_input_view.insertedText, @"\n");
  EXPECT_FALSE(HasJavaScriptCallContaining(
      main_frame_ptr, u"passwords.preventKeyboardOnElement"));
  EXPECT_FALSE(
      HasJavaScriptCallContaining(main_frame_ptr, u"submitWithEnterFallback"));
}

// Tests that `Execute()` with `follow_by_enter = true` skips native keystroke
// auto-submission and falls back to `type_tool.submitWithEnterFallback()` when
// the target WebState is in the background (`!IsVisible()`).
TEST_F(
    TypeToolTest,
    Execute_FollowByEnterTrue_BackgroundTab_SkipsNativeAndUsesSubmitFallback) {
  auto [web_state, main_frame_ptr] = CreateWebStateWithFeatures(
      {ActionTargetJavaScriptFeature::GetInstance(),
       TypeToolJavaScriptFeature::GetInstance(),
       password_manager::PasswordManagerJavaScriptFeature::GetInstance()});

  FakeKeyInputView* key_input_view = AttachFakeKeyInputView(web_state.get());

  base::Value fallback_js_result(
      base::DictValue().Set("resultCode", kOkResultCode));
  main_frame_ptr->AddJsResultForFunctionCall(
      &fallback_js_result, "type_tool.submitWithEnterFallback");

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));
  // Do NOT call `WasShown()`, simulating a background tab (`!IsVisible()`).
  ASSERT_FALSE(inserted_web_state->IsVisible());

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/true);
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(key_input_view.insertedText, nil);
  EXPECT_FALSE(HasJavaScriptCallContaining(main_frame_ptr, u"passwords."));
  EXPECT_TRUE(
      HasJavaScriptCallContaining(main_frame_ptr, u"submitWithEnterFallback"));
}

// Tests that `Execute()` with `follow_by_enter = true` aborts native keystroke
// auto-submission and falls back to `type_tool.submitWithEnterFallback()` if
// the tab is hidden mid-flow.
TEST_F(TypeToolTest,
       Execute_FollowByEnterTrue_TabHiddenMidFlow_AbortsNativeAndUsesFallback) {
  auto [web_state, main_frame_ptr] = CreateWebStateWithFeatures(
      {ActionTargetJavaScriptFeature::GetInstance(),
       TypeToolJavaScriptFeature::GetInstance(),
       password_manager::PasswordManagerJavaScriptFeature::GetInstance()});

  FakeKeyInputView* key_input_view = AttachFakeKeyInputView(web_state.get());

  base::Value bool_true_result(true);
  main_frame_ptr->AddJsResultForFunctionCall(&bool_true_result,
                                             "passwords.focusElement");
  base::Value fallback_js_result(
      base::DictValue().Set("resultCode", kOkResultCode));
  main_frame_ptr->AddJsResultForFunctionCall(
      &fallback_js_result, "type_tool.submitWithEnterFallback");

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));
  inserted_web_state->WasShown();

  // Simulate the user switching tabs (and even switching back) while
  // `passwords.focusElement()` is in flight so `SubmitFlowContext::WasHidden()`
  // latches `state_.aborted = true`.
  main_frame_ptr->SetJavaScriptFunctionCallback(
      "passwords.focusElement", base::BindRepeating(
                                    [](base::WeakPtr<web::WebState> web_state) {
                                      if (!web_state) {
                                        return;
                                      }
                                      web_state->WasHidden();
                                      web_state->WasShown();
                                    },
                                    inserted_web_state->GetWeakPtr()));

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/true);
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(key_input_view.insertedText, nil);
  EXPECT_FALSE(HasJavaScriptCallContaining(main_frame_ptr,
                                           u"setUpRendererKeystrokeShield"));
  EXPECT_FALSE(HasJavaScriptCallContaining(
      main_frame_ptr, u"passwords.preventKeyboardOnElement"));
  EXPECT_TRUE(
      HasJavaScriptCallContaining(main_frame_ptr, u"submitWithEnterFallback"));
}

// Tests that `Execute()` with `follow_by_enter = true` aborts without
// attempting auto-submission or JS Enter fallback when `type_tool.type()`
// returns an error.
TEST_F(TypeToolTest,
       Execute_FollowByEnterTrue_TypeFails_DoesNotAttemptAutoSubmitOrFallback) {
  static constexpr int kElementDisabledResultCode = 6;
  auto [web_state, main_frame_ptr] = CreateWebStateWithFeatures(
      {ActionTargetJavaScriptFeature::GetInstance(),
       TypeToolJavaScriptFeature::GetInstance()},
      /*resolve_target_result_code=*/kOkResultCode,
      /*type_result_code=*/kElementDisabledResultCode);

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/true);
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kElementDisabled);
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 2u);
  EXPECT_FALSE(HasJavaScriptCallContaining(main_frame_ptr, u"passwords."));
  EXPECT_FALSE(
      HasJavaScriptCallContaining(main_frame_ptr, u"submitWithEnterFallback"));
}

// Tests that `Execute()` with `follow_by_enter = true` skips both native
// keystroke auto-submission and `type_tool.submitWithEnterFallback()` if
// `fieldRendererId` is missing.
TEST_F(TypeToolTest,
       Execute_FollowByEnterTrue_MissingFieldRendererId_SkipsFallback) {
  auto [web_state, main_frame_ptr] =
      CreateWebStateWithFeatures({ActionTargetJavaScriptFeature::GetInstance(),
                                  TypeToolJavaScriptFeature::GetInstance()},
                                 /*resolve_target_result_code=*/kOkResultCode,
                                 /*type_result_code=*/kOkResultCode,
                                 /*field_renderer_id=*/std::nullopt);

  web::WebState* inserted_web_state =
      InsertActiveWebState(std::move(web_state));

  ToolExecutionResult result = CreateValidateAndExecuteTool(
      inserted_web_state, /*follow_by_enter=*/true);
  EXPECT_TRUE(result.IsOk());
  EXPECT_EQ(main_frame_ptr->GetJavaScriptCallHistory().size(), 2u);
  EXPECT_FALSE(HasJavaScriptCallContaining(main_frame_ptr, u"passwords."));
  EXPECT_FALSE(
      HasJavaScriptCallContaining(main_frame_ptr, u"submitWithEnterFallback"));
}

}  // namespace actor

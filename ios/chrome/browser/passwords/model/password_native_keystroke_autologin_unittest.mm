// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_native_keystroke_autologin.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>
#import <string>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/null_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/password_manager/ios/features.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake `UIView<UIKeyInput>` that tracks responder transitions and stores the
// last string passed to `insertText:`.
@interface FakeAutologinKeyInputView : UIView <UIKeyInput>
// True if this view is allowed to become the first responder.
@property(nonatomic, assign) BOOL canBecomeFirstResponderValue;
// True if this view currently reports itself as the first responder.
@property(nonatomic, assign) BOOL simulatesFirstResponder;
// True if `endEditing:` was invoked on this view.
@property(nonatomic, assign) BOOL endEditingCalled;
// The last text string passed to `insertText:`.
@property(nonatomic, copy) NSString* insertedText;
@end

@implementation FakeAutologinKeyInputView

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return self.canBecomeFirstResponderValue;
}

- (BOOL)becomeFirstResponder {
  if (self.canBecomeFirstResponderValue) {
    self.simulatesFirstResponder = YES;
    return YES;
  }
  return NO;
}

- (BOOL)resignFirstResponder {
  self.simulatesFirstResponder = NO;
  return YES;
}

- (BOOL)isFirstResponder {
  return self.simulatesFirstResponder;
}

#pragma mark - UIView

- (BOOL)endEditing:(BOOL)force {
  self.endEditingCalled = YES;
  self.simulatesFirstResponder = NO;
  return [super endEditing:force];
}

#pragma mark - UIKeyInput

- (BOOL)hasText {
  return self.insertedText.length > 0;
}

- (void)insertText:(NSString*)text {
  self.insertedText = text;
}

- (void)deleteBackward {
}

@end

namespace {

constexpr autofill::FieldRendererId kTestFieldId(42);

class PasswordNativeKeystrokeAutologinTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    web::test::OverrideJavaScriptFeatures(
        profile_.get(),
        {password_manager::PasswordManagerJavaScriptFeature::GetInstance()});

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager_ = frames_manager.get();
    std::unique_ptr<web::FakeWebFrame> main_frame =
        web::FakeWebFrame::CreateMainWebFrame();
    main_frame->set_browser_state(profile_.get());
    main_frame_ = main_frame.get();
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state_->SetWebFramesManager(
        password_manager::PasswordManagerJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::move(frames_manager));

    main_frame_->AddJsResultForFunctionCall(
        &bool_true_result_, "passwords.preventKeyboardOnElement");
    main_frame_->AddJsResultForFunctionCall(&bool_true_result_,
                                            "passwords.focusElement");
    main_frame_->AddJsResultForFunctionCall(
        &bool_true_result_, "passwords.setUpRendererKeystrokeShield");
    main_frame_->AddJsResultForFunctionCall(
        &bool_true_result_, "passwords.removeRendererKeystrokeShield");
    main_frame_->AddJsResultForFunctionCall(
        &bool_true_result_, "passwords.restoreKeyboardOnElement");

    web_view_ = [[FakeAutologinKeyInputView alloc]
        initWithFrame:CGRectMake(0, 0, 100, 40)];
    web_view_.canBecomeFirstResponderValue = YES;
    web_view_.simulatesFirstResponder = YES;
    UIWindow* key_window = GetAnyKeyWindow();
    ASSERT_TRUE(key_window);
    [key_window endEditing:YES];
    [key_window addSubview:web_view_];
    web_state_->SetView(web_view_);
    web_state_->WasShown();
  }

  void TearDown() override {
    main_frame_ = nullptr;
    frames_manager_ = nullptr;
    web_state_.reset();
    profile_.reset();
    web_view_.simulatesFirstResponder = NO;
    [web_view_ removeFromSuperview];
    web_view_ = nil;
    PlatformTest::TearDown();
  }

  // Returns true if any JS call on `main_frame_` contains `substring`.
  bool HasJavaScriptCallContaining(const std::u16string& substring) {
    for (const std::u16string& call : main_frame_->GetJavaScriptCallHistory()) {
      if (call.find(substring) != std::u16string::npos) {
        return true;
      }
    }
    return false;
  }

  web::WebTaskEnvironment task_environment_;
  base::Value bool_true_result_{true};
  base::Value bool_false_result_{false};
  FakeAutologinKeyInputView* web_view_ = nil;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<web::FakeWebFramesManager> frames_manager_ = nullptr;
  raw_ptr<web::FakeWebFrame> main_frame_ = nullptr;
};

// Tests that `TriggerAutoSubmission` invokes `completion_handler` and
// dispatches `@"\n"` to the focused view inside the `WebState`.
TEST_F(PasswordNativeKeystrokeAutologinTest, TriggerAutoSubmission_Success) {
  base::test::TestFuture<void> future;
  TriggerAutoSubmission(web_state_->GetWeakPtr(), web::kMainFakeFrameId,
                        kTestFieldId, future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_NSEQ(web_view_.insertedText, @"\n");
  EXPECT_TRUE(web_view_.endEditingCalled);
  EXPECT_TRUE(web_view_.isFirstResponder);
}

// Tests that `TriggerAutoSubmissionWithResult` completes the full native
// keystroke auto-submission flow, dispatches `@"\n"` to the focused view inside
// the `WebState`, and restores all temporary shields and `inputmode` state.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_Success) {
  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{.keystroke_dispatched = true});
  EXPECT_NSEQ(web_view_.insertedText, @"\n");
  EXPECT_TRUE(web_view_.endEditingCalled);
  EXPECT_TRUE(web_view_.userInteractionEnabled);
  EXPECT_TRUE(HasJavaScriptCallContaining(u"preventKeyboardOnElement"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"focusElement"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"setUpRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"removeRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

// Tests that `TriggerAutoSubmissionWithResult` disables
// `userInteractionEnabled` during the flow and restores
// `userInteractionEnabled = YES` on completion when `kAutoSubmissionTypeParam`
// is `kDismissThenBlockThenSubmit`.
TEST_F(
    PasswordNativeKeystrokeAutologinTest,
    TriggerAutoSubmissionWithResult_DismissThenBlockThenSubmit_RaisesAndRestoresShield) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kIOSPasswordAutoSubmission,
      {{"auto-submission-type", "DismissThenBlockThenSubmit"}});

  bool interaction_disabled_during_flow = false;
  main_frame_->SetJavaScriptFunctionCallback(
      "passwords.focusElement",
      base::BindRepeating(
          [](UIView* view, bool* disabled_flag) {
            *disabled_flag = !view.userInteractionEnabled;
          },
          web_view_, &interaction_disabled_during_flow));
  base::ScopedClosureRunner clear_js_callback(base::BindOnce(
      [](web::FakeWebFrame* frame) {
        frame->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             base::NullCallback());
      },
      main_frame_.get()));

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{.keystroke_dispatched = true});
  EXPECT_TRUE(interaction_disabled_during_flow);
  EXPECT_NSEQ(web_view_.insertedText, @"\n");
  EXPECT_TRUE(web_view_.userInteractionEnabled);
}

// Tests that `TriggerAutoSubmissionWithResult` skips `preventKeyboardOnElement`
// and `restoreKeyboardOnElement` when `should_prevent_keyboard` is false.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_SkipPreventKeyboard) {
  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(
      web_state_->GetWeakPtr(), web::kMainFakeFrameId, kTestFieldId,
      future.GetCallback(), /*should_prevent_keyboard=*/false);

  EXPECT_EQ(future.Get(), AutoSubmissionState{.keystroke_dispatched = true});
  EXPECT_NSEQ(web_view_.insertedText, @"\n");
  EXPECT_FALSE(HasJavaScriptCallContaining(u"preventKeyboardOnElement"));
  EXPECT_FALSE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

// Tests that `TriggerAutoSubmissionWithResult` invokes `completion_handler`
// synchronously without running any JavaScript if `weak_web_state` is null.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_NullWebState_FailsEarlySynchronously) {
  bool callback_called = false;
  AutoSubmissionState observed_state{.keystroke_dispatched = true};
  TriggerAutoSubmissionWithResult(
      nullptr, web::kMainFakeFrameId, kTestFieldId,
      base::BindOnce(
          [](bool* called, AutoSubmissionState* out_state,
             AutoSubmissionState state) {
            *called = true;
            *out_state = state;
          },
          &callback_called, &observed_state));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(observed_state, AutoSubmissionState{.keystroke_dispatched = false});
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(main_frame_->GetJavaScriptCallHistory().empty());
}

// Tests that `TriggerAutoSubmissionWithResult` invokes `completion_handler`
// synchronously without running any JavaScript if `weak_web_state->GetView()`
// is nil.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_NullView_FailsEarlySynchronously) {
  web_state_->SetView(nil);

  bool callback_called = false;
  AutoSubmissionState observed_state{.keystroke_dispatched = true};
  TriggerAutoSubmissionWithResult(
      web_state_->GetWeakPtr(), web::kMainFakeFrameId, kTestFieldId,
      base::BindOnce(
          [](bool* called, AutoSubmissionState* out_state,
             AutoSubmissionState state) {
            *called = true;
            *out_state = state;
          },
          &callback_called, &observed_state));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(observed_state, AutoSubmissionState{.keystroke_dispatched = false});
  EXPECT_TRUE(main_frame_->GetJavaScriptCallHistory().empty());
}

// Tests that `TriggerAutoSubmissionWithResult` calls `[view endEditing:YES]`
// and invokes `completion_handler` synchronously without running any JavaScript
// if the target frame is missing.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_MissingFrame_FailsEarlySynchronously) {
  bool callback_called = false;
  AutoSubmissionState observed_state{.keystroke_dispatched = true};
  TriggerAutoSubmissionWithResult(
      web_state_->GetWeakPtr(), "nonexistent_frame_id", kTestFieldId,
      base::BindOnce(
          [](bool* called, AutoSubmissionState* out_state,
             AutoSubmissionState state) {
            *called = true;
            *out_state = state;
          },
          &callback_called, &observed_state));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(observed_state, AutoSubmissionState{.keystroke_dispatched = false});
  EXPECT_TRUE(web_view_.endEditingCalled);
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(main_frame_->GetJavaScriptCallHistory().empty());
}

// Tests that when `passwords.focusElement` fails, the flow terminates early,
// restores `inputmode` on the element, and does not set up the renderer shield
// or dispatch a native keystroke.
TEST_F(
    PasswordNativeKeystrokeAutologinTest,
    TriggerAutoSubmissionWithResult_FocusElementFails_RestoresInputModeAndSkipsSubmit) {
  main_frame_->AddJsResultForFunctionCall(&bool_false_result_,
                                          "passwords.focusElement");

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{.keystroke_dispatched = false});
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(HasJavaScriptCallContaining(u"preventKeyboardOnElement"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"focusElement"));
  EXPECT_FALSE(HasJavaScriptCallContaining(u"setUpRendererKeystrokeShield"));
  EXPECT_FALSE(HasJavaScriptCallContaining(u"removeRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

// Tests that `TriggerAutoSubmissionWithResult` fails early without running any
// JavaScript if `field_id` is null.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_NullFieldId_FailsEarly) {
  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(
      web_state_->GetWeakPtr(), web::kMainFakeFrameId,
      autofill::FieldRendererId(), future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{});
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(main_frame_->GetJavaScriptCallHistory().empty());
}

// Tests that `TriggerAutoSubmissionWithResult` fails early without running any
// JavaScript if the `WebState` is hidden.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_HiddenWebState_FailsEarly) {
  web_state_->WasHidden();

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{});
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(main_frame_->GetJavaScriptCallHistory().empty());
}

// Tests that `TriggerAutoSubmissionWithResult` fails early without running any
// JavaScript if the `WebState`'s view is not attached to a window.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_UnattachedView_FailsEarly) {
  [web_view_ removeFromSuperview];

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{});
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(main_frame_->GetJavaScriptCallHistory().empty());
}

// Tests that hiding the tab mid-flow latches `aborted = true` (even if
// shown again before the JS callback returns), skips keystroke injection, and
// reverts the interaction shield and `inputmode`.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_TabHiddenMidFlow_AbortsAndCleansUp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kIOSPasswordAutoSubmission,
      {{"auto-submission-type", "DismissThenBlockThenSubmit"}});

  main_frame_->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             base::BindRepeating(
                                                 [](web::WebState* web_state) {
                                                   web_state->WasHidden();
                                                   web_state->WasShown();
                                                 },
                                                 web_state_.get()));
  base::ScopedClosureRunner clear_js_callback(base::BindOnce(
      [](web::FakeWebFrame* frame) {
        frame->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             base::NullCallback());
      },
      main_frame_.get()));

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), (AutoSubmissionState{.aborted = true,
                                               .keystroke_dispatched = false}));
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(web_view_.userInteractionEnabled);
  EXPECT_FALSE(HasJavaScriptCallContaining(u"setUpRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

// Tests that detaching the `WebState`'s view from its window mid-flow latches
// `aborted = true`, skips keystroke injection, and reverts the interaction
// shield and `inputmode`.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_ViewDetachedMidFlow_AbortsAndCleansUp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kIOSPasswordAutoSubmission,
      {{"auto-submission-type", "DismissThenBlockThenSubmit"}});

  base::RunLoop focus_called_loop;
  main_frame_->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             focus_called_loop.QuitClosure());
  base::ScopedClosureRunner clear_js_callback(base::BindOnce(
      [](web::FakeWebFrame* frame) {
        frame->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             base::NullCallback());
      },
      main_frame_.get()));

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  focus_called_loop.Run();
  [web_view_ removeFromSuperview];

  EXPECT_EQ(future.Get(), (AutoSubmissionState{.aborted = true,
                                               .keystroke_dispatched = false}));
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(web_view_.userInteractionEnabled);
  EXPECT_FALSE(HasJavaScriptCallContaining(u"setUpRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

// Tests that removing the target `WebFrame` mid-flow latches `aborted = true`,
// skips keystroke injection, and reverts the interaction shield on the
// `WebState`'s view.
TEST_F(
    PasswordNativeKeystrokeAutologinTest,
    TriggerAutoSubmissionWithResult_FrameDestroyedMidFlow_AbortsAndCleansUp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kIOSPasswordAutoSubmission,
      {{"auto-submission-type", "DismissThenBlockThenSubmit"}});

  base::RunLoop focus_called_loop;
  main_frame_->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             focus_called_loop.QuitClosure());

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  focus_called_loop.Run();
  main_frame_ = nullptr;
  frames_manager_->RemoveWebFrame(web::kMainFakeFrameId);

  EXPECT_EQ(future.Get(), (AutoSubmissionState{.aborted = true,
                                               .keystroke_dispatched = false}));
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(web_view_.userInteractionEnabled);
}

// Tests that dropping an in-flight callback mid-flow sets `aborted = true`,
// skips keystroke injection, and reverts the interaction shield, renderer
// shield, and `inputmode` via `SubmitFlowContext::~SubmitFlowContext()`.
TEST_F(
    PasswordNativeKeystrokeAutologinTest,
    TriggerAutoSubmissionWithResult_CallbackDroppedMidFlow_AbortsAndCleansUp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kIOSPasswordAutoSubmission,
      {{"auto-submission-type", "DismissThenBlockThenSubmit"},
       {"auto-submission-wait-period-ms", "500"}});

  // Override `SequencedTaskRunner::GetCurrentDefault()` with a `NullTaskRunner`
  // when `setUpRendererKeystrokeShield` is called so that the delayed native
  // submit task is dropped immediately, invoking `~SubmitFlowContext()`.
  std::optional<base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting>
      null_runner_override;
  main_frame_->SetJavaScriptFunctionCallback(
      "passwords.setUpRendererKeystrokeShield",
      base::BindRepeating(
          [](std::optional<
              base::SingleThreadTaskRunner::CurrentHandleOverrideForTesting>*
                 override_holder) {
            override_holder->emplace(
                base::MakeRefCounted<base::NullTaskRunner>());
          },
          &null_runner_override));
  base::ScopedClosureRunner clear_js_callback(base::BindOnce(
      [](web::FakeWebFrame* frame) {
        frame->SetJavaScriptFunctionCallback(
            "passwords.setUpRendererKeystrokeShield", base::NullCallback());
      },
      main_frame_.get()));

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());
  EXPECT_FALSE(web_view_.userInteractionEnabled);

  ASSERT_TRUE(future.Wait());
  null_runner_override.reset();

  EXPECT_EQ(future.Get(), (AutoSubmissionState{.aborted = true,
                                               .keystroke_dispatched = false}));
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_TRUE(web_view_.userInteractionEnabled);
  EXPECT_TRUE(HasJavaScriptCallContaining(u"removeRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

// Tests that destroying the `WebState` mid-flow sets `aborted = true` and
// safely completes the flow without crashing.
TEST_F(PasswordNativeKeystrokeAutologinTest,
       TriggerAutoSubmissionWithResult_WebStateDestroyedMidFlow_AbortsSafely) {
  base::RunLoop focus_called_loop;
  main_frame_->SetJavaScriptFunctionCallback("passwords.focusElement",
                                             focus_called_loop.QuitClosure());

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  focus_called_loop.Run();
  main_frame_ = nullptr;
  frames_manager_ = nullptr;
  web_state_.reset();

  EXPECT_EQ(future.Get(), (AutoSubmissionState{.aborted = true,
                                               .keystroke_dispatched = false}));
  EXPECT_EQ(web_view_.insertedText, nil);
}

// Tests that if the first responder is outside the `WebState`'s view hierarchy
// when the native submit step runs, the Enter keystroke is not dispatched to it
// and all temporary shields and `inputmode` state are cleaned up.
TEST_F(
    PasswordNativeKeystrokeAutologinTest,
    TriggerAutoSubmissionWithResult_FirstResponderOutsideWebState_DoesNotInsertText) {
  web_view_.canBecomeFirstResponderValue = NO;
  web_view_.simulatesFirstResponder = NO;
  FakeAutologinKeyInputView* external_responder =
      [[FakeAutologinKeyInputView alloc]
          initWithFrame:CGRectMake(0, 50, 100, 40)];
  external_responder.canBecomeFirstResponderValue = YES;
  external_responder.simulatesFirstResponder = YES;
  [GetAnyKeyWindow() addSubview:external_responder];
  base::ScopedClosureRunner cleanup_external_responder(base::BindOnce(
      [](FakeAutologinKeyInputView* view) {
        view.simulatesFirstResponder = NO;
        [view removeFromSuperview];
      },
      external_responder));

  base::test::TestFuture<AutoSubmissionState> future;
  TriggerAutoSubmissionWithResult(web_state_->GetWeakPtr(),
                                  web::kMainFakeFrameId, kTestFieldId,
                                  future.GetCallback());

  EXPECT_EQ(future.Get(), AutoSubmissionState{.keystroke_dispatched = false});
  EXPECT_EQ(web_view_.insertedText, nil);
  EXPECT_EQ(external_responder.insertedText, nil);
  EXPECT_TRUE(web_view_.userInteractionEnabled);
  EXPECT_TRUE(HasJavaScriptCallContaining(u"removeRendererKeystrokeShield"));
  EXPECT_TRUE(HasJavaScriptCallContaining(u"restoreKeyboardOnElement"));
}

}  // namespace

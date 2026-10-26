// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_native_keystroke_autologin.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/scoped_observation.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/password_manager/ios/features.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace {

// Returns true if `web_state` is currently visible and its view is attached to
// a window.
bool IsWebStateVisibleInWindow(web::WebState* web_state) {
  if (!web_state || !web_state->IsVisible()) {
    return false;
  }
  UIView* view = web_state->GetView();
  return view != nil && view.window != nil;
}

// Encapsulates the state and context of an auto-submission flow.
// This object is non-copyable and non-movable and is passed via
// `std::unique_ptr` through the asynchronous sequence.
class SubmitFlowContext : public web::WebStateObserver {
 public:
  SubmitFlowContext(base::WeakPtr<web::WebState> web_state,
                    base::WeakPtr<web::WebFrame> frame,
                    autofill::FieldRendererId field_id,
                    AutoSubmissionCallback completion_handler)
      : web_state_(web_state),
        frame_(frame),
        field_id_(field_id),
        completion_handler_(std::move(completion_handler)) {
    CHECK(web_state_);
    web_state_observation_.Observe(web_state_.get());
  }

  SubmitFlowContext(const SubmitFlowContext&) = delete;
  SubmitFlowContext& operator=(const SubmitFlowContext&) = delete;
  SubmitFlowContext(SubmitFlowContext&&) = delete;
  SubmitFlowContext& operator=(SubmitFlowContext&&) = delete;
  // Ensures cleanup still runs if `SubmitFlowContext` is destroyed.
  ~SubmitFlowContext() override {
    if (!done_) {
      state_.aborted = true;
      std::move(*this).Finish();
    }
  }

  // web::WebStateObserver:
  void WasHidden(web::WebState* web_state) override { state_.aborted = true; }

  void WebStateDestroyed(web::WebState* web_state) override {
    state_.aborted = true;
    web_state_observation_.Reset();
    web_state_ = nullptr;
    frame_ = nullptr;
  }

  // Returns true if the flow has not been aborted (e.g., by the `WebState`
  // being hidden or destroyed mid-flow), the target `WebFrame` still exists,
  // and the `WebState` is still visible and attached to a window. Latches
  // `state_.aborted = true` if the flow can no longer proceed.
  bool CanProceed() {
    if (state_.aborted) {
      return false;
    }
    if (!frame_ || !IsWebStateVisibleInWindow(web_state_.get())) {
      state_.aborted = true;
      return false;
    }
    return true;
  }

  // Finish the flow by doing Cleanup & Completion. Last step in the sequence
  // regardless of the auto-submission process outcome (success or failure). Can
  // only be called once.
  void Finish() && {
    CHECK(!done_);
    done_ = true;
    web_state_observation_.Reset();

    if (web_state_) {
      if (shield_raised_) {
        if (UIView* web_view = web_state_->GetView()) {
          web_view.userInteractionEnabled = YES;
        }
      }

      if (renderer_shield_raised_ && frame_) {
        password_manager::PasswordManagerJavaScriptFeature::GetInstance()
            ->RemoveRendererKeystrokeShield(frame_.get(), base::DoNothing());
      }

      if (input_mode_changed_ && frame_) {
        password_manager::PasswordManagerJavaScriptFeature::GetInstance()
            ->RestoreKeyboardOnElement(frame_.get(), field_id_,
                                       base::DoNothing());
      }
    }

    std::move(completion_handler_).Run(state_);
  }

  // --- Getters & Setters ---
  base::WeakPtr<web::WebState> web_state() const { return web_state_; }
  base::WeakPtr<web::WebFrame> frame() const { return frame_; }
  autofill::FieldRendererId field_id() const { return field_id_; }

  bool shield_raised() const { return shield_raised_; }
  void set_shield_raised(bool raised) { shield_raised_ = raised; }

  bool renderer_shield_raised() const { return renderer_shield_raised_; }
  void set_renderer_shield_raised(bool raised) {
    renderer_shield_raised_ = raised;
  }

  bool input_mode_changed() const { return input_mode_changed_; }
  void set_input_mode_changed(bool changed) { input_mode_changed_ = changed; }

  void set_keystroke_dispatched(bool keystroke_dispatched) {
    state_.keystroke_dispatched = keystroke_dispatched;
  }

 private:
  // --- Context (Dependencies) ---
  base::WeakPtr<web::WebState> web_state_;
  base::WeakPtr<web::WebFrame> frame_;
  autofill::FieldRendererId field_id_;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // --- State (Progress) ---
  // True if the interaction shield over the web view was raised.
  bool shield_raised_ = false;
  // True if the transparent shield in the renderer was set up.
  bool renderer_shield_raised_ = false;
  // True if `inputmode` was successfully changed on the element.
  bool input_mode_changed_ = false;
  AutoSubmissionState state_;
  // The handler to execute when the flow completes or fails.
  AutoSubmissionCallback completion_handler_;

  // True if the flow is done.
  bool done_ = false;
};

// Type alias for a step in the auto-submission sequence. The step passes
// the flow_context object to the next step.
using StepCallback =
    base::OnceCallback<void(std::unique_ptr<SubmitFlowContext>)>;

// Triggers submit via native keystroke on the first responder.
void RunNativeSubmit(std::unique_ptr<SubmitFlowContext> flow_context) {
  if (flow_context->CanProceed()) {
    UIView* web_view = flow_context->web_state()->GetView();
    UIResponder* first_responder = GetFirstResponder();

    // Ensure the first responder is a UIView and is part of the WebState's
    // view hierarchy. This prevents keystrokes from bleeding into the
    // omnibox or other native Chrome UI elements if the first responder
    // abruptly changed.
    if ([first_responder isKindOfClass:[UIView class]] &&
        [(UIView*)first_responder isDescendantOfView:web_view]) {
      if ([first_responder conformsToProtocol:@protocol(UIKeyInput)]) {
        [(id<UIKeyInput>)first_responder insertText:@"\n"];
        flow_context->set_keystroke_dispatched(true);
      }
    }
  }

  // Finish the flow as this is the last step in the flow.
  std::move(*flow_context).Finish();
}

// Step Factory: Interaction Shield.
// Disables user interaction on the web view.
// [Conditional]: Only if the variant is `kDismissThenBlockThenSubmit`.
StepCallback CreateInteractionShieldStep(StepCallback next_step) {
  if (password_manager::features::kAutoSubmissionTypeParam.Get() !=
      password_manager::features::AutoSubmissionType::
          kDismissThenBlockThenSubmit) {
    return next_step;
  }

  auto step_logic = [](StepCallback next_step,
                       std::unique_ptr<SubmitFlowContext> flow_context) {
    if (!flow_context->CanProceed()) {
      std::move(*flow_context).Finish();
      return;
    }
    if (UIView* web_view = flow_context->web_state()->GetView()) {
      web_view.userInteractionEnabled = NO;
      flow_context->set_shield_raised(true);
    }
    if (!flow_context->shield_raised()) {
      // Terminate flow if the shield could not be raised.
      std::move(*flow_context).Finish();
      return;
    }
    std::move(next_step).Run(std::move(flow_context));
  };

  return base::BindOnce(std::move(step_logic), std::move(next_step));
}

// Step Factory: Prevent Keyboard.
// Sets `inputmode="none"` on the target element to prevent the keyboard from
// showing.
// [Conditional]: Only if `should_prevent_keyboard` is `true`.
StepCallback CreatePreventKeyboardStep(StepCallback next_step,
                                       bool should_prevent_keyboard) {
  if (!should_prevent_keyboard) {
    return next_step;
  }

  auto step_logic = [](StepCallback next_step,
                       std::unique_ptr<SubmitFlowContext> flow_context) {
    if (!flow_context->CanProceed()) {
      std::move(*flow_context).Finish();
      return;
    }
    web::WebFrame* frame_ptr = flow_context->frame().get();

    auto js_callback = [](StepCallback next_step,
                          std::unique_ptr<SubmitFlowContext> flow_context,
                          BOOL success) {
      if (success) {
        flow_context->set_input_mode_changed(true);
        if (flow_context->CanProceed()) {
          std::move(next_step).Run(std::move(flow_context));
          return;
        }
      }
      // Abandon flow if the operation wasn't a success or the tab was hidden.
      std::move(*flow_context).Finish();
    };

    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->PreventKeyboardOnElement(
            frame_ptr, flow_context->field_id(),
            base::BindOnce(std::move(js_callback), std::move(next_step),
                           std::move(flow_context)));
  };

  return base::BindOnce(std::move(step_logic), std::move(next_step));
}

// Step Factory: Dismiss Keyboard.
// Call `endEditing:YES` in case dismissing the bottom sheet retriggers a focus
// event.
// [Conditional]: Skipped for the `kSubmitThenDismiss` variant.
StepCallback CreateDismissKeyboardStep(StepCallback next_step) {
  auto step_logic = [](StepCallback next_step,
                       std::unique_ptr<SubmitFlowContext> flow_context) {
    if (!flow_context->CanProceed()) {
      // Terminate flow if the web state is gone or no longer in the foreground.
      std::move(*flow_context).Finish();
      return;
    }

    if (password_manager::features::kAutoSubmissionTypeParam.Get() !=
        password_manager::features::AutoSubmissionType::kSubmitThenDismiss) {
      // Hide keyboard to prevent WebKit from restoring focus.
      [flow_context->web_state()->GetView() endEditing:YES];
    }
    std::move(next_step).Run(std::move(flow_context));
  };

  return base::BindOnce(std::move(step_logic), std::move(next_step));
}

// Step Factory: Focus Element.
// Focuses the target element (silently, due to `inputmode` or caller-level
// keyboard suppression) so it becomes the active element.
StepCallback CreateFocusStep(StepCallback next_step) {
  auto step_logic = [](StepCallback next_step,
                       std::unique_ptr<SubmitFlowContext> flow_context) {
    if (!flow_context->CanProceed()) {
      std::move(*flow_context).Finish();
      return;
    }
    web::WebFrame* frame_ptr = flow_context->frame().get();

    auto js_callback = [](StepCallback next_step,
                          std::unique_ptr<SubmitFlowContext> flow_context,
                          BOOL success) {
      if (success && flow_context->CanProceed()) {
        std::move(next_step).Run(std::move(flow_context));
        return;
      }
      // Terminate flow if the operation wasn't a success or the tab was hidden.
      std::move(*flow_context).Finish();
    };

    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->FocusElement(
            frame_ptr, flow_context->field_id(),
            base::BindOnce(std::move(js_callback), std::move(next_step),
                           std::move(flow_context)));
  };

  return base::BindOnce(std::move(step_logic), std::move(next_step));
}

// Step Factory: Renderer Keystroke Shield.
// Set up a transparent shield in the renderer to intercept and prevent stray
// user interactions.
StepCallback CreateRendererShieldStep(StepCallback next_step) {
  auto step_logic = [](StepCallback next_step,
                       std::unique_ptr<SubmitFlowContext> flow_context) {
    if (!flow_context->CanProceed()) {
      std::move(*flow_context).Finish();
      return;
    }
    web::WebFrame* frame_ptr = flow_context->frame().get();

    auto js_callback = [](StepCallback next_step,
                          std::unique_ptr<SubmitFlowContext> flow_context,
                          BOOL success) {
      if (success) {
        flow_context->set_renderer_shield_raised(true);
        if (flow_context->CanProceed()) {
          std::move(next_step).Run(std::move(flow_context));
          return;
        }
      }
      std::move(*flow_context).Finish();
    };

    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->SetUpRendererKeystrokeShield(
            frame_ptr, flow_context->field_id(),
            base::BindOnce(std::move(js_callback), std::move(next_step),
                           std::move(flow_context)));
  };

  return base::BindOnce(std::move(step_logic), std::move(next_step));
}

// Step Factory: Native Submit (delayed or direct).
// Ensure the web view is the first responder and inject the native
// `insertText:@"\n"` keystroke to submit.
// [Conditional]: If the `auto-submission-use-wait-period` parameter is true,
// wait a short delay before injecting the keystroke to ensure focus has fully
// propagated.
StepCallback CreateNativeSubmitStep() {
  StepCallback submit_and_restore = base::BindOnce(&RunNativeSubmit);

  int wait_period_ms =
      password_manager::features::kAutoSubmissionWaitPeriodMsParam.Get();

  if (wait_period_ms > 0) {
    auto delayed_logic = [](StepCallback task, base::TimeDelta delay,
                            std::unique_ptr<SubmitFlowContext> flow_context) {
      if (!flow_context->CanProceed()) {
        // Terminate flow if the web state is gone or no longer in the
        // foreground.
        std::move(*flow_context).Finish();
        return;
      }
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(std::move(task), std::move(flow_context)),
          delay);
    };

    return base::BindOnce(std::move(delayed_logic),
                          std::move(submit_and_restore),
                          base::Milliseconds(wait_period_ms));
  }

  // If not using a delay to wait on the first responder to be settled force
  // first responder.
  auto force_first_responder =
      [](StepCallback task, std::unique_ptr<SubmitFlowContext> flow_context) {
        if (!flow_context->CanProceed()) {
          std::move(*flow_context).Finish();
          return;
        }
        if (UIView* web_view = flow_context->web_state()->GetView()) {
          // Make sure the web view is the first responder so
          // it can receive the keystroke.
          [web_view becomeFirstResponder];
        }
        std::move(task).Run(std::move(flow_context));
      };

  return base::BindOnce(std::move(force_first_responder),
                        std::move(submit_and_restore));
}

}  // namespace

void TriggerAutoSubmission(base::WeakPtr<web::WebState> weak_web_state,
                           const std::string& frame_id,
                           autofill::FieldRendererId field_id,
                           base::OnceClosure completion_handler) {
  CHECK(completion_handler);
  TriggerAutoSubmissionWithResult(
      std::move(weak_web_state), frame_id, field_id,
      base::IgnoreArgs<AutoSubmissionState>(std::move(completion_handler)));
}

// Triggers the auto-submission with the following flow:
//
// 1. Raise Interaction Shield: Disables user interaction on the web view.
//    [Conditional]: Only if the variant is `kDismissThenBlockThenSubmit`.
//
// 2. Prevent Keyboard: Sets `inputmode="none"` on the target element to
//    prevent the keyboard from showing.
//    [Conditional]: Only if `should_prevent_keyboard` is `true`.
//
// 3. Dismiss Keyboard: Calls `endEditing:YES` in case dismissing the bottom
//    sheet retriggers a focus event.
//    [Conditional]: Skipped for the `kSubmitThenDismiss` variant.
//
// 4. Focus Element: Focuses the target element (silently, due to `inputmode` or
//    caller-level keyboard suppression) so it becomes the active element.
//
// 5. Renderer Keystroke Shield: Sets up a transparent shield in the renderer
//    to intercept and prevent stray user interactions.
//
// 6. Trigger Native Submit: Ensures the web view is the first responder and
//    injects the native `insertText:@"\n"` keystroke to submit.
//    [Conditional]: If the `auto-submission-use-wait-period` parameter is
//    `true`, waits a short delay before injecting the keystroke to ensure focus
//    has fully propagated.
//
// 7. Unified Cleanup & Completion: Restores the original `inputmode` and blurs
//    the target element (if `inputmode` was changed), removes the renderer
//    keystroke shield (if raised), lowers the interaction shield (if raised),
//    and invokes `completion_handler` with the final `AutoSubmissionState`.
void TriggerAutoSubmissionWithResult(
    base::WeakPtr<web::WebState> weak_web_state,
    const std::string& frame_id,
    autofill::FieldRendererId field_id,
    AutoSubmissionCallback completion_handler,
    bool should_prevent_keyboard) {
  CHECK(completion_handler);

  if (!IsWebStateVisibleInWindow(weak_web_state.get()) || field_id.is_null()) {
    std::move(completion_handler).Run(AutoSubmissionState{});
    return;
  }

  UIView* view = weak_web_state->GetView();
  password_manager::PasswordManagerJavaScriptFeature* feature =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance();
  web::WebFramesManager* manager =
      feature->GetWebFramesManager(weak_web_state.get());
  web::WebFrame* frame = manager ? manager->GetFrameWithId(frame_id) : nullptr;

  if (!frame) {
    // Fallback if frame is gone. Hide keyboard on failure.
    [view endEditing:YES];
    std::move(completion_handler).Run(AutoSubmissionState{});
    return;
  }

  auto flow_context = std::make_unique<SubmitFlowContext>(
      std::move(weak_web_state), frame->AsWeakPtr(), field_id,
      std::move(completion_handler));

  // Build the chain for the flow in reverse order of execution.
  StepCallback step6 = CreateNativeSubmitStep();
  StepCallback step5 = CreateRendererShieldStep(std::move(step6));
  StepCallback step4 = CreateFocusStep(std::move(step5));
  StepCallback step3 = CreateDismissKeyboardStep(std::move(step4));
  StepCallback step2 =
      CreatePreventKeyboardStep(std::move(step3), should_prevent_keyboard);
  StepCallback step1 = CreateInteractionShieldStep(std::move(step2));

  // Kick off the flow.
  std::move(step1).Run(std::move(flow_context));
}

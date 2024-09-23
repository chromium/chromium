// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eyedropper/eye_dropper.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_color_selection_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_color_selection_result.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/scoped_abort_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class EyeDropper::OpenAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  OpenAbortAlgorithm(EyeDropper* eyedropper, AbortSignal* signal)
      : eyedropper_(eyedropper), abortsignal_(signal) {}
  ~OpenAbortAlgorithm() override = default;

  void Run() override { eyedropper_->AbortCallback(abortsignal_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(eyedropper_);
    visitor->Trace(abortsignal_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<EyeDropper> eyedropper_;
  Member<AbortSignal> abortsignal_;
};

constexpr char kNotAvailableMessage[] = "EyeDropper is not available.";

EyeDropper::EyeDropper(ExecutionContext* context)
    : eye_dropper_chooser_(context) {}

EyeDropper* EyeDropper::Create(ExecutionContext* context) {
  return MakeGarbageCollected<EyeDropper>(context);
}

ScriptPromise<ColorSelectionResult> EyeDropper::open(
    ScriptState* script_state,
    const ColorSelectionOptions* options,
    ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::EyeDropperAPIEnabled());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated with a window.");
    return EmptyPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "EyeDropper::open() requires user gesture.");
    return EmptyPromise();
  }

  if (!::features::IsEyeDropperEnabled()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      kNotAvailableMessage);
    return EmptyPromise();
  }

  if (eye_dropper_chooser_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "EyeDropper is already open.");
    return EmptyPromise();
  }

  std::unique_ptr<ScopedAbortState> end_chooser_abort_state = nullptr;
  std::unique_ptr<ScopedAbortState> response_handler_abort_state = nullptr;
  if (auto* signal = options->getSignalOr(nullptr)) {
    if (signal->aborted()) {
      return ScriptPromise<ColorSelectionResult>::Reject(
          script_state, signal->reason(script_state));
    }
    auto* handle = signal->AddAlgorithm(
        MakeGarbageCollected<OpenAbortAlgorithm>(this, signal));
    end_chooser_abort_state =
        std::make_unique<ScopedAbortState>(signal, handle);
    response_handler_abort_state =
        std::make_unique<ScopedAbortState>(signal, handle);
  }

  resolver_ = MakeGarbageCollected<ScriptPromiseResolver<ColorSelectionResult>>(
      script_state, exception_state.GetContext());
  auto promise = resolver_->Promise();

  auto* frame = window->GetFrame();
  frame->GetBrowserInterfaceBroker().GetInterface(
      eye_dropper_chooser_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
  eye_dropper_chooser_.set_disconnect_handler(
      WTF::BindOnce(&EyeDropper::EndChooser, WrapWeakPersistent(this),
                    std::move(end_chooser_abort_state)));
  eye_dropper_chooser_->Choose(
      resolver_->WrapCallbackInScriptScope(WTF::BindOnce(
          &EyeDropper::EyeDropperResponseHandler, WrapPersistent(this),
          std::move(response_handler_abort_state))));
  return promise;
}

void EyeDropper::AbortCallback(AbortSignal* signal) {
  if (resolver_) {
    ScriptState* script_state = resolver_->GetScriptState();
    if (IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                      script_state)) {
      ScriptState::Scope script_state_scope(script_state);
      resolver_->Reject(signal->reason(script_state));
    }
  }

  eye_dropper_chooser_.reset();
  resolver_ = nullptr;
}

void EyeDropper::EyeDropperResponseHandler(
    std::unique_ptr<ScopedAbortState> scoped_abort_state,
    ScriptPromiseResolver<ColorSelectionResult>* resolver,
    bool success,
    uint32_t color) {
  eye_dropper_chooser_.reset();

  // The abort callback resets the Mojo remote if an abort is signalled,
  // so by receiving a reply, the eye dropper operation must *not* have
  // been aborted by the abort signal. Thus, the promise is not yet resolved,
  // so resolver_ must be non-null.
  DCHECK_EQ(resolver_, resolver);

  if (success) {
    ColorSelectionResult* result = ColorSelectionResult::Create();
    // TODO(https://1351544): The EyeDropper should return a Color or an
    // SkColor4f, instead of an SkColor.
    result->setSRGBHex(Color::FromRGBA32(color).SerializeAsCanvasColor());
    resolver->Resolve(result);
    resolver_ = nullptr;
  } else {
    RejectPromiseHelper(DOMExceptionCode::kAbortError,
                        "The user canceled the selection.");
  }
}

void EyeDropper::EndChooser(
    std::unique_ptr<ScopedAbortState> scoped_abort_state) {
  eye_dropper_chooser_.reset();

  if (!resolver_ ||
      !IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_->GetScriptState())) {
    return;
  }

  ScriptState::Scope script_state_scope(resolver_->GetScriptState());

  RejectPromiseHelper(DOMExceptionCode::kOperationError, kNotAvailableMessage);
}

void EyeDropper::RejectPromiseHelper(DOMExceptionCode exception_code,
                                     const WTF::String& message) {
  resolver_->RejectWithDOMException(exception_code, message);
  resolver_ = nullptr;
}

void EyeDropper::Trace(Visitor* visitor) const {
  visitor->Trace(eye_dropper_chooser_);
  visitor->Trace(resolver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

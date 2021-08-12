// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/eyedropper/eye_dropper.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_color_selection_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

EyeDropper::EyeDropper(ExecutionContext* context)
    : eye_dropper_chooser_(context) {}

EyeDropper* EyeDropper::Create(ExecutionContext* context) {
  return MakeGarbageCollected<EyeDropper>(context);
}

ScriptPromise EyeDropper::open(ScriptState* script_state,
                               ExceptionState& exception_state) {
  DCHECK(RuntimeEnabledFeatures::EyeDropperAPIEnabled());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated with a window.");
    return ScriptPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "EyeDropper::open() requires user gesture.");
    return ScriptPromise();
  }

  if (eye_dropper_chooser_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "EyeDropper is already open.");
    return ScriptPromise();
  }

  auto* resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver_->Promise();

  auto* frame = window->GetFrame();
  frame->GetBrowserInterfaceBroker().GetInterface(
      eye_dropper_chooser_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
  eye_dropper_chooser_.set_disconnect_handler(
      WTF::Bind(&EyeDropper::EndChooser, WrapWeakPersistent(this)));
  eye_dropper_chooser_->Choose(WTF::Bind(&EyeDropper::EyeDropperResponseHandler,
                                         WrapPersistent(this),
                                         WrapPersistent(resolver_)));

  return promise;
}

void EyeDropper::EyeDropperResponseHandler(ScriptPromiseResolver* resolver,
                                           bool success,
                                           uint32_t color) {
  eye_dropper_chooser_.reset();

  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (success) {
    ColorSelectionResult* result = ColorSelectionResult::Create();
    result->setSRGBHex(Color(color).Serialized());
    resolver->Resolve(result);
  } else {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "The user canceled the selection."));
  }
}

void EyeDropper::EndChooser() {
  eye_dropper_chooser_.reset();
  if (resolver_) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "EyeDropper is not available."));
    resolver_ = nullptr;
  }
}

void EyeDropper::Trace(Visitor* visitor) const {
  visitor->Trace(eye_dropper_chooser_);
  visitor->Trace(resolver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_enumeration/window_screens.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/screen_enumeration/screens.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
const char WindowScreens::kSupplementName[] = "WindowScreens";

WindowScreens::WindowScreens(LocalDOMWindow* window)
    : ExecutionContextLifecycleObserver(window),
      Supplement<LocalDOMWindow>(*window),
      permission_service_(window) {}

// static
ScriptPromise WindowScreens::getScreens(ScriptState* script_state,
                                        LocalDOMWindow& window,
                                        ExceptionState& exception_state) {
  return From(&window)->GetScreens(script_state, exception_state);
}

void WindowScreens::ContextDestroyed() {
  screens_.Clear();
}

void WindowScreens::Trace(Visitor* visitor) const {
  visitor->Trace(screens_);
  visitor->Trace(permission_service_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
WindowScreens* WindowScreens::From(LocalDOMWindow* window) {
  auto* supplement = Supplement<LocalDOMWindow>::From<WindowScreens>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowScreens>(window);
    Supplement<LocalDOMWindow>::ProvideTo(*window, supplement);
  }
  return supplement;
}

ScriptPromise WindowScreens::GetScreens(ScriptState* script_state,
                                        ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsSecureContext());  // [SecureContext] in IDL.
  if (!permission_service_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    ConnectToPermissionService(
        context, permission_service_.BindNewPipeAndPassReceiver(
                     context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  permission_service_->RequestPermission(
      CreatePermissionDescriptor(
          mojom::blink::PermissionName::WINDOW_PLACEMENT),
      LocalFrame::HasTransientUserActivation(GetSupplementable()->GetFrame()),
      WTF::Bind(&WindowScreens::OnPermissionRequestComplete,
                WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void WindowScreens::OnPermissionRequestComplete(
    ScriptPromiseResolver* resolver,
    mojom::blink::PermissionStatus status) {
  if (!resolver->GetScriptState()->ContextIsValid())
    return;
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    auto* const isolate = resolver->GetScriptState()->GetIsolate();
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        isolate, DOMExceptionCode::kNotAllowedError, "Permission denied."));
    return;
  }

  if (!screens_)
    screens_ = MakeGarbageCollected<Screens>(GetSupplementable());
  resolver->Resolve(screens_);
}

}  // namespace blink

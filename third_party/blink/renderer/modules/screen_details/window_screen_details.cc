// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_details/window_screen_details.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/screen_details/screen_details.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
const char WindowScreenDetails::kSupplementName[] = "WindowScreenDetails";

WindowScreenDetails::WindowScreenDetails(LocalDOMWindow* window)
    : ExecutionContextLifecycleObserver(window),
      Supplement<LocalDOMWindow>(*window),
      permission_service_(window) {}

// static
ScriptPromise WindowScreenDetails::getScreenDetails(
    ScriptState* script_state,
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  return From(&window)->GetScreenDetails(script_state, exception_state);
}

void WindowScreenDetails::ContextDestroyed() {
  screen_details_.Clear();
}

void WindowScreenDetails::Trace(Visitor* visitor) const {
  visitor->Trace(screen_details_);
  visitor->Trace(permission_service_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
WindowScreenDetails* WindowScreenDetails::From(LocalDOMWindow* window) {
  auto* supplement =
      Supplement<LocalDOMWindow>::From<WindowScreenDetails>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowScreenDetails>(window);
    Supplement<LocalDOMWindow>::ProvideTo(*window, supplement);
  }
  return supplement;
}

ScriptPromise WindowScreenDetails::GetScreenDetails(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DCHECK(window->IsSecureContext());  // [SecureContext] in IDL.
  if (!permission_service_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    ConnectToPermissionService(
        window, permission_service_.BindNewPipeAndPassReceiver(
                    window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  auto permission_descriptor = CreatePermissionDescriptor(
      mojom::blink::PermissionName::WINDOW_MANAGEMENT);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  auto callback =
      WTF::BindOnce(&WindowScreenDetails::OnPermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver));

  // Only allow the user prompts when the frame has a transient activation.
  // Otherwise, resolve or reject the promise with the current permission state.
  // This allows sites with permission already granted to obtain screen info
  // when the document loads, to populate multi-screen UI.
  if (LocalFrame::HasTransientUserActivation(GetSupplementable()->GetFrame())) {
    permission_service_->RequestPermission(std::move(permission_descriptor),
                                           /*user_gesture=*/true,
                                           std::move(callback));
  } else {
    permission_service_->HasPermission(std::move(permission_descriptor),
                                       std::move(callback));
  }

  return resolver->Promise();
}

void WindowScreenDetails::OnPermissionRequestComplete(
    ScriptPromiseResolver* resolver,
    mojom::blink::PermissionStatus status) {
  if (!resolver->GetScriptState()->ContextIsValid())
    return;
  if (status != mojom::blink::PermissionStatus::GRANTED) {
    auto* const isolate = resolver->GetScriptState()->GetIsolate();
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        isolate, DOMExceptionCode::kNotAllowedError,
        status == mojom::blink::PermissionStatus::DENIED
            ? "Permission denied."
            : "Permission decision deferred."));
    return;
  }

  if (!screen_details_)
    screen_details_ = MakeGarbageCollected<ScreenDetails>(GetSupplementable());
  resolver->Resolve(screen_details_);
}

}  // namespace blink

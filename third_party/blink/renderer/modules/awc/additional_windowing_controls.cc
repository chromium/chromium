// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/awc/additional_windowing_controls.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

namespace {

using AdditionalWindowingControlsActionCallback =
    base::OnceCallback<void(mojom::blink::PermissionStatus)>;
using ui::mojom::blink::WindowShowState;

bool IsPermissionGranted(ScriptPromiseResolver<IDLUndefined>* resolver,
                         mojom::blink::PermissionStatus status) {
  if (!resolver->GetScriptState()->ContextIsValid()) {
    return false;
  }

  if (status != mojom::blink::PermissionStatus::GRANTED) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kNotAllowedError,
        status == mojom::blink::PermissionStatus::DENIED
            ? "Permission denied."
            : "Permission decision deferred.");
    return false;
  }
  return true;
}

bool CanUseWindowingControls(LocalDOMWindow* window,
                             ExceptionState& exception_state) {
  auto* frame = window->GetFrame();
  if (!frame || !frame->IsOutermostMainFrame() ||
      frame->GetPage()->IsPrerendering()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in primary top-level browsing contexts.");
    return false;
  }
  return true;
}

bool IsMaximized(LocalDOMWindow* window) {
  return window->GetFrame()->GetWidgetForLocalRoot()->WindowShowState() ==
         WindowShowState::kMaximized;
}

bool IsMinimized(LocalDOMWindow* window) {
  return window->GetFrame()->GetWidgetForLocalRoot()->WindowShowState() ==
         WindowShowState::kMinimized;
}

bool IsNormal(LocalDOMWindow* window) {
  WindowShowState show_state =
      window->GetFrame()->GetWidgetForLocalRoot()->WindowShowState();
  return show_state == WindowShowState::kDefault ||
         show_state == WindowShowState::kNormal;
}

ScriptPromise<IDLUndefined> MaybePromptWindowManagementPermission(
    LocalDOMWindow* window,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    AdditionalWindowingControlsActionCallback callback) {
  auto* permission_service =
      window->document()->GetPermissionService(window->GetExecutionContext());
  CHECK(permission_service);

  auto permission_descriptor = PermissionDescriptor::New();
  permission_descriptor->name = PermissionName::WINDOW_MANAGEMENT;

  // Only allow the user prompts when the frame has a transient activation.
  // Otherwise, resolve or reject the promise with the current permission state.
  if (LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    LocalFrame::ConsumeTransientUserActivation(window->GetFrame());
    permission_service->RequestPermission(std::move(permission_descriptor),
                                          /*user_gesture=*/true,
                                          std::move(callback));
  } else {
    permission_service->HasPermission(std::move(permission_descriptor),
                                      std::move(callback));
  }

  return resolver->Promise();
}

base::OnceCallback<void(bool)> GetWindowEventCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    String error_message) {
  return blink::BindOnce(
      [](ScriptPromiseResolver<IDLUndefined>* resolver,
         const String& error_message, bool succeeded) {
        if (succeeded) {
          resolver->Resolve();
        } else {
          resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                           error_message);
        }
      },
      WrapPersistent(resolver), std::move(error_message));
}

base::OnceCallback<void(bool)> GetMaximizeCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  return GetWindowEventCallback(resolver, "Could not maximize the window.");
}

base::OnceCallback<void(bool)> GetMinimizeCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  return GetWindowEventCallback(resolver, "Could not minimize the window.");
}

base::OnceCallback<void(bool)> GetRestoreCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  return GetWindowEventCallback(resolver, "Could not restore the window.");
}

void OnMaximizePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

  if (IsMaximized(window)) {
    resolver->Resolve();
  } else {
    window->GetFrame()->GetChromeClient().Maximize(
        *window->GetFrame(), GetMaximizeCallback(resolver));
  }
}

void OnMinimizePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

  if (IsMinimized(window)) {
    resolver->Resolve();
  } else {
    window->GetFrame()->GetChromeClient().Minimize(
        *window->GetFrame(), GetMinimizeCallback(resolver));
  }
}

void OnRestorePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

  if (IsNormal(window)) {
    resolver->Resolve();
  } else {
    window->GetFrame()->GetChromeClient().Restore(*window->GetFrame(),
                                                  GetRestoreCallback(resolver));
  }
}

void OnSetResizablePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    bool resizable,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

  ChromeClient& chrome_client = window->GetFrame()->GetChromeClient();
  chrome_client.SetResizable(resizable, *window->GetFrame());

  // TODO(crbug.com/1505666): Add wait for the resizability change to be
  // completed before resolving the promise.

  resolver->Resolve();
}

}  // namespace

// static
ScriptPromise<IDLUndefined> AdditionalWindowingControls::maximize(
    ScriptState* script_state,
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  if (!CanUseWindowingControls(&window, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  return MaybePromptWindowManagementPermission(
      &window, resolver,
      BindOnce(&OnMaximizePermissionRequestComplete, WrapPersistent(resolver),
               WrapPersistent(&window)));
}

// static
ScriptPromise<IDLUndefined> AdditionalWindowingControls::minimize(
    ScriptState* script_state,
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  if (!CanUseWindowingControls(&window, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  return MaybePromptWindowManagementPermission(
      &window, resolver,
      BindOnce(&OnMinimizePermissionRequestComplete, WrapPersistent(resolver),
               WrapPersistent(&window)));
}

// static
ScriptPromise<IDLUndefined> AdditionalWindowingControls::restore(
    ScriptState* script_state,
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  if (!CanUseWindowingControls(&window, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  return MaybePromptWindowManagementPermission(
      &window, resolver,
      BindOnce(&OnRestorePermissionRequestComplete, WrapPersistent(resolver),
               WrapPersistent(&window)));
}

// static
ScriptPromise<IDLUndefined> AdditionalWindowingControls::setResizable(
    ScriptState* script_state,
    LocalDOMWindow& window,
    bool resizable,
    ExceptionState& exception_state) {
  if (!CanUseWindowingControls(&window, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  return MaybePromptWindowManagementPermission(
      &window, resolver,
      BindOnce(&OnSetResizablePermissionRequestComplete,
               WrapPersistent(resolver), WrapPersistent(&window), resizable));
}

}  // namespace blink

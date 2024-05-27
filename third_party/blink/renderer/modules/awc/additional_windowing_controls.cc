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

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

namespace {

using AdditionalWindowingControlsActionCallback =
    base::OnceCallback<void(mojom::blink::PermissionStatus)>;

bool IsPermissionGranted(ScriptPromiseResolver<IDLUndefined>* resolver,
                         mojom::blink::PermissionStatus status) {
  if (!resolver->GetScriptState()->ContextIsValid()) {
    return false;
  }

  if (status != mojom::blink::PermissionStatus::GRANTED) {
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        resolver->GetScriptState()->GetIsolate(),
        DOMExceptionCode::kNotAllowedError,
        status == mojom::blink::PermissionStatus::DENIED
            ? "Permission denied."
            : "Permission decision deferred."));
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

// Additional windowing controls (AWC) is a desktop-only feature.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "API is only supported on Desktop platforms. This excludes mobile "
      "platforms.");
  return false;
#else
  return true;
#endif
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

void OnMaximizePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  window->GetFrame()->GetChromeClient().Maximize(*window->GetFrame());
#endif

  // TODO(crbug.com/1505666): Add wait for the display state change to be
  // completed before resolving the promise.

  resolver->Resolve();
}

void OnMinimizePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  window->GetFrame()->GetChromeClient().Minimize(*window->GetFrame());
#endif

  // TODO(crbug.com/1505666): Add wait for the display state change to be
  // completed before resolving the promise.

  resolver->Resolve();
}

void OnRestorePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  window->GetFrame()->GetChromeClient().Restore(*window->GetFrame());
#endif

  // TODO(crbug.com/1505666): Add wait for the display state change to be
  // completed before resolving the promise.

  resolver->Resolve();
}

void OnSetResizablePermissionRequestComplete(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    LocalDOMWindow* window,
    bool resizable,
    mojom::blink::PermissionStatus status) {
  if (!IsPermissionGranted(resolver, status)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  ChromeClient& chrome_client = window->GetFrame()->GetChromeClient();
  chrome_client.SetResizable(resizable, *window->GetFrame());
#endif

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
      WTF::BindOnce(&OnMaximizePermissionRequestComplete,
                    WrapPersistent(resolver), WrapPersistent(&window)));
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
      WTF::BindOnce(&OnMinimizePermissionRequestComplete,
                    WrapPersistent(resolver), WrapPersistent(&window)));
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
      WTF::BindOnce(&OnRestorePermissionRequestComplete,
                    WrapPersistent(resolver), WrapPersistent(&window)));
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
      WTF::BindOnce(&OnSetResizablePermissionRequestComplete,
                    WrapPersistent(resolver), WrapPersistent(&window),
                    resizable));
}

}  // namespace blink

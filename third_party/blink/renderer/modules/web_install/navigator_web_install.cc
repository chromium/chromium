// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/web_install/navigator_web_install.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_install_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

const char NavigatorWebInstall::kSupplementName[] = "NavigatorWebInstall";

void OnInstallResponse(ScriptPromiseResolver<WebInstallResult>* resolver,
                       mojom::blink::WebInstallServiceResult result,
                       const KURL& manifest_id) {
  if (result != mojom::blink::WebInstallServiceResult::kSuccess) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
    return;
  }

  WebInstallResult* blink_result = WebInstallResult::Create();
  blink_result->setManifestId(manifest_id.GetString());
  resolver->Resolve(std::move(blink_result));
}

NavigatorWebInstall::NavigatorWebInstall(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      service_(navigator.GetExecutionContext()) {}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    const String& manifest_id,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallImpl(
      script_state, manifest_id, exception_state);
}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    const String& manifest_id,
    const String& install_url,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallImpl(
      script_state, manifest_id, install_url, exception_state);
}

ScriptPromise<WebInstallResult> NavigatorWebInstall::InstallImpl(
    ScriptState* script_state,
    const String& manifest_id,
    ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(script_state, exception_state)) {
    return ScriptPromise<WebInstallResult>();
  }

  auto* frame = GetSupplementable()->DomWindow()->GetFrame();
  if (!LocalFrame::ConsumeTransientUserActivation(frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Unable to install app. This API can only be called shortly after a "
        "user activation.");
    return ScriptPromise<WebInstallResult>();
  }

  KURL resolved_id = ResolveManifestId(manifest_id, exception_state);
  if (!resolved_id.IsValid()) {
    return ScriptPromise<WebInstallResult>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebInstallResult>>(
          script_state);
  ScriptPromise<WebInstallResult> promise = resolver->Promise();

  CHECK(GetService());
  GetService()->InstallCurrentDocument(
      resolved_id,
      WTF::BindOnce(&blink::OnInstallResponse, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<WebInstallResult> NavigatorWebInstall::InstallImpl(
    ScriptState* script_state,
    const String& manifest_id,
    const String& install_url,
    ExceptionState& exception_state) {
  if (!CheckPreconditionsMaybeThrow(script_state, exception_state)) {
    return ScriptPromise<WebInstallResult>();
  }

  auto* frame = GetSupplementable()->DomWindow()->GetFrame();
  if (!LocalFrame::ConsumeTransientUserActivation(frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Unable to install app. This API can only be called shortly after a "
        "user activation.");
    return ScriptPromise<WebInstallResult>();
  }

  KURL resolved_id = ResolveManifestId(manifest_id, exception_state);
  if (!resolved_id.IsValid()) {
    return ScriptPromise<WebInstallResult>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebInstallResult>>(
          script_state);
  ScriptPromise<WebInstallResult> promise = resolver->Promise();

  CHECK(GetService());
  GetService()->InstallBackgroundDocument(
      resolved_id, KURL(install_url),
      WTF::BindOnce(&blink::OnInstallResponse, WrapPersistent(resolver)));
  return promise;
}

void NavigatorWebInstall::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorWebInstall& NavigatorWebInstall::From(Navigator& navigator) {
  NavigatorWebInstall* navigator_web_install =
      Supplement<Navigator>::From<NavigatorWebInstall>(navigator);
  if (!navigator_web_install) {
    navigator_web_install =
        MakeGarbageCollected<NavigatorWebInstall>(navigator);
    ProvideTo(navigator, navigator_web_install);
  }
  return *navigator_web_install;
}

HeapMojoRemote<mojom::blink::WebInstallService>&
NavigatorWebInstall::GetService() {
  if (!service_.is_bound()) {
    auto* context = GetSupplementable()->GetExecutionContext();
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // In case the other endpoint gets disconnected, we want to reset our end of
    // the pipe as well so that we don't remain connected to a half-open pipe.
    service_.set_disconnect_handler(WTF::BindOnce(
        &NavigatorWebInstall::OnConnectionError, WrapWeakPersistent(this)));
  }
  return service_;
}

void NavigatorWebInstall::OnConnectionError() {
  service_.reset();
}

bool NavigatorWebInstall::CheckPreconditionsMaybeThrow(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // TODO(crbug.com/333795265): Verify that site has been granted web install
  // permission once implemented.

  Navigator* const navigator = GetSupplementable();

  if (!navigator->DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The object is no longer associated to a document.");
    return false;
  }

  if (!navigator->DomWindow()->GetFrame()->IsMainFrame() ||
      navigator->DomWindow()->GetFrame()->GetPage()->IsPrerendering() ||
      navigator->DomWindow()->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "API is only supported in primary top-level browsing contexts.");
    return false;
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state.");
    return false;
  }

  return true;
}

KURL NavigatorWebInstall::ResolveManifestId(const String& manifest_id,
                                            ExceptionState& exception_state) {
  KURL resolved = KURL(manifest_id);
  if (resolved.IsValid()) {
    return resolved;
  }

  KURL document_url =
      GetSupplementable()->DomWindow()->GetFrame()->GetDocument()->Url();
  KURL origin = KURL(SecurityOrigin::Create(document_url)->ToString());

  resolved = KURL(origin, manifest_id);
  if (!resolved.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kAbortError,
                                      "Invalid manifest id.");
    return KURL();
  }

  return resolved;
}

}  // namespace blink

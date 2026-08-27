// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/web_install/navigator_web_install.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_install_params.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_install_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace {
const char kInvalidManifestIdErrorDetails[] = "Invalid manifest id";
const char kInvalidManifestUrlErrorDetails[] = "Invalid manifest url";
}  // namespace

namespace blink {
const char NavigatorWebInstall::kSupplementName[] = "NavigatorWebInstall";

void OnInstallFromManifestResponse(
    ScriptPromiseResolver<WebInstallResult>* resolver,
    mojom::blink::WebInstallServiceResult result) {
  switch (result) {
    case mojom::blink::WebInstallServiceResult::kAbortError:
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
      return;
    case mojom::blink::WebInstallServiceResult::kDataError:
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kDataError));
      return;
    // TODO(crbug.com/520052963): Evaluate how much information to expose to
    // the caller. For now, eliminate the manifest ID, but keep the success.
    case mojom::blink::WebInstallServiceResult::kSuccess:
      resolver->Resolve(WebInstallResult::Create());
      return;
  }
  NOTREACHED();
}

NavigatorWebInstall::NavigatorWebInstall(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      service_(navigator.GetExecutionContext()) {}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallImpl(script_state,
                                                          exception_state);
}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    const InstallParams* params,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallFromParamsImpl(
      script_state, params, exception_state);
}

ScriptPromise<WebInstallResult> NavigatorWebInstall::InstallImpl(
    ScriptState* script_state,
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

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebInstallResult>>(
          script_state);
  ScriptPromise<WebInstallResult> promise = resolver->Promise();

  // Initiate installation of the current document.
  CHECK(GetService());
  GetService()->InstallFromManifest(
      /*options=*/nullptr, BindOnce(&blink::OnInstallFromManifestResponse,
                                    WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<WebInstallResult> NavigatorWebInstall::InstallFromParamsImpl(
    ScriptState* script_state,
    const InstallParams* params,
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

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebInstallResult>>(
          script_state);
  ScriptPromise<WebInstallResult> promise = resolver->Promise();

  // `manifest` is non-nullable, but it could still be invalid.
  CHECK(params);
  CHECK(params->hasManifest());
  KURL manifest_url(params->manifest());
  if (!manifest_url.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kInvalidManifestUrlErrorDetails));
    return promise;
  }

  mojom::blink::ManifestInstallOptionsPtr options =
      mojom::blink::ManifestInstallOptions::New();
  options->manifest_url = manifest_url;

  // Treat null `manifestId` as if it wasn't provided.
  if (params->hasManifestId() && !params->manifestId().IsNull()) {
    KURL manifest_id = KURL(params->manifestId());
    // Reject invalid ids, including empty strings.
    if (!manifest_id.IsValid()) {
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), kInvalidManifestIdErrorDetails));
      return promise;
    }
    options->manifest_id = manifest_id;
  }

  CHECK(GetService());
  GetService()->InstallFromManifest(
      std::move(options), BindOnce(&blink::OnInstallFromManifestResponse,
                                   WrapPersistent(resolver)));
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
    service_.set_disconnect_handler(BindOnce(
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
  CHECK(base::FeatureList::IsEnabled(blink::features::kWebAppInstallation));

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               network::mojom::PermissionsPolicyFeature::kWebAppInstallation)) {
    exception_state.ThrowSecurityError(
        "Access to the feature \"web-app-installation\" is disallowed by "
        "Permissions Policy.");
    return false;
  }

  Navigator* const navigator = GetSupplementable();

  if (!navigator->DomWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "The object is no longer associated to a document.");
    return false;
  }

  // TODO(crbug.com/493534965): Evaluate sandbox restrictions. In the meantime,
  // disallow in all sandboxed contexts (iframes and top level documents).
  if (navigator->DomWindow()->GetSandboxFlags() !=
      network::mojom::blink::WebSandboxFlags::kNone) {
    exception_state.ThrowSecurityError(
        "API is not allowed in sandboxed contexts.");
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

}  // namespace blink

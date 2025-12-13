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
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_install_result.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

const char NavigatorWebInstall::kSupplementName[] = "NavigatorWebInstall";
const char kInvalidInstallUrlErrorDetails[] = "Invalid install url";
const char kInvalidManifestIdErrorDetails[] = "Invalid manifest id";

void OnInstallResponse(ScriptPromiseResolver<WebInstallResult>* resolver,
                       mojom::blink::WebInstallServiceResult result,
                       const KURL& manifest_id) {
  switch (result) {
    case mojom::blink::WebInstallServiceResult::kAbortError:
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));
      break;
    case mojom::blink::WebInstallServiceResult::kDataError:
      resolver->Reject(
          MakeGarbageCollected<DOMException>(DOMExceptionCode::kDataError));
      break;
    case mojom::blink::WebInstallServiceResult::kSuccess:
      WebInstallResult* blink_result = WebInstallResult::Create();
      blink_result->setManifestId(manifest_id.GetString());
      resolver->Resolve(std::move(blink_result));
      break;
  }
}

NavigatorWebInstall::NavigatorWebInstall(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      service_(navigator.GetExecutionContext()) {}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallImpl(
      script_state,
      /*install_url=*/std::optional<String>(),
      /*manifest_id=*/std::optional<String>(), exception_state);
}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    const String& install_url,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallImpl(
      script_state, std::optional<String>(install_url),
      /*manifest_id=*/std::optional<String>(), exception_state);
}

// static:
ScriptPromise<WebInstallResult> NavigatorWebInstall::install(
    ScriptState* script_state,
    Navigator& navigator,
    const String& install_url,
    const String& manifest_id,
    ExceptionState& exception_state) {
  return NavigatorWebInstall::From(navigator).InstallImpl(
      script_state, std::optional<String>(install_url),
      std::optional<String>(manifest_id), exception_state);
}

ScriptPromise<WebInstallResult> NavigatorWebInstall::InstallImpl(
    ScriptState* script_state,
    const std::optional<String>& install_url,
    const std::optional<String>& manifest_id,
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

  CHECK(GetService());

  // `navigator.install()` was called.
  // Initiate installation of the current document.
  if (!manifest_id && !install_url) {
    GetService()->Install(
        /*options=*/nullptr,
        BindOnce(&blink::OnInstallResponse, WrapPersistent(resolver)));
    return promise;
  }

  // `install_url` is a required field for all other signatures.
  CHECK(install_url.has_value());

  if (!IsInstallUrlValid(install_url.value())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), kInvalidInstallUrlErrorDetails));
    return promise;
  }
  mojom::blink::InstallOptionsPtr options = mojom::blink::InstallOptions::New();
  options->install_url = KURL(install_url.value());

  // `navigator.install(install_url, manifest_id)` was called.
  if (manifest_id) {
    KURL resolved_id = ValidateAndResolveManifestId(manifest_id.value());
    if (!resolved_id.IsValid()) {
      resolver->Reject(V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), kInvalidManifestIdErrorDetails));
      return promise;
    }
    options->manifest_id = resolved_id;
  }

  GetService()->Install(std::move(options), BindOnce(&blink::OnInstallResponse,
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

bool NavigatorWebInstall::IsInstallUrlValid(const String& install_url) {
  // This covers edge cases such as the empty string, undefined, null, other
  // JavaScript types (eg. Number, Boolean), and general invalid strings
  // (strings that are not valid URLs).
  return !install_url.empty() && KURL(install_url).IsValid();
}

KURL NavigatorWebInstall::ValidateAndResolveManifestId(
    const String& manifest_id) {
  // TODO(crbug.com/397043069): Verify manifest id resolution on blink side.
  // Ensure that edge cases are handled correctly as well.

  // Manifest id validation is different than install url, since
  // `manifest_id` alone doesn't have to be a valid URL, so we can only check
  // for empty, undefined, and null arguments to the API at this point.
  if (manifest_id.empty() || manifest_id == "null" ||
      manifest_id == "undefined") {
    return KURL();
  }

  KURL resolved = KURL(manifest_id);
  if (resolved.IsValid()) {
    return resolved;
  }

  KURL document_url =
      GetSupplementable()->DomWindow()->GetFrame()->GetDocument()->Url();
  KURL origin = KURL(SecurityOrigin::Create(document_url)->ToString());

  resolved = KURL(origin, manifest_id);
  return resolved.IsValid() ? resolved : KURL();
}

}  // namespace blink

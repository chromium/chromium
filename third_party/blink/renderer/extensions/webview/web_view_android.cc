// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/web_view_android.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/webview/webview_media_integrity.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/extensions_webview/v8/v8_get_media_integrity_token_provider_params.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_error.h"
#include "third_party/blink/renderer/extensions/webview/media_integrity/media_integrity_token_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {
const char kInvalidContext[] = "Invalid context";
}  // namespace

namespace blink {

const char WebViewAndroid::kSupplementName[] = "WebView";

WebViewAndroid& WebViewAndroid::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());

  auto* supplement =
      Supplement<ExecutionContext>::From<WebViewAndroid>(execution_context);

  if (!supplement) {
    supplement = MakeGarbageCollected<WebViewAndroid>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

WebViewAndroid::WebViewAndroid(ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context),
      ExecutionContextClient(&execution_context),
      media_integrity_service_remote_(&execution_context) {}

void WebViewAndroid::EnsureServiceConnection(
    ExecutionContext* execution_context) {
  if (media_integrity_service_remote_.is_bound()) {
    return;
  }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context->GetTaskRunner(TaskType::kInternalDefault);
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      media_integrity_service_remote_.BindNewPipeAndPassReceiver(task_runner));
  media_integrity_service_remote_.set_disconnect_handler(WTF::BindOnce(
      &WebViewAndroid::OnServiceConnectionError, WrapWeakPersistent(this)));
}

void WebViewAndroid::OnServiceConnectionError() {
  media_integrity_service_remote_.reset();
  for (auto& resolver : provider_resolvers_) {
    ScriptState* script_state = resolver->GetScriptState();
    if (!script_state->ContextIsValid()) {
      continue;
    }
    ScriptState::Scope scope(script_state);
    resolver->Reject(MediaIntegrityError::CreateForName(
        script_state->GetIsolate(),
        V8MediaIntegrityErrorName::Enum::kInternalError));
  }
  provider_resolvers_.clear();
}

ScriptPromise<MediaIntegrityTokenProvider>
WebViewAndroid::getExperimentalMediaIntegrityTokenProvider(
    ScriptState* script_state,
    GetMediaIntegrityTokenProviderParams* params,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidContext);
    return EmptyPromise();
  }
  ScriptState::Scope scope(script_state);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  const SecurityOrigin* origin = execution_context->GetSecurityOrigin();
  if ((origin->Protocol() != url::kHttpScheme &&
       origin->Protocol() != url::kHttpsScheme) ||
      !origin->IsPotentiallyTrustworthy()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "getExperimentalMediaIntegrityTokenProvider: "
        "can only be used from trustworthy http/https origins");
    return EmptyPromise();
  }

  ScriptPromiseResolver<MediaIntegrityTokenProvider>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<MediaIntegrityTokenProvider>>(
          script_state, exception_state.GetContext());
  ScriptPromise<MediaIntegrityTokenProvider> promise = resolver->Promise();

  if (!params->hasCloudProjectNumber()) {
    resolver->Reject(MediaIntegrityError::CreateForName(
        script_state->GetIsolate(),
        V8MediaIntegrityErrorName::Enum::kInvalidArgument));
    return promise;
  }

  const uint64_t cloud_project_number = params->cloudProjectNumber();

  // This is checked in the browser also, but the browser will consider it a bad
  // message (and has the right to ignore or kill the renderer). We want to
  // report an error to the script instead.
  if (cloud_project_number >
      mojom::blink::WebViewMediaIntegrityService::kMaxCloudProjectNumber) {
    resolver->Reject(MediaIntegrityError::CreateForName(
        script_state->GetIsolate(),
        V8MediaIntegrityErrorName::Enum::kInvalidArgument));
    return promise;
  }

  EnsureServiceConnection(execution_context);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      execution_context->GetTaskRunner(TaskType::kInternalDefault);
  mojo::PendingRemote<mojom::blink::WebViewMediaIntegrityProvider>
      provider_pending_remote;
  mojo::PendingReceiver<mojom::blink::WebViewMediaIntegrityProvider>
      provider_pending_receiver =
          provider_pending_remote.InitWithNewPipeAndPassReceiver();

  provider_resolvers_.insert(resolver);
  media_integrity_service_remote_->GetIntegrityProvider(
      std::move(provider_pending_receiver), cloud_project_number,
      WTF::BindOnce(&WebViewAndroid::OnGetIntegrityProviderResponse,
                    WrapPersistent(this), WrapPersistent(script_state),
                    std::move(provider_pending_remote), cloud_project_number,
                    WrapPersistent(resolver)));

  return promise;
}

void WebViewAndroid::OnGetIntegrityProviderResponse(
    ScriptState* script_state,
    mojo::PendingRemote<mojom::blink::WebViewMediaIntegrityProvider>
        provider_pending_remote,
    const uint64_t cloud_project_number,
    ScriptPromiseResolver<MediaIntegrityTokenProvider>* resolver,
    const std::optional<mojom::blink::WebViewMediaIntegrityErrorCode> error) {
  provider_resolvers_.erase(resolver);

  if (!script_state->ContextIsValid()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidContext));
    return;
  }
  ScriptState::Scope scope(script_state);

  if (error.has_value()) {
    resolver->Reject(MediaIntegrityError::CreateFromMojomEnum(
        script_state->GetIsolate(), *error));
    return;
  }

  MediaIntegrityTokenProvider* provider =
      MakeGarbageCollected<MediaIntegrityTokenProvider>(
          ExecutionContext::From(script_state),
          std::move(provider_pending_remote), cloud_project_number);

  resolver->Resolve(provider);
}

void WebViewAndroid::Trace(Visitor* visitor) const {
  visitor->Trace(provider_resolvers_);
  visitor->Trace(media_integrity_service_remote_);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

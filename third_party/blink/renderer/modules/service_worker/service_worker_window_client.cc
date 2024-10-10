// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_visibility_state.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/page/page_hidden_state.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

void DidFocus(ScriptPromiseResolver<ServiceWorkerWindowClient>* resolver,
              mojom::blink::ServiceWorkerClientInfoPtr client) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (!client) {
    resolver->Reject(ServiceWorkerError::GetException(
        resolver, mojom::blink::ServiceWorkerErrorType::kNotFound,
        "The client was not found."));
    return;
  }
  resolver->Resolve(MakeGarbageCollected<ServiceWorkerWindowClient>(*client));
}

void DidNavigateOrOpenWindow(
    ScriptPromiseResolver<IDLNullable<ServiceWorkerWindowClient>>* resolver,
    bool success,
    mojom::blink::ServiceWorkerClientInfoPtr info,
    const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (!success) {
    DCHECK(!info);
    DCHECK(!error_msg.IsNull());
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowException::CreateTypeError(
        resolver->GetScriptState()->GetIsolate(), error_msg));
    return;
  }
  ServiceWorkerWindowClient* window_client = nullptr;
  // Even if the open/navigation succeeded, |info| may be null if information of
  // the opened/navigated window could not be obtained (this can happen for a
  // cross-origin window, or if the browser process could not get the
  // information in time before the window was closed).
  if (info)
    window_client = MakeGarbageCollected<ServiceWorkerWindowClient>(*info);
  resolver->Resolve(window_client);
}

}  // namespace

// static
ServiceWorkerWindowClient::ResolveWindowClientCallback
ServiceWorkerWindowClient::CreateResolveWindowClientCallback(
    ScriptPromiseResolver<IDLNullable<ServiceWorkerWindowClient>>* resolver) {
  return WTF::BindOnce(&DidNavigateOrOpenWindow, WrapPersistent(resolver));
}

ServiceWorkerWindowClient::ServiceWorkerWindowClient(
    const mojom::blink::ServiceWorkerClientInfo& info)
    : ServiceWorkerClient(info),
      page_hidden_(info.page_hidden),
      is_focused_(info.is_focused) {
  DCHECK_EQ(mojom::blink::ServiceWorkerClientType::kWindow, info.client_type);
}

ServiceWorkerWindowClient::~ServiceWorkerWindowClient() = default;

V8VisibilityState ServiceWorkerWindowClient::visibilityState() const {
  if (page_hidden_) {
    return V8VisibilityState(V8VisibilityState::Enum::kHidden);
  } else {
    return V8VisibilityState(V8VisibilityState::Enum::kVisible);
  }
}

ScriptPromise<ServiceWorkerWindowClient> ServiceWorkerWindowClient::focus(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ServiceWorkerWindowClient>>(
          script_state);
  auto promise = resolver->Promise();
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));

  if (!global_scope->IsWindowInteractionAllowed()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidAccessError,
        "Not allowed to focus a window."));
    return promise;
  }
  global_scope->ConsumeWindowInteraction();

  global_scope->GetServiceWorkerHost()->FocusClient(
      Uuid(), WTF::BindOnce(&DidFocus, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLNullable<ServiceWorkerWindowClient>>
ServiceWorkerWindowClient::navigate(ScriptState* script_state,
                                    const String& url) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<ServiceWorkerWindowClient>>>(
      script_state);
  auto promise = resolver->Promise();
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));

  KURL parsed_url = KURL(global_scope->location()->Url(), url);
  if (!parsed_url.IsValid() || parsed_url.ProtocolIsAbout()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }
  if (!global_scope->GetSecurityOrigin()->CanDisplay(parsed_url)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "'" + parsed_url.ElidedString() + "' cannot navigate."));
    return promise;
  }

  global_scope->GetServiceWorkerHost()->NavigateClient(
      Uuid(), parsed_url, CreateResolveWindowClientCallback(resolver));
  return promise;
}

void ServiceWorkerWindowClient::Trace(Visitor* visitor) const {
  ServiceWorkerClient::Trace(visitor);
}

}  // namespace blink

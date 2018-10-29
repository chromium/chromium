// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/page/page_visibility_state.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

namespace {

void DidFocus(ScriptPromiseResolver* resolver,
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
  resolver->Resolve(ServiceWorkerWindowClient::Create(*client));
}

}  // namespace

ServiceWorkerWindowClient* ServiceWorkerWindowClient::Create(
    const WebServiceWorkerClientInfo& info) {
  DCHECK_EQ(mojom::blink::ServiceWorkerClientType::kWindow, info.client_type);
  return new ServiceWorkerWindowClient(info);
}

ServiceWorkerWindowClient* ServiceWorkerWindowClient::Create(
    const mojom::blink::ServiceWorkerClientInfo& info) {
  DCHECK_EQ(mojom::blink::ServiceWorkerClientType::kWindow, info.client_type);
  return new ServiceWorkerWindowClient(info);
}

ServiceWorkerWindowClient::ServiceWorkerWindowClient(
    const WebServiceWorkerClientInfo& info)
    : ServiceWorkerClient(info),
      page_visibility_state_(info.page_visibility_state),
      is_focused_(info.is_focused) {}

ServiceWorkerWindowClient::ServiceWorkerWindowClient(
    const mojom::blink::ServiceWorkerClientInfo& info)
    : ServiceWorkerClient(info),
      page_visibility_state_(info.page_visibility_state),
      is_focused_(info.is_focused) {}

ServiceWorkerWindowClient::~ServiceWorkerWindowClient() = default;

String ServiceWorkerWindowClient::visibilityState() const {
  return PageVisibilityStateString(page_visibility_state_);
}

ScriptPromise ServiceWorkerWindowClient::focus(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ExecutionContext::From(script_state)->IsWindowInteractionAllowed()) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidAccessError,
                                          "Not allowed to focus a window."));
    return promise;
  }
  ExecutionContext::From(script_state)->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(ExecutionContext::From(script_state))
      ->Focus(Uuid(), WTF::Bind(&DidFocus, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise ServiceWorkerWindowClient::navigate(ScriptState* script_state,
                                                  const String& url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  KURL parsed_url =
      KURL(To<WorkerGlobalScope>(context)->location()->Url(), url);
  if (!parsed_url.IsValid() || parsed_url.ProtocolIsAbout()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }
  if (!context->GetSecurityOrigin()->CanDisplay(parsed_url)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "'" + parsed_url.ElidedString() + "' cannot navigate."));
    return promise;
  }

  ServiceWorkerGlobalScopeClient::From(context)->Navigate(Uuid(), parsed_url,
                                                          resolver);
  return promise;
}

void ServiceWorkerWindowClient::Trace(blink::Visitor* visitor) {
  ServiceWorkerClient::Trace(visitor);
}

}  // namespace blink

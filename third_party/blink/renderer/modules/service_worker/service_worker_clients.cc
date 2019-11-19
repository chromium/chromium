// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_clients.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

mojom::ServiceWorkerClientType GetClientType(const String& type) {
  if (type == "window")
    return mojom::ServiceWorkerClientType::kWindow;
  if (type == "worker")
    return mojom::ServiceWorkerClientType::kDedicatedWorker;
  if (type == "sharedworker")
    return mojom::ServiceWorkerClientType::kSharedWorker;
  if (type == "all")
    return mojom::ServiceWorkerClientType::kAll;
  NOTREACHED();
  return mojom::ServiceWorkerClientType::kWindow;
}

void DidGetClient(ScriptPromiseResolver* resolver,
                  mojom::blink::ServiceWorkerClientInfoPtr info) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (!info) {
    // Resolve the promise with undefined.
    resolver->Resolve();
    return;
  }
  ServiceWorkerClient* client = nullptr;
  switch (info->client_type) {
    case mojom::ServiceWorkerClientType::kWindow:
      client = ServiceWorkerWindowClient::Create(*info);
      break;
    case mojom::ServiceWorkerClientType::kDedicatedWorker:
    case mojom::ServiceWorkerClientType::kSharedWorker:
      client = ServiceWorkerClient::Create(*info);
      break;
    case mojom::ServiceWorkerClientType::kAll:
      NOTREACHED();
      return;
  }
  resolver->Resolve(client);
}

void DidClaim(ScriptPromiseResolver* resolver,
              mojom::blink::ServiceWorkerErrorType error,
              const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (error != mojom::blink::ServiceWorkerErrorType::kNone) {
    DCHECK(!error_msg.IsNull());
    resolver->Reject(
        ServiceWorkerError::GetException(resolver, error, error_msg));
    return;
  }
  DCHECK(error_msg.IsNull());
  resolver->Resolve();
}

void DidGetClients(ScriptPromiseResolver* resolver,
                   Vector<mojom::blink::ServiceWorkerClientInfoPtr> infos) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  HeapVector<Member<ServiceWorkerClient>> clients;
  for (const auto& info : infos) {
    if (info->client_type == mojom::blink::ServiceWorkerClientType::kWindow)
      clients.push_back(ServiceWorkerWindowClient::Create(*info));
    else
      clients.push_back(ServiceWorkerClient::Create(*info));
  }
  resolver->Resolve(std::move(clients));
}

}  // namespace

ServiceWorkerClients* ServiceWorkerClients::Create() {
  return MakeGarbageCollected<ServiceWorkerClients>();
}

ServiceWorkerClients::ServiceWorkerClients() = default;

ScriptPromise ServiceWorkerClients::get(ScriptState* script_state,
                                        const String& id) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));
  // TODO(jungkees): May be null due to worker termination:
  // http://crbug.com/413518.
  if (!global_scope)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  global_scope->GetServiceWorkerHost()->GetClient(
      id, WTF::Bind(&DidGetClient, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise ServiceWorkerClients::matchAll(
    ScriptState* script_state,
    const ClientQueryOptions* options) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!global_scope)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  global_scope->GetServiceWorkerHost()->GetClients(
      mojom::blink::ServiceWorkerClientQueryOptions::New(
          options->includeUncontrolled(), GetClientType(options->type())),
      WTF::Bind(&DidGetClients, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise ServiceWorkerClients::claim(ScriptState* script_state) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));

  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!global_scope)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  global_scope->GetServiceWorkerHost()->ClaimClients(
      WTF::Bind(&DidClaim, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise ServiceWorkerClients::openWindow(ScriptState* script_state,
                                               const String& url) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));

  KURL parsed_url = KURL(global_scope->location()->Url(), url);
  if (!parsed_url.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }

  if (!global_scope->GetSecurityOrigin()->CanDisplay(parsed_url)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "'" + parsed_url.ElidedString() + "' cannot be opened."));
    return promise;
  }

  if (!global_scope->IsWindowInteractionAllowed()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidAccessError,
        "Not allowed to open a window."));
    return promise;
  }
  global_scope->ConsumeWindowInteraction();

  global_scope->GetServiceWorkerHost()->OpenNewTab(
      parsed_url,
      ServiceWorkerWindowClient::CreateResolveWindowClientCallback(resolver));
  return promise;
}

}  // namespace blink

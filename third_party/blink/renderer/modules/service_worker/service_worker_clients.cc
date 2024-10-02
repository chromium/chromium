// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_clients.h"

#include <utility>

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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

mojom::blink::ServiceWorkerClientType GetClientType(V8ClientType::Enum type) {
  switch (type) {
    case V8ClientType::Enum::kWindow:
      return mojom::blink::ServiceWorkerClientType::kWindow;
    case V8ClientType::Enum::kWorker:
      return mojom::blink::ServiceWorkerClientType::kDedicatedWorker;
    case V8ClientType::Enum::kSharedworker:
      return mojom::blink::ServiceWorkerClientType::kSharedWorker;
    case V8ClientType::Enum::kAll:
      return mojom::blink::ServiceWorkerClientType::kAll;
  }
  NOTREACHED();
}

void DidGetClient(ScriptPromiseResolver<ServiceWorkerClient>* resolver,
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
    case mojom::blink::ServiceWorkerClientType::kWindow:
      client = MakeGarbageCollected<ServiceWorkerWindowClient>(*info);
      break;
    case mojom::blink::ServiceWorkerClientType::kDedicatedWorker:
    case mojom::blink::ServiceWorkerClientType::kSharedWorker:
      client = MakeGarbageCollected<ServiceWorkerClient>(*info);
      break;
    case mojom::blink::ServiceWorkerClientType::kAll:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  resolver->Resolve(client);
}

void DidClaim(ScriptPromiseResolver<IDLUndefined>* resolver,
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

void DidGetClients(
    ScriptPromiseResolver<IDLSequence<ServiceWorkerClient>>* resolver,
    Vector<mojom::blink::ServiceWorkerClientInfoPtr> infos) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  HeapVector<Member<ServiceWorkerClient>> clients;
  for (const auto& info : infos) {
    if (info->client_type == mojom::blink::ServiceWorkerClientType::kWindow)
      clients.push_back(MakeGarbageCollected<ServiceWorkerWindowClient>(*info));
    else
      clients.push_back(MakeGarbageCollected<ServiceWorkerClient>(*info));
  }
  resolver->Resolve(std::move(clients));
}

}  // namespace

ServiceWorkerClients* ServiceWorkerClients::Create() {
  return MakeGarbageCollected<ServiceWorkerClients>();
}

ServiceWorkerClients::ServiceWorkerClients() = default;

ScriptPromise<ServiceWorkerClient> ServiceWorkerClients::get(
    ScriptState* script_state,
    const String& id) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));
  // TODO(jungkees): May be null due to worker termination:
  // http://crbug.com/413518.
  if (!global_scope)
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ServiceWorkerClient>>(
          script_state);
  global_scope->GetServiceWorkerHost()->GetClient(
      id, WTF::BindOnce(&DidGetClient, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLSequence<ServiceWorkerClient>> ServiceWorkerClients::matchAll(
    ScriptState* script_state,
    const ClientQueryOptions* options) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!global_scope)
    return ScriptPromise<IDLSequence<ServiceWorkerClient>>();

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<ServiceWorkerClient>>>(script_state);
  global_scope->GetServiceWorkerHost()->GetClients(
      mojom::blink::ServiceWorkerClientQueryOptions::New(
          options->includeUncontrolled(),
          GetClientType(options->type().AsEnum())),
      WTF::BindOnce(&DidGetClients, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> ServiceWorkerClients::claim(
    ScriptState* script_state) {
  ServiceWorkerGlobalScope* global_scope =
      To<ServiceWorkerGlobalScope>(ExecutionContext::From(script_state));

  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!global_scope)
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  global_scope->GetServiceWorkerHost()->ClaimClients(
      WTF::BindOnce(&DidClaim, WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLNullable<ServiceWorkerWindowClient>>
ServiceWorkerClients::openWindow(ScriptState* script_state, const String& url) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLNullable<ServiceWorkerWindowClient>>>(
      script_state);
  auto promise = resolver->Promise();
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

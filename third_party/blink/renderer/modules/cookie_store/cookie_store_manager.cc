// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cookie_store/cookie_store_manager.h"

#include <optional>
#include <utility>

#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_list_item.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cookie_store_get_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Returns null if and only if an exception is thrown.
mojom::blink::CookieChangeSubscriptionPtr ToBackendSubscription(
    const KURL& default_cookie_url,
    const CookieStoreGetOptions* subscription,
    ExceptionState& exception_state) {
  auto backend_subscription = mojom::blink::CookieChangeSubscription::New();

  if (subscription->hasUrl()) {
    KURL subscription_url(default_cookie_url, subscription->url());
    if (!subscription_url.GetString().StartsWith(
            default_cookie_url.GetString())) {
      exception_state.ThrowTypeError("URL must be within ServiceWorker scope");
      return nullptr;
    }
    backend_subscription->url = subscription_url;
  } else {
    backend_subscription->url = default_cookie_url;
  }

  // TODO(crbug.com/1124499): Cleanup matchType after re-evaluation.
  backend_subscription->match_type =
      network::mojom::blink::CookieMatchType::EQUALS;

  if (subscription->hasName()) {
    backend_subscription->name = subscription->name();
  } else {
    // No name provided. Use a filter that matches all cookies. This overrides
    // a user-provided matchType.
    backend_subscription->match_type =
        network::mojom::blink::CookieMatchType::STARTS_WITH;
    backend_subscription->name = g_empty_string;
  }

  return backend_subscription;
}

CookieStoreGetOptions* ToCookieChangeSubscription(
    const mojom::blink::CookieChangeSubscription& backend_subscription) {
  CookieStoreGetOptions* subscription = CookieStoreGetOptions::Create();
  subscription->setUrl(backend_subscription.url);

  if (!backend_subscription.name.empty())
    subscription->setName(backend_subscription.name);

  return subscription;
}

KURL DefaultCookieURL(ServiceWorkerRegistration* registration) {
  DCHECK(registration);
  return KURL(registration->scope());
}

}  // namespace

// static
const char CookieStoreManager::kSupplementName[] = "CookieStoreManager";

// static
CookieStoreManager* CookieStoreManager::cookies(
    ServiceWorkerRegistration& registration) {
  auto* supplement =
      Supplement<ServiceWorkerRegistration>::From<CookieStoreManager>(
          registration);
  if (!supplement) {
    supplement = MakeGarbageCollected<CookieStoreManager>(registration);
    ProvideTo(registration, supplement);
  }
  return supplement;
}

CookieStoreManager::CookieStoreManager(ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration),
      registration_(&registration),
      backend_(registration.GetExecutionContext()),
      default_cookie_url_(DefaultCookieURL(&registration)) {
  auto* execution_context = registration.GetExecutionContext();
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      backend_.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kDOMManipulation)));
}

ScriptPromise<IDLUndefined> CookieStoreManager::subscribe(
    ScriptState* script_state,
    const HeapVector<Member<CookieStoreGetOptions>>& subscriptions,
    ExceptionState& exception_state) {
  Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_subscriptions;
  backend_subscriptions.ReserveInitialCapacity(subscriptions.size());
  for (const CookieStoreGetOptions* subscription : subscriptions) {
    mojom::blink::CookieChangeSubscriptionPtr backend_subscription =
        ToBackendSubscription(default_cookie_url_, subscription,
                              exception_state);
    if (backend_subscription.is_null()) {
      DCHECK(exception_state.HadException());
      return EmptyPromise();
    }
    backend_subscriptions.push_back(std::move(backend_subscription));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  backend_->AddSubscriptions(
      registration_->RegistrationId(), std::move(backend_subscriptions),
      WTF::BindOnce(&CookieStoreManager::OnSubscribeResult,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> CookieStoreManager::unsubscribe(
    ScriptState* script_state,
    const HeapVector<Member<CookieStoreGetOptions>>& subscriptions,
    ExceptionState& exception_state) {
  Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_subscriptions;
  backend_subscriptions.ReserveInitialCapacity(subscriptions.size());
  for (const CookieStoreGetOptions* subscription : subscriptions) {
    mojom::blink::CookieChangeSubscriptionPtr backend_subscription =
        ToBackendSubscription(default_cookie_url_, subscription,
                              exception_state);
    if (backend_subscription.is_null()) {
      DCHECK(exception_state.HadException());
      return EmptyPromise();
    }
    backend_subscriptions.push_back(std::move(backend_subscription));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  backend_->RemoveSubscriptions(
      registration_->RegistrationId(), std::move(backend_subscriptions),
      WTF::BindOnce(&CookieStoreManager::OnSubscribeResult,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise<IDLSequence<CookieStoreGetOptions>>
CookieStoreManager::getSubscriptions(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  if (!backend_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "CookieStore backend went away");
    return ScriptPromise<IDLSequence<CookieStoreGetOptions>>();
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<CookieStoreGetOptions>>>(
      script_state, exception_state.GetContext());
  backend_->GetSubscriptions(
      registration_->RegistrationId(),
      WTF::BindOnce(&CookieStoreManager::OnGetSubscriptionsResult,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return resolver->Promise();
}

void CookieStoreManager::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  visitor->Trace(backend_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

void CookieStoreManager::OnSubscribeResult(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    bool backend_success) {
  if (!backend_success) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kUnknownError,
        "An unknown error occurred while subscribing to cookie changes.");
    return;
  }
  resolver->Resolve();
}

void CookieStoreManager::OnGetSubscriptionsResult(
    ScriptPromiseResolver<IDLSequence<CookieStoreGetOptions>>* resolver,
    Vector<mojom::blink::CookieChangeSubscriptionPtr> backend_result,
    bool backend_success) {
  if (!backend_success) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kUnknownError,
        "An unknown error occurred while subscribing to cookie changes.");
    return;
  }

  HeapVector<Member<CookieStoreGetOptions>> subscriptions;
  subscriptions.ReserveInitialCapacity(backend_result.size());
  for (const auto& backend_subscription : backend_result) {
    CookieStoreGetOptions* subscription =
        ToCookieChangeSubscription(*backend_subscription);
    subscriptions.push_back(subscription);
  }

  resolver->Resolve(std::move(subscriptions));
}

}  // namespace blink

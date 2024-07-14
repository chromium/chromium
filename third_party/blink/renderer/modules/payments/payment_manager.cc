// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_manager.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/payments/payment_instruments.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

PaymentInstruments* PaymentManager::instruments() {
  if (!instruments_) {
    instruments_ = MakeGarbageCollected<PaymentInstruments>(
        *this, registration_->GetExecutionContext());
  }
  return instruments_.Get();
}

const String& PaymentManager::userHint() {
  return user_hint_;
}

void PaymentManager::setUserHint(const String& user_hint) {
  user_hint_ = user_hint;
  manager_->SetUserHint(user_hint_);
}

ScriptPromise<IDLBoolean> PaymentManager::enableDelegations(
    ScriptState* script_state,
    const Vector<V8PaymentDelegation>& delegations,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot enable payment delegations");
    return EmptyPromise();
  }

  if (enable_delegations_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot call enableDelegations() again until the previous "
        "enableDelegations() is finished");
    return EmptyPromise();
  }

  using MojoPaymentDelegation = payments::mojom::blink::PaymentDelegation;
  Vector<MojoPaymentDelegation> mojo_delegations;
  for (auto delegation : delegations) {
    MojoPaymentDelegation mojo_delegation = MojoPaymentDelegation::PAYER_EMAIL;
    switch (delegation.AsEnum()) {
      case V8PaymentDelegation::Enum::kShippingAddress:
        mojo_delegation = MojoPaymentDelegation::SHIPPING_ADDRESS;
        break;
      case V8PaymentDelegation::Enum::kPayerName:
        mojo_delegation = MojoPaymentDelegation::PAYER_NAME;
        break;
      case V8PaymentDelegation::Enum::kPayerPhone:
        mojo_delegation = MojoPaymentDelegation::PAYER_PHONE;
        break;
      case V8PaymentDelegation::Enum::kPayerEmail:
        mojo_delegation = MojoPaymentDelegation::PAYER_EMAIL;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    mojo_delegations.push_back(mojo_delegation);
  }

  manager_->EnableDelegations(
      std::move(mojo_delegations),
      WTF::BindOnce(&PaymentManager::OnEnableDelegationsResponse,
                    WrapPersistent(this)));
  enable_delegations_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
          script_state, exception_state.GetContext());
  return enable_delegations_resolver_->Promise();
}

void PaymentManager::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  visitor->Trace(manager_);
  visitor->Trace(instruments_);
  visitor->Trace(enable_delegations_resolver_);
  ScriptWrappable::Trace(visitor);
}

PaymentManager::PaymentManager(ServiceWorkerRegistration* registration)
    : registration_(registration),
      manager_(registration->GetExecutionContext()),
      instruments_(nullptr) {
  DCHECK(registration);

  if (ExecutionContext* context = registration->GetExecutionContext()) {
    context->GetBrowserInterfaceBroker().GetInterface(
        manager_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kUserInteraction)));
  }

  manager_.set_disconnect_handler(WTF::BindOnce(
      &PaymentManager::OnServiceConnectionError, WrapWeakPersistent(this)));
  manager_->Init(registration_->GetExecutionContext()->Url(),
                 registration_->scope());
}

void PaymentManager::OnEnableDelegationsResponse(
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(enable_delegations_resolver_);
  enable_delegations_resolver_->Resolve(
      status == payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
  enable_delegations_resolver_.Clear();
}

void PaymentManager::OnServiceConnectionError() {
  if (enable_delegations_resolver_)
    enable_delegations_resolver_.Clear();

  manager_.reset();
}

}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_app_service_worker_registration.h"

#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/payments/payment_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

bool AllowedToUsePaymentFeatures(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return false;

  // Check if the context is in fenced frame or not and return false here
  // because we can't restrict the payment handler API access by permission
  // policy when it's called from service worker context.
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame())
    return false;

  return execution_context->GetSecurityContext()
      .GetPermissionsPolicy()
      ->IsFeatureEnabled(mojom::blink::PermissionsPolicyFeature::kPayment);
}

}  // namespace

PaymentAppServiceWorkerRegistration::~PaymentAppServiceWorkerRegistration() =
    default;

// static
PaymentAppServiceWorkerRegistration& PaymentAppServiceWorkerRegistration::From(
    ServiceWorkerRegistration& registration) {
  PaymentAppServiceWorkerRegistration* supplement =
      Supplement<ServiceWorkerRegistration>::From<
          PaymentAppServiceWorkerRegistration>(registration);

  if (!supplement) {
    supplement = MakeGarbageCollected<PaymentAppServiceWorkerRegistration>(
        &registration);
    ProvideTo(registration, supplement);
  }

  return *supplement;
}

// static
PaymentManager* PaymentAppServiceWorkerRegistration::paymentManager(
    ScriptState* script_state,
    ServiceWorkerRegistration& registration,
    ExceptionState& exception_state) {
  return PaymentAppServiceWorkerRegistration::From(registration)
      .paymentManager(script_state, exception_state);
}

PaymentManager* PaymentAppServiceWorkerRegistration::paymentManager(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!AllowedToUsePaymentFeatures(script_state)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Must be in a top-level browsing context or an iframe needs to specify allow=\"payment\" "
        "explicitly");
    return nullptr;
  }

  if (!payment_manager_) {
    payment_manager_ =
        MakeGarbageCollected<PaymentManager>(GetSupplementable());
  }
  return payment_manager_.Get();
}

void PaymentAppServiceWorkerRegistration::Trace(Visitor* visitor) const {
  visitor->Trace(payment_manager_);
  Supplement<ServiceWorkerRegistration>::Trace(visitor);
}

PaymentAppServiceWorkerRegistration::PaymentAppServiceWorkerRegistration(
    ServiceWorkerRegistration* registration)
    : Supplement(*registration) {}

// static
const char PaymentAppServiceWorkerRegistration::kSupplementName[] =
    "PaymentAppServiceWorkerRegistration";

}  // namespace blink

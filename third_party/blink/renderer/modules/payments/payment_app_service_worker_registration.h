// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_APP_SERVICE_WORKER_REGISTRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_APP_SERVICE_WORKER_REGISTRATION_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class PaymentManager;
class ScriptState;
class ServiceWorkerRegistration;

class PaymentAppServiceWorkerRegistration final
    : public GarbageCollected<PaymentAppServiceWorkerRegistration>,
      public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(PaymentAppServiceWorkerRegistration);

 public:
  static const char kSupplementName[];

  explicit PaymentAppServiceWorkerRegistration(ServiceWorkerRegistration*);
  virtual ~PaymentAppServiceWorkerRegistration();

  static PaymentAppServiceWorkerRegistration& From(ServiceWorkerRegistration&);

  static PaymentManager* paymentManager(ScriptState*,
                                        ServiceWorkerRegistration&);
  PaymentManager* paymentManager(ScriptState*);

  void Trace(blink::Visitor*) override;

 private:
  Member<ServiceWorkerRegistration> registration_;
  Member<PaymentManager> payment_manager_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppServiceWorkerRegistration);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_APP_SERVICE_WORKER_REGISTRATION_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class PaymentInstruments;
class ScriptPromiseResolver;
class ScriptPromise;
class ScriptState;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PaymentManager* Create(ServiceWorkerRegistration*);

  explicit PaymentManager(ServiceWorkerRegistration*);

  PaymentInstruments* instruments();

  const String& userHint();
  void setUserHint(const String&);

  void Trace(blink::Visitor*) override;

  ScriptPromise enableDelegations(
      ScriptState*,
      const Vector<String>& stringified_delegations);

 private:
  void OnServiceConnectionError();

  void OnEnableDelegationsResponse(
      payments::mojom::blink::PaymentHandlerStatus status);

  Member<ServiceWorkerRegistration> registration_;
  mojo::Remote<payments::mojom::blink::PaymentManager> manager_;
  Member<PaymentInstruments> instruments_;
  String user_hint_;
  Member<ScriptPromiseResolver> enable_delegations_resolver_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_

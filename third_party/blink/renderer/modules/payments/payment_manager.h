// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_delegation.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExceptionState;
class PaymentInstruments;
class ScriptPromiseResolver;
class ScriptPromise;
class ScriptState;
class ServiceWorkerRegistration;

class MODULES_EXPORT PaymentManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PaymentManager(ServiceWorkerRegistration*);

  PaymentInstruments* instruments();

  const String& userHint();
  void setUserHint(const String&);

  void Trace(Visitor*) const override;

  ScriptPromise enableDelegations(
      ScriptState*,
      const Vector<V8PaymentDelegation>& delegations,
      ExceptionState&);

 private:
  void OnServiceConnectionError();

  void OnEnableDelegationsResponse(
      payments::mojom::blink::PaymentHandlerStatus status);

  Member<ServiceWorkerRegistration> registration_;
  HeapMojoRemote<payments::mojom::blink::PaymentManager,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      manager_;
  Member<PaymentInstruments> instruments_;
  String user_hint_;
  Member<ScriptPromiseResolver> enable_delegations_resolver_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_MANAGER_H_

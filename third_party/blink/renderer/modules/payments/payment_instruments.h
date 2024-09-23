// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_

#include "base/memory/raw_ref.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class PaymentInstrument;
class PaymentManager;
class ScriptState;

class MODULES_EXPORT PaymentInstruments final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PaymentInstruments(const PaymentManager&, ExecutionContext*);

  PaymentInstruments(const PaymentInstruments&) = delete;
  PaymentInstruments& operator=(const PaymentInstruments&) = delete;

  ScriptPromise<IDLBoolean> deleteInstrument(ScriptState*,
                                             const String& instrument_key,
                                             ExceptionState&);
  ScriptPromise<PaymentInstrument> get(ScriptState*,
                                       const String& instrument_key,
                                       ExceptionState&);
  ScriptPromise<IDLSequence<IDLString>> keys(ScriptState*, ExceptionState&);
  ScriptPromise<IDLBoolean> has(ScriptState*,
                                const String& instrument_key,
                                ExceptionState&);
  ScriptPromise<IDLUndefined> set(ScriptState*,
                                  const String& instrument_key,
                                  const PaymentInstrument* details,
                                  ExceptionState&);
  ScriptPromise<IDLUndefined> clear(ScriptState*, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  mojom::blink::PermissionService* GetPermissionService(ScriptState*);
  void OnRequestPermission(ScriptPromiseResolver<IDLUndefined>*,
                           const String&,
                           const PaymentInstrument*,
                           mojom::blink::PermissionStatus);

  void onDeletePaymentInstrument(ScriptPromiseResolver<IDLBoolean>*,
                                 payments::mojom::blink::PaymentHandlerStatus);
  void onGetPaymentInstrument(ScriptPromiseResolver<PaymentInstrument>*,
                              payments::mojom::blink::PaymentInstrumentPtr,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onKeysOfPaymentInstruments(
      ScriptPromiseResolver<IDLSequence<IDLString>>*,
      const Vector<String>&,
      payments::mojom::blink::PaymentHandlerStatus);
  void onHasPaymentInstrument(ScriptPromiseResolver<IDLBoolean>*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onSetPaymentInstrument(ScriptPromiseResolver<IDLUndefined>*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onClearPaymentInstruments(ScriptPromiseResolver<IDLUndefined>*,
                                 payments::mojom::blink::PaymentHandlerStatus);

  Member<const PaymentManager> payment_manager_;

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_

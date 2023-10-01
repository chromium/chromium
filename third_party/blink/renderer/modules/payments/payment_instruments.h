// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_

#include "base/memory/raw_ref.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class PaymentInstrument;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT PaymentInstruments final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PaymentInstruments(
      const HeapMojoRemote<payments::mojom::blink::PaymentManager>&,
      ExecutionContext*);

  PaymentInstruments(const PaymentInstruments&) = delete;
  PaymentInstruments& operator=(const PaymentInstruments&) = delete;

  ScriptPromise deleteInstrument(ScriptState*,
                                 const String& instrument_key,
                                 ExceptionState&);
  ScriptPromise get(ScriptState*,
                    const String& instrument_key,
                    ExceptionState&);
  ScriptPromise keys(ScriptState*, ExceptionState&);
  ScriptPromise has(ScriptState*,
                    const String& instrument_key,
                    ExceptionState&);
  ScriptPromise set(ScriptState*,
                    const String& instrument_key,
                    const PaymentInstrument* details,
                    ExceptionState&);
  ScriptPromise clear(ScriptState*, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  mojom::blink::PermissionService* GetPermissionService(ScriptState*);
  void OnRequestPermission(ScriptPromiseResolver*,
                           const String&,
                           const PaymentInstrument*,
                           mojom::blink::PermissionStatus);

  void onDeletePaymentInstrument(ScriptPromiseResolver*,
                                 payments::mojom::blink::PaymentHandlerStatus);
  void onGetPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentInstrumentPtr,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onKeysOfPaymentInstruments(ScriptPromiseResolver*,
                                  const Vector<String>&,
                                  payments::mojom::blink::PaymentHandlerStatus);
  void onHasPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onSetPaymentInstrument(ScriptPromiseResolver*,
                              payments::mojom::blink::PaymentHandlerStatus);
  void onClearPaymentInstruments(ScriptPromiseResolver*,
                                 payments::mojom::blink::PaymentHandlerStatus);

  const raw_ref<const HeapMojoRemote<payments::mojom::blink::PaymentManager>,
                ExperimentalRenderer>
      manager_;

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_

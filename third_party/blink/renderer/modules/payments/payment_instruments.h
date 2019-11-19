// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class PaymentInstrument;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class PaymentInstrumentParameter;

class MODULES_EXPORT PaymentInstruments final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PaymentInstruments(
      const mojo::Remote<payments::mojom::blink::PaymentManager>&);

  ScriptPromise deleteInstrument(ScriptState*, const String& instrument_key);
  ScriptPromise get(ScriptState*, const String& instrument_key);
  ScriptPromise keys(ScriptState*);
  ScriptPromise has(ScriptState*, const String& instrument_key);
  ScriptPromise set(ScriptState*,
                    const String& instrument_key,
                    const PaymentInstrument* details,
                    ExceptionState&);
  ScriptPromise clear(ScriptState*);

 private:
  mojom::blink::PermissionService* GetPermissionService(ScriptState*);
  void OnRequestPermission(ScriptPromiseResolver*,
                           const String&,
                           PaymentInstrumentParameter*,
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

  const mojo::Remote<payments::mojom::blink::PaymentManager>& manager_;

  mojo::Remote<mojom::blink::PermissionService> permission_service_;

  DISALLOW_COPY_AND_ASSIGN(PaymentInstruments);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_INSTRUMENTS_H_

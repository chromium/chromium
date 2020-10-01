// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/dom_window_digital_goods.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"

namespace blink {

namespace {

using payments::mojom::blink::CreateDigitalGoodsResponseCode;

const char known_payment_method_[] = "https://play.google.com/billing";

void OnCreateDigitalGoodsResponse(
    ScriptPromiseResolver* resolver,
    CreateDigitalGoodsResponseCode code,
    mojo::PendingRemote<payments::mojom::blink::DigitalGoods> pending_remote) {
  if (code != CreateDigitalGoodsResponseCode::kOk) {
    DCHECK(!pending_remote);
    DVLOG(1) << "CreateDigitalGoodsResponseCode " << code;
    resolver->Resolve();
    return;
  }
  DCHECK(pending_remote);

  auto* digital_goods_service_ =
      MakeGarbageCollected<DigitalGoodsService>(std::move(pending_remote));
  resolver->Resolve(digital_goods_service_);
}

}  // namespace

const char DOMWindowDigitalGoods::kSupplementName[] = "DOMWindowDigitalGoods";

ScriptPromise DOMWindowDigitalGoods::getDigitalGoodsService(
    ScriptState* script_state,
    LocalDOMWindow& window,
    const String& payment_method) {
  return FromState(&window)->GetDigitalGoodsService(script_state,
                                                    payment_method);
}

ScriptPromise DOMWindowDigitalGoods::GetDigitalGoodsService(
    ScriptState* script_state,
    const String& payment_method) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  if (payment_method.IsEmpty()) {
    resolver->Resolve();
    return promise;
  }
  if (payment_method != known_payment_method_) {
    resolver->Resolve();
    return promise;
  }

  // TODO: Bind only on platforms where an implementation exists.
  if (!mojo_service_) {
    ExecutionContext::From(script_state)
        ->GetBrowserInterfaceBroker()
        .GetInterface(mojo_service_.BindNewPipeAndPassReceiver());
  }

  mojo_service_->CreateDigitalGoods(
      payment_method,
      WTF::Bind(&OnCreateDigitalGoodsResponse, WrapPersistent(resolver)));

  return promise;
}

void DOMWindowDigitalGoods::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
DOMWindowDigitalGoods* DOMWindowDigitalGoods::FromState(
    LocalDOMWindow* window) {
  DOMWindowDigitalGoods* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowDigitalGoods>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowDigitalGoods>();
    ProvideTo(*window, supplement);
  }

  return supplement;
}

}  // namespace blink

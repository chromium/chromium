// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/dom_window_digital_goods.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"
#include "third_party/blink/renderer/modules/payments/goods/util.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using blink::digital_goods_util::LogConsoleError;
using payments::mojom::blink::CreateDigitalGoodsResponseCode;

void OnCreateDigitalGoodsResponse(
    ScriptPromiseResolver* resolver,
    CreateDigitalGoodsResponseCode code,
    mojo::PendingRemote<payments::mojom::blink::DigitalGoods> pending_remote) {
  if (code != CreateDigitalGoodsResponseCode::kOk) {
    DCHECK(!pending_remote);
    LogConsoleError(resolver->GetScriptState(),
                    "GetDigitalGoodsService: " + mojo::ConvertTo<String>(code));
    resolver->Resolve(v8::Null(resolver->GetScriptState()->GetIsolate()));
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
    LogConsoleError(script_state,
                    "GetDigitalGoodsService: Empty payment method.");
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return promise;
  }

  if (!script_state->ContextIsValid()) {
    LogConsoleError(script_state, "GetDigitalGoodsService: internal error.");
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return promise;
  }

  auto* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  if (execution_context->IsContextDestroyed()) {
    LogConsoleError(script_state, "GetDigitalGoodsService: internal error.");
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return promise;
  }

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kPayment)) {
    LogConsoleError(script_state,
                    "GetDigitalGoodsService: Payments not enabled.");
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return promise;
  }

  if (!mojo_service_) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        mojo_service_.BindNewPipeAndPassReceiver());
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

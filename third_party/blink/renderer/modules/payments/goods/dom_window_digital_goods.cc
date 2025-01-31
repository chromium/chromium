// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/dom_window_digital_goods.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"
#include "third_party/blink/renderer/modules/payments/goods/util.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

using blink::digital_goods_util::LogConsoleError;
using payments::mojom::blink::CreateDigitalGoodsResponseCode;

void OnCreateDigitalGoodsResponse(
    ScriptPromiseResolver<DigitalGoodsService>* resolver,
    CreateDigitalGoodsResponseCode code,
    mojo::PendingRemote<payments::mojom::blink::DigitalGoods> pending_remote) {
  if (code != CreateDigitalGoodsResponseCode::kOk) {
    DCHECK(!pending_remote);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, mojo::ConvertTo<String>(code)));
    return;
  }
  DCHECK(pending_remote);

  auto* digital_goods_service_ = MakeGarbageCollected<DigitalGoodsService>(
      resolver->GetExecutionContext(), std::move(pending_remote));
  resolver->Resolve(digital_goods_service_);
}

}  // namespace

const char DOMWindowDigitalGoods::kSupplementName[] = "DOMWindowDigitalGoods";

DOMWindowDigitalGoods::DOMWindowDigitalGoods(LocalDOMWindow& window)
    : Supplement(window), mojo_service_(&window) {}

ScriptPromise<DigitalGoodsService>
DOMWindowDigitalGoods::getDigitalGoodsService(ScriptState* script_state,
                                              LocalDOMWindow& window,
                                              const String& payment_method,
                                              ExceptionState& exception_state) {
  return FromState(&window)->GetDigitalGoodsService(
      script_state, window, payment_method, exception_state);
}

ScriptPromise<DigitalGoodsService>
DOMWindowDigitalGoods::GetDigitalGoodsService(ScriptState* script_state,
                                              LocalDOMWindow& window,
                                              const String& payment_method,
                                              ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<DigitalGoodsService>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  if (execution_context->IsContextDestroyed()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "The execution context is destroyed."));
    return promise;
  }

  if (window.IsCrossSiteSubframeIncludingScheme()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Access denied from cross-site frames"));
    return promise;
  }

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kPayment)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "Payment permissions policy not granted"));
    return promise;
  }

  if (payment_method.empty()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Empty payment method"));
    return promise;
  }

  if (!mojo_service_) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        mojo_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  mojo_service_->CreateDigitalGoods(
      payment_method,
      WTF::BindOnce(&OnCreateDigitalGoodsResponse, WrapPersistent(resolver)));

  return promise;
}

void DOMWindowDigitalGoods::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_service_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
DOMWindowDigitalGoods* DOMWindowDigitalGoods::FromState(
    LocalDOMWindow* window) {
  DOMWindowDigitalGoods* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowDigitalGoods>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowDigitalGoods>(*window);
    ProvideTo(*window, supplement);
  }

  return supplement;
}

}  // namespace blink

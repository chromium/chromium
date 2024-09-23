// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_service.h"

#include <type_traits>
#include <utility>

#include "base/check.h"
#include "components/digital_goods/mojom/digital_goods.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ItemDetails;
class PurchaseDetails;

using payments::mojom::blink::BillingResponseCode;

namespace {

void OnGetDetailsResponse(
    ScriptPromiseResolver<IDLSequence<ItemDetails>>* resolver,
    BillingResponseCode code,
    Vector<payments::mojom::blink::ItemDetailsPtr> item_details_list) {
  if (code != BillingResponseCode::kOk) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, mojo::ConvertTo<String>(code)));
    return;
  }
  HeapVector<Member<ItemDetails>> blink_item_details_list;
  for (const auto& details : item_details_list) {
    blink::ItemDetails* blink_details = details.To<blink::ItemDetails*>();
    if (blink_details) {
      blink_item_details_list.push_back(blink_details);
    }
  }

  resolver->Resolve(std::move(blink_item_details_list));
}

void ResolveWithPurchaseReferenceList(
    ScriptPromiseResolver<IDLSequence<PurchaseDetails>>* resolver,
    BillingResponseCode code,
    Vector<payments::mojom::blink::PurchaseReferencePtr>
        purchase_reference_list) {
  if (code != BillingResponseCode::kOk) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, mojo::ConvertTo<String>(code)));
    return;
  }
  HeapVector<Member<PurchaseDetails>> blink_purchase_details_list;
  for (const auto& details : purchase_reference_list) {
    blink::PurchaseDetails* blink_details =
        details.To<blink::PurchaseDetails*>();
    if (blink_details) {
      blink_purchase_details_list.push_back(blink_details);
    }
  }

  resolver->Resolve(std::move(blink_purchase_details_list));
}

void OnConsumeResponse(ScriptPromiseResolver<IDLUndefined>* resolver,
                       BillingResponseCode code) {
  if (code != BillingResponseCode::kOk) {
    resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                     mojo::ConvertTo<String>(code));
    return;
  }
  resolver->Resolve();
}

}  // namespace

DigitalGoodsService::DigitalGoodsService(
    ExecutionContext* context,
    mojo::PendingRemote<payments::mojom::blink::DigitalGoods> pending_remote)
    : mojo_service_(context) {
  DCHECK(pending_remote.is_valid());
  mojo_service_.Bind(std::move(pending_remote),
                     context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  DCHECK(mojo_service_);
}

DigitalGoodsService::~DigitalGoodsService() = default;

ScriptPromise<IDLSequence<ItemDetails>> DigitalGoodsService::getDetails(
    ScriptState* script_state,
    const Vector<String>& item_ids) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<ItemDetails>>>(
          script_state);
  auto promise = resolver->Promise();

  if (item_ids.empty()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Must specify at least one item ID."));
    return promise;
  }

  mojo_service_->GetDetails(
      item_ids, WTF::BindOnce(&OnGetDetailsResponse, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLSequence<PurchaseDetails>> DigitalGoodsService::listPurchases(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<PurchaseDetails>>>(
          script_state);
  auto promise = resolver->Promise();

  mojo_service_->ListPurchases(WTF::BindOnce(&ResolveWithPurchaseReferenceList,
                                             WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLSequence<PurchaseDetails>>
DigitalGoodsService::listPurchaseHistory(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<PurchaseDetails>>>(
          script_state);
  auto promise = resolver->Promise();

  mojo_service_->ListPurchaseHistory(WTF::BindOnce(
      &ResolveWithPurchaseReferenceList, WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLUndefined> DigitalGoodsService::consume(
    ScriptState* script_state,
    const String& purchase_token) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  if (purchase_token.empty()) {
    resolver->RejectWithTypeError("Must specify purchase token.");
    return promise;
  }

  mojo_service_->Consume(
      purchase_token,
      WTF::BindOnce(&OnConsumeResponse, WrapPersistent(resolver)));
  return promise;
}

void DigitalGoodsService::Trace(Visitor* visitor) const {
  visitor->Trace(mojo_service_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

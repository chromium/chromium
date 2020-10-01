// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"

#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"

namespace mojo {

using payments::mojom::blink::BillingResponseCode;
using payments::mojom::blink::ItemDetailsPtr;

blink::ItemDetails* TypeConverter<blink::ItemDetails*, ItemDetailsPtr>::Convert(
    const ItemDetailsPtr& input) {
  if (!input)
    return nullptr;
  blink::ItemDetails* output = blink::ItemDetails::Create();
  output->setItemId(input->item_id);
  output->setTitle(input->title);
  output->setDescription(input->description);
  output->setPrice(
      blink::PaymentEventDataConversion::ToPaymentCurrencyAmount(input->price));
  return output;
}

WTF::String TypeConverter<WTF::String, BillingResponseCode>::Convert(
    const BillingResponseCode& input) {
  switch (input) {
    case BillingResponseCode::kOk:
      return "ok";
    case BillingResponseCode::kError:
      return "error";
    case BillingResponseCode::kItemAlreadyOwned:
      return "itemAlreadyOwned";
    case BillingResponseCode::kItemNotOwned:
      return "itemNotOwned";
    case BillingResponseCode::kItemUnavailable:
      return "itemUnavailable";
    case BillingResponseCode::kClientAppUnavailable:
      return "clientAppUnavailable";
    case BillingResponseCode::kClientAppError:
      return "clientAppError";
  }
  NOTREACHED();
}

}  // namespace mojo

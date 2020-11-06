// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"

#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"

namespace mojo {

using payments::mojom::blink::BillingResponseCode;
using payments::mojom::blink::ItemDetailsPtr;
using payments::mojom::blink::PurchaseDetailsPtr;

blink::ItemDetails* TypeConverter<blink::ItemDetails*, ItemDetailsPtr>::Convert(
    const ItemDetailsPtr& input) {
  if (!input)
    return nullptr;
  blink::ItemDetails* output = blink::ItemDetails::Create();
  output->setItemId(input->item_id);
  output->setTitle(input->title);
  if (!input->description.IsEmpty())
    output->setDescription(input->description);
  output->setPrice(
      blink::PaymentEventDataConversion::ToPaymentCurrencyAmount(input->price));
  if (input->subscription_period && !input->subscription_period.IsEmpty())
    output->setSubscriptionPeriod(input->subscription_period);
  if (input->free_trial_period && !input->free_trial_period.IsEmpty())
    output->setFreeTrialPeriod(input->free_trial_period);
  if (input->introductory_price) {
    output->setIntroductoryPrice(
        blink::PaymentEventDataConversion::ToPaymentCurrencyAmount(
            input->introductory_price));
  }
  if (input->introductory_price_period &&
      !input->introductory_price_period.IsEmpty()) {
    output->setIntroductoryPricePeriod(input->introductory_price_period);
  }
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

blink::PurchaseDetails*
TypeConverter<blink::PurchaseDetails*, PurchaseDetailsPtr>::Convert(
    const PurchaseDetailsPtr& input) {
  if (!input)
    return nullptr;
  blink::PurchaseDetails* output = blink::PurchaseDetails::Create();
  output->setItemId(input->item_id);
  output->setPurchaseToken(input->purchase_token);
  output->setAcknowledged(input->acknowledged);
  switch (input->purchase_state) {
    case payments::mojom::blink::PurchaseState::kUnknown:
      // Omit setting PurchaseState on output.
      break;
    case payments::mojom::blink::PurchaseState::kPurchased:
      output->setPurchaseState("purchased");
      break;
    case payments::mojom::blink::PurchaseState::kPending:
      output->setPurchaseState("pending");
      break;
  }
  output->setPurchaseTime(input->purchase_time.InMilliseconds());
  output->setWillAutoRenew(input->will_auto_renew);
  return output;
}

}  // namespace mojo

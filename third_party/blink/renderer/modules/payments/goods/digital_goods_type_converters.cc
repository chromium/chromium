// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"

#include <optional>
#include <utility>

#include "base/notreached.h"
#include "components/digital_goods/mojom/digital_goods.mojom-blink.h"
#include "components/payments/mojom/payment_request_data.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

using payments::mojom::blink::BillingResponseCode;
using payments::mojom::blink::CreateDigitalGoodsResponseCode;
using payments::mojom::blink::ItemDetailsPtr;
using payments::mojom::blink::ItemType;
using payments::mojom::blink::PurchaseReferencePtr;

WTF::String TypeConverter<WTF::String, CreateDigitalGoodsResponseCode>::Convert(
    const CreateDigitalGoodsResponseCode& input) {
  switch (input) {
    case CreateDigitalGoodsResponseCode::kOk:
      return "ok";
    case CreateDigitalGoodsResponseCode::kError:
      return "error";
    case CreateDigitalGoodsResponseCode::kUnsupportedPaymentMethod:
      return "unsupported payment method";
    case CreateDigitalGoodsResponseCode::kUnsupportedContext:
      return "unsupported context";
  }
  NOTREACHED_IN_MIGRATION();
}

blink::ItemDetails* TypeConverter<blink::ItemDetails*, ItemDetailsPtr>::Convert(
    const ItemDetailsPtr& input) {
  if (!input)
    return nullptr;
  blink::ItemDetails* output = blink::ItemDetails::Create();
  output->setItemId(input->item_id);
  output->setTitle(input->title);
  if (!input->description.empty())
    output->setDescription(input->description);
  output->setPrice(
      blink::PaymentEventDataConversion::ToPaymentCurrencyAmount(input->price));
  if (input->subscription_period && !input->subscription_period.empty())
    output->setSubscriptionPeriod(input->subscription_period);
  if (input->free_trial_period && !input->free_trial_period.empty())
    output->setFreeTrialPeriod(input->free_trial_period);
  if (input->introductory_price) {
    output->setIntroductoryPrice(
        blink::PaymentEventDataConversion::ToPaymentCurrencyAmount(
            input->introductory_price));
  }
  if (input->introductory_price_period &&
      !input->introductory_price_period.empty()) {
    output->setIntroductoryPricePeriod(input->introductory_price_period);
  }
  if (input->introductory_price_cycles > 0)
    output->setIntroductoryPriceCycles(input->introductory_price_cycles);
  switch (input->type) {
    case ItemType::kUnknown:
      // Omit setting ItemType on output.
      break;
    case ItemType::kProduct:
      output->setType("product");
      break;
    case ItemType::kSubscription:
      output->setType("subscription");
      break;
  }
  WTF::Vector<WTF::String> icon_urls;
  if (input->icon_urls.has_value()) {
    for (const blink::KURL& icon_url : input->icon_urls.value()) {
      if (icon_url.IsValid() && !icon_url.IsEmpty()) {
        icon_urls.push_back(icon_url.GetString());
      }
    }
  }
  output->setIconURLs(std::move(icon_urls));
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
  NOTREACHED_IN_MIGRATION();
}

blink::PurchaseDetails*
TypeConverter<blink::PurchaseDetails*, PurchaseReferencePtr>::Convert(
    const PurchaseReferencePtr& input) {
  if (!input)
    return nullptr;
  blink::PurchaseDetails* output = blink::PurchaseDetails::Create();
  output->setItemId(input->item_id);
  output->setPurchaseToken(input->purchase_token);
  return output;
}

}  // namespace mojo

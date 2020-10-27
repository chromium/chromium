// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_item_details.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_currency_amount.h"
#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"

namespace blink {

using payments::mojom::blink::BillingResponseCode;

TEST(DigitalGoodsTypeConvertersTest, MojoBillingResponseOkToIdl) {
  auto response_code = BillingResponseCode::kOk;
  EXPECT_EQ(mojo::ConvertTo<String>(response_code), "ok");
}

TEST(DigitalGoodsTypeConvertersTest, MojoBillingResponseErrorToIdl) {
  auto response_code = BillingResponseCode::kError;
  EXPECT_EQ(mojo::ConvertTo<String>(response_code), "error");
}

TEST(DigitalGoodsTypeConvertersTest, MojoItemDetailsToIdl_WithOptionalFields) {
  auto mojo_item_details = payments::mojom::blink::ItemDetails::New();
  const String item_id = "shiny-sword-id";
  const String title = "Shiny Sword";
  const String description = "A sword that is shiny";
  const String price_currency = "AUD";
  const String price_value = "100.00";
  const String subscription_period = "P1Y";
  const String free_trial_period = "P1M";
  const String introductory_price_currency = "USD";
  const String introductory_price_value = "1.00";
  const String introductory_price_period = "P1W";

  mojo_item_details->item_id = item_id;
  mojo_item_details->title = title;
  mojo_item_details->description = description;
  auto price = payments::mojom::blink::PaymentCurrencyAmount::New(
      price_currency, price_value);
  mojo_item_details->price = std::move(price);
  mojo_item_details->subscription_period = subscription_period;
  mojo_item_details->free_trial_period = free_trial_period;
  auto introductory_price = payments::mojom::blink::PaymentCurrencyAmount::New(
      introductory_price_currency, introductory_price_value);
  mojo_item_details->introductory_price = std::move(introductory_price);
  mojo_item_details->introductory_price_period = introductory_price_period;

  auto* idl_item_details = mojo_item_details.To<ItemDetails*>();
  EXPECT_EQ(idl_item_details->itemId(), item_id);
  EXPECT_EQ(idl_item_details->title(), title);
  EXPECT_EQ(idl_item_details->description(), description);
  EXPECT_EQ(idl_item_details->price()->currency(), price_currency);
  EXPECT_EQ(idl_item_details->price()->value(), price_value);
  EXPECT_EQ(idl_item_details->subscriptionPeriod(), subscription_period);
  EXPECT_EQ(idl_item_details->freeTrialPeriod(), free_trial_period);
  EXPECT_EQ(idl_item_details->introductoryPrice()->currency(),
            introductory_price_currency);
  EXPECT_EQ(idl_item_details->introductoryPrice()->value(),
            introductory_price_value);
  EXPECT_EQ(idl_item_details->introductoryPricePeriod(),
            introductory_price_period);
}

TEST(DigitalGoodsTypeConvertersTest,
     MojoItemDetailsToIdl_WithoutOptionalFields) {
  auto mojo_item_details = payments::mojom::blink::ItemDetails::New();
  const String item_id = "shiny-sword-id";
  const String title = "Shiny Sword";
  const String currency = "AUD";
  const String value = "100.00";

  mojo_item_details->item_id = item_id;
  mojo_item_details->title = title;
  // Description is required by mojo but not by IDL.
  mojo_item_details->description = "";
  auto price = payments::mojom::blink::PaymentCurrencyAmount::New();
  price->currency = currency;
  price->value = value;
  mojo_item_details->price = std::move(price);

  auto* idl_item_details = mojo_item_details.To<ItemDetails*>();
  EXPECT_EQ(idl_item_details->itemId(), item_id);
  EXPECT_EQ(idl_item_details->title(), title);
  EXPECT_EQ(idl_item_details->price()->currency(), currency);
  EXPECT_EQ(idl_item_details->price()->value(), value);
  EXPECT_FALSE(idl_item_details->hasDescription());
  EXPECT_FALSE(idl_item_details->hasSubscriptionPeriod());
  EXPECT_FALSE(idl_item_details->hasFreeTrialPeriod());
  EXPECT_FALSE(idl_item_details->hasIntroductoryPrice());
  EXPECT_FALSE(idl_item_details->hasIntroductoryPricePeriod());
}

TEST(DigitalGoodsTypeConvertersTest, NullMojoItemDetailsToIdl) {
  payments::mojom::blink::ItemDetailsPtr mojo_item_details;

  auto* idl_item_details = mojo_item_details.To<ItemDetails*>();
  EXPECT_EQ(idl_item_details, nullptr);
}

}  // namespace blink

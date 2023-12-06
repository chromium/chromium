// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/digital_goods_type_converters.h"

#include <string>

#include "base/time/time.h"
#include "components/digital_goods/mojom/digital_goods.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-shared.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_item_details.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_currency_amount.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using payments::mojom::blink::BillingResponseCode;

TEST(DigitalGoodsTypeConvertersTest, MojoBillingResponseToIdl) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kOk), "ok");
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kError), "error");
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kItemAlreadyOwned),
            "itemAlreadyOwned");
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kItemNotOwned),
            "itemNotOwned");
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kItemUnavailable),
            "itemUnavailable");
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kClientAppUnavailable),
            "clientAppUnavailable");
  EXPECT_EQ(mojo::ConvertTo<String>(BillingResponseCode::kClientAppError),
            "clientAppError");
}

TEST(DigitalGoodsTypeConvertersTest, MojoItemDetailsToIdl_WithOptionalFields) {
  test::TaskEnvironment task_environment;
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
  const uint64_t introductory_price_cycles = 123;
  const String icon_url_1 = "https://foo.com/icon_url_1.png";
  const String icon_url_2 = "https://foo.com/icon_url_2.png";

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
  mojo_item_details->introductory_price_cycles = introductory_price_cycles;
  mojo_item_details->type = payments::mojom::ItemType::kSubscription;
  mojo_item_details->icon_urls = {KURL(icon_url_1), KURL(icon_url_2)};

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
  EXPECT_EQ(idl_item_details->introductoryPriceCycles(),
            introductory_price_cycles);
  EXPECT_EQ(idl_item_details->type(), "subscription");
  ASSERT_EQ(idl_item_details->iconURLs().size(), 2u);
  EXPECT_EQ(idl_item_details->iconURLs()[0], icon_url_1);
  EXPECT_EQ(idl_item_details->iconURLs()[1], icon_url_2);
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
  EXPECT_FALSE(idl_item_details->hasIntroductoryPriceCycles());
  EXPECT_FALSE(idl_item_details->hasType());
  EXPECT_EQ(idl_item_details->iconURLs().size(), 0u);
}

TEST(DigitalGoodsTypeConvertersTest, NullMojoItemDetailsToIdl) {
  test::TaskEnvironment task_environment;
  payments::mojom::blink::ItemDetailsPtr mojo_item_details;

  auto* idl_item_details = mojo_item_details.To<ItemDetails*>();
  EXPECT_EQ(idl_item_details, nullptr);
}

TEST(DigitalGoodsTypeConvertersTest, MojoPurchaseReferenceToIdl) {
  test::TaskEnvironment task_environment;
  auto mojo_purchase_reference =
      payments::mojom::blink::PurchaseReference::New();
  const String item_id = "shiny-sword-id";
  const String purchase_token = "purchase-token-for-shiny-sword";

  mojo_purchase_reference->item_id = item_id;
  mojo_purchase_reference->purchase_token = purchase_token;

  auto* idl_purchase_details = mojo_purchase_reference.To<PurchaseDetails*>();
  EXPECT_EQ(idl_purchase_details->itemId(), item_id);
  EXPECT_EQ(idl_purchase_details->purchaseToken(), purchase_token);
}

TEST(DigitalGoodsTypeConvertersTest, NullMojoPurchaseReferenceToIdl) {
  test::TaskEnvironment task_environment;
  payments::mojom::blink::PurchaseReferencePtr mojo_purchase_reference;

  auto* idl_purchase_details = mojo_purchase_reference.To<PurchaseDetails*>();
  EXPECT_EQ(idl_purchase_details, nullptr);
}

}  // namespace blink

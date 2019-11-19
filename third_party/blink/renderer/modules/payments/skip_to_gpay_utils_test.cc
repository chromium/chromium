// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/skip_to_gpay_utils.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/payments/payment_options.h"

namespace blink {
namespace {

using ::payments::mojom::blink::PaymentMethodData;
using ::payments::mojom::blink::PaymentMethodDataPtr;

const char kInputDataV1[] =
    "{"
    " \"apiVersion\": 1 "
    "}";
const char kInputDataV2[] =
    "{"
    "\"apiVersion\":2,"
    "\"apiVersionMinor\":0,"
    "\"allowedPaymentMethods\":["
    "  {\"type\":\"CARD\", \"parameters\": {}}"
    "]}";

PaymentMethodDataPtr MakeTestPaymentMethodData() {
  PaymentMethodDataPtr output = PaymentMethodData::New();
  output->supported_method = "https://google.com/pay";
  return output;
}

TEST(SkipToGPayUtilsTest, NothingRequested) {
  auto* options = PaymentOptions::Create();

  {
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kInputDataV1;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ("{\"apiVersion\":1}", output->gpay_bridge_data->stringified_data);
    EXPECT_FALSE(output->gpay_bridge_data->phone_requested);
    EXPECT_FALSE(output->gpay_bridge_data->name_requested);
    EXPECT_FALSE(output->gpay_bridge_data->email_requested);
    EXPECT_FALSE(output->gpay_bridge_data->shipping_requested);
  }
  {
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kInputDataV2;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{}}]}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_FALSE(output->gpay_bridge_data->phone_requested);
    EXPECT_FALSE(output->gpay_bridge_data->name_requested);
    EXPECT_FALSE(output->gpay_bridge_data->email_requested);
    EXPECT_FALSE(output->gpay_bridge_data->shipping_requested);
  }
}

TEST(SkipToGPayUtilsTest, MissingApiVersionConsideredV1) {
  auto* options = PaymentOptions::Create();
  PaymentMethodDataPtr output = MakeTestPaymentMethodData();
  output->stringified_data = "{}";

  ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));
  EXPECT_EQ("{}", output->gpay_bridge_data->stringified_data);
}

TEST(SkipToGPayUtilsTest, InvalidInputData_NotJSON) {
  auto* options = PaymentOptions::Create();
  PaymentMethodDataPtr output = MakeTestPaymentMethodData();
  output->stringified_data = "{invalid_json";

  ASSERT_FALSE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));
  EXPECT_TRUE(output->gpay_bridge_data.is_null());
}

TEST(SkipToGPayUtilsTest, InvalidInputData_NotAnObject) {
  auto* options = PaymentOptions::Create();
  PaymentMethodDataPtr output = MakeTestPaymentMethodData();
  output->stringified_data = "not_an_object";

  ASSERT_FALSE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));
  EXPECT_TRUE(output->gpay_bridge_data.is_null());
}

TEST(SkipToGPayUtilsTest, RequestEverything) {
  auto* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  options->setRequestPayerPhone(true);
  options->setRequestPayerEmail(true);
  options->setRequestShipping(true);

  {
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kInputDataV1;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":1,\"cardRequirements\":{\"billingAddressRequired\":"
        "true},\"phoneNumberRequired\":true,\"emailRequired\":true,"
        "\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kInputDataV1, output->stringified_data);
  }
  {
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kInputDataV2;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}],"
        "\"emailRequired\":true,\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kInputDataV2, output->stringified_data);
  }
}

TEST(SkipToGPayUtilsTest, RequestPhoneOnly) {
  auto* options = PaymentOptions::Create();
  options->setRequestPayerPhone(true);

  {
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kInputDataV1;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":1,\"cardRequirements\":{\"billingAddressRequired\":"
        "true},\"phoneNumberRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    // Phone number can only be requested as part of billing address, which
    // implies that name will be requested too.
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_FALSE(output->gpay_bridge_data->email_requested);
    EXPECT_FALSE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kInputDataV1, output->stringified_data);
  }
  {
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kInputDataV2;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}]}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    // Phone number can only be requested as part of billing address, which
    // implies that name will be requested too.
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_FALSE(output->gpay_bridge_data->email_requested);
    EXPECT_FALSE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kInputDataV2, output->stringified_data);
  }
}

TEST(SkipToGPayUtilsTest, ShippingAlreadyRequested) {
  auto* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  options->setRequestPayerPhone(true);
  options->setRequestPayerEmail(true);
  options->setRequestShipping(true);

  {
    const char kShippingRequested[] =
        "{\"apiVersion\":1,\"shippingAddressRequired\":true}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kShippingRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":1,\"shippingAddressRequired\":true,"
        "\"cardRequirements\":{\"billingAddressRequired\":true},"
        "\"phoneNumberRequired\":true,\"emailRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_FALSE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kShippingRequested, output->stringified_data);
  }
  {
    const char kShippingRequested[] =
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{}}],"
        "\"shippingAddressRequired\":true}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kShippingRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}],"
        "\"shippingAddressRequired\":true,\"emailRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_FALSE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kShippingRequested, output->stringified_data);
  }
}

TEST(SkipToGPayUtilsTest, NameAlreadyRequested) {
  auto* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  options->setRequestPayerPhone(true);
  options->setRequestPayerEmail(true);
  options->setRequestShipping(true);

  {
    const char kNameRequested[] =
        "{\"apiVersion\":1,"
        "\"cardRequirements\":{\"billingAddressRequired\":true}}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kNameRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":1,\"cardRequirements\":{\"billingAddressRequired\":"
        "true},\"phoneNumberRequired\":true,\"emailRequired\":true,"
        "\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_FALSE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kNameRequested, output->stringified_data);
  }
  {
    const char kNameRequested[] =
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\","
        "\"parameters\":{\"billingAddressRequired\":true}}]}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kNameRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}],"
        "\"emailRequired\":true,\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_FALSE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kNameRequested, output->stringified_data);
  }
}

TEST(SkipToGPayUtilsTest, PhoneAlreadyRequested) {
  auto* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  options->setRequestPayerPhone(true);
  options->setRequestPayerEmail(true);
  options->setRequestShipping(true);

  {
    const char kPhoneRequested[] =
        "{\"apiVersion\":1,"
        "\"phoneNumberRequired\":true}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kPhoneRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":1,\"phoneNumberRequired\":true,\"cardRequirements\":{"
        "\"billingAddressRequired\":true},\"emailRequired\":true,"
        "\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_FALSE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kPhoneRequested, output->stringified_data);
  }
  {
    const char kPhoneRequested[] =
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}]}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kPhoneRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}],"
        "\"emailRequired\":true,\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_FALSE(output->gpay_bridge_data->phone_requested);
    EXPECT_FALSE(output->gpay_bridge_data->name_requested);
    EXPECT_TRUE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kPhoneRequested, output->stringified_data);
  }
}

TEST(SkipToGPayUtilsTest, EmailAlreadyRequested) {
  auto* options = PaymentOptions::Create();
  options->setRequestPayerName(true);
  options->setRequestPayerPhone(true);
  options->setRequestPayerEmail(true);
  options->setRequestShipping(true);

  {
    const char kEmailRequested[] = "{\"apiVersion\":1,\"emailRequired\":true}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kEmailRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":1,\"emailRequired\":true,\"cardRequirements\":{"
        "\"billingAddressRequired\":true},\"phoneNumberRequired\":true,"
        "\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_FALSE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kEmailRequested, output->stringified_data);
  }
  {
    const char kEmailRequested[] =
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{}}],"
        "\"emailRequired\":true}";
    PaymentMethodDataPtr output = MakeTestPaymentMethodData();
    output->stringified_data = kEmailRequested;

    ASSERT_TRUE(SkipToGPayUtils::PatchPaymentMethodData(*options, output));

    EXPECT_EQ(
        "{\"apiVersion\":2,\"apiVersionMinor\":0,\"allowedPaymentMethods\":[{"
        "\"type\":\"CARD\",\"parameters\":{\"billingAddressRequired\":true,"
        "\"billingAddressParameters\":{\"phoneNumberRequired\":true}}}],"
        "\"emailRequired\":true,\"shippingAddressRequired\":true}",
        output->gpay_bridge_data->stringified_data);
    EXPECT_TRUE(output->gpay_bridge_data->phone_requested);
    EXPECT_TRUE(output->gpay_bridge_data->name_requested);
    EXPECT_FALSE(output->gpay_bridge_data->email_requested);
    EXPECT_TRUE(output->gpay_bridge_data->shipping_requested);
    EXPECT_EQ(kEmailRequested, output->stringified_data);
  }
}

}  // namespace
}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_address.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

TEST(PaymentAddressTest, ValuesAreCopiedOver) {
  test::TaskEnvironment task_environment;
  payments::mojom::blink::PaymentAddressPtr input =
      payments::mojom::blink::PaymentAddress::New();
  input->country = "US";
  input->address_line.push_back("340 Main St");
  input->address_line.push_back("BIN1");
  input->address_line.push_back("First floor");
  input->region = "CA";
  input->city = "Los Angeles";
  input->dependent_locality = "Venice";
  input->postal_code = "90291";
  input->sorting_code = "CEDEX";
  input->organization = "Google";
  input->recipient = "Jon Doe";
  input->phone = "Phone Number";

  PaymentAddress* output =
      MakeGarbageCollected<PaymentAddress>(std::move(input));

  EXPECT_EQ("US", output->country());
  EXPECT_EQ(3U, output->addressLine().size());
  EXPECT_EQ("340 Main St", output->addressLine()[0]);
  EXPECT_EQ("BIN1", output->addressLine()[1]);
  EXPECT_EQ("First floor", output->addressLine()[2]);
  EXPECT_EQ("CA", output->region());
  EXPECT_EQ("Los Angeles", output->city());
  EXPECT_EQ("Venice", output->dependentLocality());
  EXPECT_EQ("90291", output->postalCode());
  EXPECT_EQ("CEDEX", output->sortingCode());
  EXPECT_EQ("Google", output->organization());
  EXPECT_EQ("Jon Doe", output->recipient());
  EXPECT_EQ("Phone Number", output->phone());
}

}  // namespace
}  // namespace blink

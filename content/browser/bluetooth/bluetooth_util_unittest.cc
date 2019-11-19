// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_util.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
const char kCyclingPowerUUIDString[] = "00001818-0000-1000-8000-00805f9b34fb";

}  // namespace

class BluetoothUtilTest : public testing::Test {
 public:
  BluetoothUtilTest() = default;
  ~BluetoothUtilTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothUtilTest);
};

TEST_F(BluetoothUtilTest, SameFilters) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoName) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, base::nullopt, "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, base::nullopt, "a");
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasName) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, base::nullopt, "a");
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentName) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "cd", "a");
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoNamePrefix) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(services, "ab",
                                                              base::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(services, "ab",
                                                              base::nullopt);
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasNamePrefix) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(services, "ab",
                                                              base::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentNamePrefix) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "ab");
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoServicesUUID) {
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(base::nullopt, "ab", "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(base::nullopt, "ab", "a");
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasServicesUUID) {
  base::Optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services, "ab", "a");
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(base::nullopt, "ab", "ab");
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentServicesUUID) {
  base::Optional<std::vector<device::BluetoothUUID>> services_1;
  services_1.emplace();
  services_1->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services_1, "ab", "a");

  base::Optional<std::vector<device::BluetoothUUID>> services_2;
  services_2.emplace();
  services_2->push_back(device::BluetoothUUID(kCyclingPowerUUIDString));
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services_2, "ab", "a");

  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, SameServicesUUIDButDifferentOrder) {
  base::Optional<std::vector<device::BluetoothUUID>> services_1;
  services_1.emplace();
  services_1->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  services_1->push_back(device::BluetoothUUID(kCyclingPowerUUIDString));
  auto filter_1 =
      blink::mojom::WebBluetoothLeScanFilter::New(services_1, "ab", "a");

  base::Optional<std::vector<device::BluetoothUUID>> services_2;
  services_2.emplace();
  services_2->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  services_2->push_back(device::BluetoothUUID(kCyclingPowerUUIDString));
  auto filter_2 =
      blink::mojom::WebBluetoothLeScanFilter::New(services_2, "ab", "a");

  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

}  // namespace content

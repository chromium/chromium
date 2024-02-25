// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_util.h"

#include "testing/gtest/include/gtest/gtest.h"

using WebBluetoothManufacturerDataMap =
    base::flat_map<blink::mojom::WebBluetoothCompanyPtr,
                   std::vector<blink::mojom::WebBluetoothDataFilterPtr>>;

namespace content {

namespace {

const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
const char kCyclingPowerUUIDString[] = "00001818-0000-1000-8000-00805f9b34fb";

std::vector<blink::mojom::WebBluetoothDataFilterPtr> CreateDataFilters(
    std::vector<uint8_t> filter_data,
    std::vector<uint8_t> filter_mask) {
  EXPECT_EQ(filter_data.size(), filter_mask.size());
  std::vector<blink::mojom::WebBluetoothDataFilterPtr> data_filters;
  for (size_t i = 0; i < filter_data.size(); ++i) {
    auto data_filter = blink::mojom::WebBluetoothDataFilter::New();
    data_filter->data = filter_data[i];
    data_filter->mask = filter_mask[i];
    data_filters.push_back(std::move(data_filter));
  }
  return data_filters;
}

}  // namespace

class BluetoothUtilTest : public testing::Test {
 public:
  BluetoothUtilTest() = default;

  BluetoothUtilTest(const BluetoothUtilTest&) = delete;
  BluetoothUtilTest& operator=(const BluetoothUtilTest&) = delete;

  ~BluetoothUtilTest() override = default;
};

TEST_F(BluetoothUtilTest, SameFilters) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  std::vector<uint8_t> filter_data = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> filter_mask = {0xff, 0xff, 0xff, 0xff};

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  manufacturer_data_1.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data, filter_mask)});

  WebBluetoothManufacturerDataMap manufacturer_data_2;
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data, filter_mask)});

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoName) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, /*name=*/std::nullopt, "a",
      /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, /*name=*/std::nullopt, "a",
      /*manufacturer_data=*/std::nullopt);
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasName) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, /*name=*/std::nullopt, "a",
      /*manufacturer_data=*/std::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentName) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "cd", "a", /*manufacturer_data=*/std::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoNamePrefix) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", /*name_prefix=*/std::nullopt,
      /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", /*name_prefix=*/std::nullopt,
      /*manufacturer_data=*/std::nullopt);
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasNamePrefix) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", /*name_prefix=*/std::nullopt,
      /*manufacturer_data=*/std::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentNamePrefix) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "ab", /*manufacturer_data=*/std::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoServicesUUID) {
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      /*services=*/std::nullopt, "ab", "a",
      /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      /*services=*/std::nullopt, "ab", "a",
      /*manufacturer_data=*/std::nullopt);
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasServicesUUID) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", /*manufacturer_data=*/std::nullopt);
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      /*services=*/std::nullopt, "ab", "ab",
      /*manufacturer_data=*/std::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentServicesUUID) {
  std::optional<std::vector<device::BluetoothUUID>> services_1;
  services_1.emplace();
  services_1->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services_1, "ab", "a", /*manufacturer_data=*/std::nullopt);

  std::optional<std::vector<device::BluetoothUUID>> services_2;
  services_2.emplace();
  services_2->push_back(device::BluetoothUUID(kCyclingPowerUUIDString));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services_2, "ab", "a", /*manufacturer_data=*/std::nullopt);

  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, SameServicesUUIDButDifferentOrder) {
  std::optional<std::vector<device::BluetoothUUID>> services_1;
  services_1.emplace();
  services_1->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  services_1->push_back(device::BluetoothUUID(kCyclingPowerUUIDString));
  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services_1, "ab", "a", /*manufacturer_data=*/std::nullopt);

  std::optional<std::vector<device::BluetoothUUID>> services_2;
  services_2.emplace();
  services_2->push_back(device::BluetoothUUID(kCyclingPowerUUIDString));
  services_2->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services_2, "ab", "a", /*manufacturer_data=*/std::nullopt);

  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, BothNoManufacturerData) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  WebBluetoothManufacturerDataMap manufacturer_data_2;

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_TRUE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, OnlyOneHasManufacturerData) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  WebBluetoothManufacturerDataMap manufacturer_data;

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", /*manufacturer_data=*/std::nullopt);
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentManufacturerDataSize) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  manufacturer_data_1.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters({}, {})});

  WebBluetoothManufacturerDataMap manufacturer_data_2;
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters({}, {})});
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0002),
                              CreateDataFilters({}, {})});

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentManufacturerDataCompanyIdentifier) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  manufacturer_data_1.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters({}, {})});

  WebBluetoothManufacturerDataMap manufacturer_data_2;
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0002),
                              CreateDataFilters({}, {})});

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentManufacturerDataFilterSize) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  std::vector<uint8_t> filter_data_1 = {0x01};
  std::vector<uint8_t> filter_mask_1 = {0xff};
  manufacturer_data_1.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data_1, filter_mask_1)});

  WebBluetoothManufacturerDataMap manufacturer_data_2;
  std::vector<uint8_t> filter_data_2 = {0x01, 0x02};
  std::vector<uint8_t> filter_mask_2 = {0xff, 0xff};
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data_2, filter_mask_2)});

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentManufacturerData) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  std::vector<uint8_t> filter_mask = {0xff, 0xff, 0xff, 0xff};

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  std::vector<uint8_t> filter_data_1 = {0x01, 0x02, 0x03, 0x04};
  manufacturer_data_1.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data_1, filter_mask)});

  WebBluetoothManufacturerDataMap manufacturer_data_2;
  std::vector<uint8_t> filter_data_2 = {0x05, 0x06, 0x07, 0x08};
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data_2, filter_mask)});

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, DifferentManufacturerDataMask) {
  std::optional<std::vector<device::BluetoothUUID>> services;
  services.emplace();
  services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));

  std::vector<uint8_t> filter_data = {0x01, 0x02, 0x03, 0x04};

  WebBluetoothManufacturerDataMap manufacturer_data_1;
  std::vector<uint8_t> filter_mask_1 = {0xff, 0xff, 0xff, 0xff};
  manufacturer_data_1.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data, filter_mask_1)});

  WebBluetoothManufacturerDataMap manufacturer_data_2;
  std::vector<uint8_t> filter_mask_2 = {0xff, 0xff, 0xff, 0x00};
  manufacturer_data_2.insert({blink::mojom::WebBluetoothCompany::New(0x0001),
                              CreateDataFilters(filter_data, filter_mask_2)});

  auto filter_1 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_1));
  auto filter_2 = blink::mojom::WebBluetoothLeScanFilter::New(
      services, "ab", "a", std::move(manufacturer_data_2));
  EXPECT_FALSE(AreScanFiltersSame(*filter_1, *filter_2));
}

TEST_F(BluetoothUtilTest, MatchesBluetoothDataFilterMatch) {
  // Same data full mask.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0xff));
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x2, 0xff));
    EXPECT_TRUE(MatchesBluetoothDataFilter(filter, {0x1, 0x2}));
  }

  // Same data partial mask.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0x01));
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x2, 0x02));
    EXPECT_TRUE(MatchesBluetoothDataFilter(filter, {0x1, 0x2}));
  }

  // Prefix matches.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0xff));
    EXPECT_TRUE(MatchesBluetoothDataFilter(filter, {0x1, 0x2}));
  }

  // Empty filter matches anything.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    EXPECT_TRUE(MatchesBluetoothDataFilter(filter, {0x1, 0x2}));
    EXPECT_TRUE(MatchesBluetoothDataFilter(filter, {}));
  }
}

TEST_F(BluetoothUtilTest, MatchesBluetoothDataFilterNotMatch) {
  // Different data full mask.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0xff));
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x2, 0xff));
    EXPECT_FALSE(MatchesBluetoothDataFilter(filter, {0x2, 0x2}));
  }

  // Same data partial mask.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0x01));
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x2, 0x02));
    EXPECT_FALSE(MatchesBluetoothDataFilter(filter, {0x2, 0x2}));
  }

  // Prefix doesn't match.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x2, 0xff));
    EXPECT_FALSE(MatchesBluetoothDataFilter(filter, {0x1, 0x2}));
  }

  // Filter is longer than data.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0xff));
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x2, 0xff));
    EXPECT_FALSE(MatchesBluetoothDataFilter(filter, {0x1}));
  }

  // Filter expect there is second byte.
  {
    std::vector<blink::mojom::WebBluetoothDataFilterPtr> filter;
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x1, 0xff));
    filter.push_back(blink::mojom::WebBluetoothDataFilter::New(0x0, 0x00));
    EXPECT_FALSE(MatchesBluetoothDataFilter(filter, {0x1}));
  }
}

}  // namespace content

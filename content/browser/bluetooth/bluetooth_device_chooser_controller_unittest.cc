// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";
constexpr char kBatteryServiceUUIDString[] =
    "0000180f-0000-1000-8000-00805f9b34fb";

namespace content {

class BluetoothDeviceChooserControllerTest : public testing::Test {};

TEST_F(BluetoothDeviceChooserControllerTest, CalculateSignalStrengthLevel) {
  EXPECT_EQ(
      0, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-128));
  EXPECT_EQ(
      0, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-84));

  EXPECT_EQ(
      1, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-83));
  EXPECT_EQ(
      1, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-77));

  EXPECT_EQ(
      2, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-76));
  EXPECT_EQ(
      2, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-69));

  EXPECT_EQ(
      3, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-68));
  EXPECT_EQ(
      3, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-58));

  EXPECT_EQ(
      4, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(-57));
  EXPECT_EQ(
      4, BluetoothDeviceChooserController::CalculateSignalStrengthLevel(127));
}

TEST_F(BluetoothDeviceChooserControllerTest, ComputeScanFilterTest) {
  // No supported OS level filters have a name prefix filter. Since filters are
  // unioned we must not filter any devices if we have a name prefix filter.
  {
    auto filter = blink::mojom::WebBluetoothLeScanFilter::New();
    filter->name_prefix = "test name prefix";

    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> scan_filters;
    scan_filters.emplace_back(std::move(filter));

    std::unique_ptr<device::BluetoothDiscoveryFilter> resulting_filter =
        BluetoothDeviceChooserController::ComputeScanFilter(
            std::move(scan_filters));

    const base::flat_set<device::BluetoothDiscoveryFilter::DeviceInfoFilter>*
        device_info_filters = resulting_filter->GetDeviceFilters();

    EXPECT_TRUE(device_info_filters->empty());
  }
  // Ensures ComputeScanFilter adds all UUIDs to the BluetoothDiscoveryFilter
  // that it creates
  {
    std::vector<device::BluetoothUUID> services;
    services.emplace_back(device::BluetoothUUID(kHeartRateUUIDString));
    services.emplace_back(device::BluetoothUUID(kBatteryServiceUUIDString));

    auto filter = blink::mojom::WebBluetoothLeScanFilter::New();
    filter->services = std::move(services);

    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> scan_filters;
    scan_filters.emplace_back(std::move(filter));

    std::unique_ptr<device::BluetoothDiscoveryFilter> resulting_filter =
        BluetoothDeviceChooserController::ComputeScanFilter(
            std::move(scan_filters));

    const base::flat_set<device::BluetoothDiscoveryFilter::DeviceInfoFilter>*
        device_info_filters = resulting_filter->GetDeviceFilters();

    EXPECT_TRUE(device_info_filters->begin()->uuids.contains(
        device::BluetoothUUID(kHeartRateUUIDString)));
    EXPECT_TRUE(device_info_filters->begin()->uuids.contains(
        device::BluetoothUUID(kBatteryServiceUUIDString)));
    EXPECT_TRUE(device_info_filters->begin()->name.empty());
  }
  // Ensures ComputeScanFilter adds the correct name to the
  // BluetoothDiscoveryFilter that it creates
  {
    auto filter = blink::mojom::WebBluetoothLeScanFilter::New();
    filter->name = "test name";

    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> scan_filters;
    scan_filters.emplace_back(std::move(filter));

    std::unique_ptr<device::BluetoothDiscoveryFilter> resulting_filter =
        BluetoothDeviceChooserController::ComputeScanFilter(
            std::move(scan_filters));

    const base::flat_set<device::BluetoothDiscoveryFilter::DeviceInfoFilter>*
        device_info_filters = resulting_filter->GetDeviceFilters();

    EXPECT_TRUE(device_info_filters->begin()->uuids.empty());
    EXPECT_EQ(device_info_filters->begin()->name, "test name");
  }
  // Ensures ComputeScanFilter adds both the name and the UUIDs to the
  // BluetoothDiscoveryFilter that it creates
  {
    std::vector<device::BluetoothUUID> services;
    services.emplace_back(device::BluetoothUUID(kHeartRateUUIDString));
    services.emplace_back(device::BluetoothUUID(kBatteryServiceUUIDString));

    auto filter = blink::mojom::WebBluetoothLeScanFilter::New();
    filter->services = std::move(services);
    filter->name = "test name";

    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> scan_filters;
    scan_filters.emplace_back(std::move(filter));

    std::unique_ptr<device::BluetoothDiscoveryFilter> resulting_filter =
        BluetoothDeviceChooserController::ComputeScanFilter(
            std::move(scan_filters));

    const base::flat_set<device::BluetoothDiscoveryFilter::DeviceInfoFilter>*
        device_info_filters = resulting_filter->GetDeviceFilters();

    EXPECT_TRUE(device_info_filters->begin()->uuids.contains(
        device::BluetoothUUID(kHeartRateUUIDString)));
    EXPECT_TRUE(device_info_filters->begin()->uuids.contains(
        device::BluetoothUUID(kBatteryServiceUUIDString)));
    EXPECT_EQ(device_info_filters->begin()->name, "test name");
  }
  // Ensures we will not filter any devices if we have at least one
  // name_prefix only filter even with other filters present.
  {
    std::vector<device::BluetoothUUID> services;
    services.emplace_back(device::BluetoothUUID(kHeartRateUUIDString));
    std::vector<device::BluetoothUUID> services2;
    services2.emplace_back(device::BluetoothUUID(kBatteryServiceUUIDString));

    auto filter = blink::mojom::WebBluetoothLeScanFilter::New();
    filter->services = std::move(services);
    filter->name = "test name";

    auto prefix_filter = blink::mojom::WebBluetoothLeScanFilter::New();
    prefix_filter->name_prefix = "test name prefix";

    auto filter2 = blink::mojom::WebBluetoothLeScanFilter::New();
    filter2->services = std::move(services2);
    filter2->name = "test name2";

    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> scan_filters;
    scan_filters.emplace_back(std::move(filter));
    scan_filters.emplace_back(std::move(prefix_filter));

    std::unique_ptr<device::BluetoothDiscoveryFilter> resulting_filter =
        BluetoothDeviceChooserController::ComputeScanFilter(
            std::move(scan_filters));

    const base::flat_set<device::BluetoothDiscoveryFilter::DeviceInfoFilter>*
        device_info_filters = resulting_filter->GetDeviceFilters();

    EXPECT_TRUE(device_info_filters->empty());
  }
}

}  // namespace content

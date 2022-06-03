// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices.h"

#include "base/strings/string_util.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "url/gurl.h"

using device::BluetoothUUID;

namespace content {
namespace {
const std::string kDeviceAddress1 = "00:00:00";
const std::string kDeviceAddress2 = "11:11:11";

const std::string kDeviceName = "TestName";

const char kGlucoseUUIDString[] = "00001808-0000-1000-8000-00805f9b34fb";
const char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";
const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
const char kBloodPressureUUIDString[] = "00001813-0000-1000-8000-00805f9b34fb";
const char kCyclingPowerUUIDString[] = "00001818-0000-1000-8000-00805f9b34fb";
const BluetoothUUID kGlucoseUUID(kGlucoseUUIDString);
const BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
const BluetoothUUID kBatteryServiceUUID(kBatteryServiceUUIDString);
const BluetoothUUID kBloodPressureUUID(kBloodPressureUUIDString);
const BluetoothUUID kCyclingPowerUUID(kCyclingPowerUUIDString);

class BluetoothAllowedDevicesTest : public testing::Test {
 protected:
  BluetoothAllowedDevicesTest() {
    empty_options_ = blink::mojom::WebBluetoothRequestDeviceOptions::New();
  }

  ~BluetoothAllowedDevicesTest() override {}

  blink::mojom::WebBluetoothRequestDeviceOptionsPtr empty_options_;
};

}  // namespace

TEST_F(BluetoothAllowedDevicesTest, UniqueOriginNotSupported) {
  auto allowed_devices_map = std::make_unique<BluetoothAllowedDevicesMap>();
  EXPECT_DEATH_IF_SUPPORTED(
      allowed_devices_map->GetOrCreateAllowedDevices(url::Origin()), "");
}

TEST_F(BluetoothAllowedDevicesTest, AddDevice) {
  BluetoothAllowedDevices allowed_devices;

  const blink::WebBluetoothDeviceId& device_id =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id, *allowed_devices.GetDeviceId(kDeviceAddress1));
  EXPECT_EQ(kDeviceAddress1, allowed_devices.GetDeviceAddress(device_id));
}

TEST_F(BluetoothAllowedDevicesTest, AddDeviceTwice) {
  BluetoothAllowedDevices allowed_devices;
  const blink::WebBluetoothDeviceId& device_id1 =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);
  const blink::WebBluetoothDeviceId& device_id2 =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);

  EXPECT_EQ(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1, *allowed_devices.GetDeviceId(kDeviceAddress1));
  EXPECT_EQ(kDeviceAddress1, allowed_devices.GetDeviceAddress(device_id1));
}

TEST_F(BluetoothAllowedDevicesTest, AddTwoDevices) {
  BluetoothAllowedDevices allowed_devices;
  const blink::WebBluetoothDeviceId& device_id1 =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);
  const blink::WebBluetoothDeviceId& device_id2 =
      allowed_devices.AddDevice(kDeviceAddress2, empty_options_);

  EXPECT_NE(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1, *allowed_devices.GetDeviceId(kDeviceAddress1));
  EXPECT_EQ(device_id2, *allowed_devices.GetDeviceId(kDeviceAddress2));

  EXPECT_EQ(kDeviceAddress1, allowed_devices.GetDeviceAddress(device_id1));
  EXPECT_EQ(kDeviceAddress2, allowed_devices.GetDeviceAddress(device_id2));
}

TEST_F(BluetoothAllowedDevicesTest, AddTwoDevicesFromTwoOriginsToMap) {
  auto allowed_devices_map = std::make_unique<BluetoothAllowedDevicesMap>();
  content::BluetoothAllowedDevices& allowed_devices1 =
      allowed_devices_map->GetOrCreateAllowedDevices(
          url::Origin::Create(GURL("https://www.example1.com")));
  content::BluetoothAllowedDevices& allowed_devices2 =
      allowed_devices_map->GetOrCreateAllowedDevices(
          url::Origin::Create(GURL("https://www.example2.com")));

  const blink::WebBluetoothDeviceId& device_id1 =
      allowed_devices1.AddDevice(kDeviceAddress1, empty_options_);
  const blink::WebBluetoothDeviceId& device_id2 =
      allowed_devices2.AddDevice(kDeviceAddress2, empty_options_);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.
  EXPECT_EQ(nullptr, allowed_devices1.GetDeviceId(kDeviceAddress2));
  EXPECT_EQ(nullptr, allowed_devices2.GetDeviceId(kDeviceAddress1));

  EXPECT_EQ(std::string(), allowed_devices1.GetDeviceAddress(device_id2));
  EXPECT_EQ(std::string(), allowed_devices2.GetDeviceAddress(device_id1));

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1, *(allowed_devices1.GetDeviceId(kDeviceAddress1)));
  EXPECT_EQ(device_id2, *(allowed_devices2.GetDeviceId(kDeviceAddress2)));

  EXPECT_EQ(kDeviceAddress1, allowed_devices1.GetDeviceAddress(device_id1));
  EXPECT_EQ(kDeviceAddress2, allowed_devices2.GetDeviceAddress(device_id2));
}

TEST_F(BluetoothAllowedDevicesTest, AddDeviceFromTwoOriginsToMap) {
  auto allowed_devices_map = std::make_unique<BluetoothAllowedDevicesMap>();
  content::BluetoothAllowedDevices& allowed_devices1 =
      allowed_devices_map->GetOrCreateAllowedDevices(
          url::Origin::Create(GURL("https://www.example1.com")));
  content::BluetoothAllowedDevices& allowed_devices2 =
      allowed_devices_map->GetOrCreateAllowedDevices(
          url::Origin::Create(GURL("https://www.example2.com")));

  const blink::WebBluetoothDeviceId& device_id1 =
      allowed_devices1.AddDevice(kDeviceAddress1, empty_options_);
  const blink::WebBluetoothDeviceId& device_id2 =
      allowed_devices2.AddDevice(kDeviceAddress1, empty_options_);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.
  EXPECT_EQ(std::string(), allowed_devices1.GetDeviceAddress(device_id2));
  EXPECT_EQ(std::string(), allowed_devices2.GetDeviceAddress(device_id1));
}

TEST_F(BluetoothAllowedDevicesTest, AddRemoveAddDevice) {
  BluetoothAllowedDevices allowed_devices;
  const blink::WebBluetoothDeviceId device_id_first_time =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);

  allowed_devices.RemoveDevice(kDeviceAddress1);

  const blink::WebBluetoothDeviceId device_id_second_time =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);

  EXPECT_NE(device_id_first_time, device_id_second_time);
}

TEST_F(BluetoothAllowedDevicesTest, RemoveDevice) {
  BluetoothAllowedDevices allowed_devices;

  const blink::WebBluetoothDeviceId device_id =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);

  allowed_devices.RemoveDevice(kDeviceAddress1);

  EXPECT_EQ(nullptr, allowed_devices.GetDeviceId(kDeviceAddress1));
  EXPECT_EQ(std::string(), allowed_devices.GetDeviceAddress(device_id));
}

TEST_F(BluetoothAllowedDevicesTest, NoPermissionForAnyService) {
  BluetoothAllowedDevices allowed_devices;

  // Setup device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter->name = kDeviceName;
  options->filters.emplace();
  options->filters->push_back({scan_filter.Clone()});

  // Add to map.
  const blink::WebBluetoothDeviceId device_id =
      allowed_devices.AddDevice(kDeviceAddress1, options);

  // Try to access at least one service.
  EXPECT_FALSE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id, kGlucoseUUID));
}

TEST_F(BluetoothAllowedDevicesTest, AllowedServices_OneDevice) {
  BluetoothAllowedDevices allowed_devices;

  // Setup device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter1 =
      blink::mojom::WebBluetoothLeScanFilter::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter2 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter1->services.emplace();
  scan_filter1->services->push_back(kGlucoseUUID);
  options->filters.emplace();
  options->filters->push_back(scan_filter1.Clone());

  scan_filter2->services.emplace();
  scan_filter2->services->push_back(kHeartRateUUID);
  options->filters->push_back(scan_filter2.Clone());

  options->optional_services.push_back(kBatteryServiceUUID);
  options->optional_services.push_back(kHeartRateUUID);

  // Add to map.
  const blink::WebBluetoothDeviceId device_id1 =
      allowed_devices.AddDevice(kDeviceAddress1, options);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id1));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kHeartRateUUID));
  EXPECT_TRUE(allowed_devices.IsAllowedToAccessService(device_id1,
                                                       kBatteryServiceUUID));

  // Try to access a non-allowed service.
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kBloodPressureUUID));

  // Try to access allowed services after removing device.
  allowed_devices.RemoveDevice(kDeviceAddress1);

  EXPECT_FALSE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id1));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices.IsAllowedToAccessService(device_id1,
                                                        kBatteryServiceUUID));

  // Add device back.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();

  options2->filters.emplace();
  options2->filters->push_back(scan_filter1.Clone());
  options2->filters->push_back(scan_filter2.Clone());

  const blink::WebBluetoothDeviceId device_id2 =
      allowed_devices.AddDevice(kDeviceAddress1, options2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id2));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id2, kGlucoseUUID));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id2, kHeartRateUUID));

  // Try to access a non-allowed service.
  EXPECT_FALSE(allowed_devices.IsAllowedToAccessService(device_id2,
                                                        kBatteryServiceUUID));

  // Try to access services from old device.
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices.IsAllowedToAccessService(device_id1,
                                                        kBatteryServiceUUID));
}

TEST_F(BluetoothAllowedDevicesTest, AllowedServices_TwoDevices) {
  BluetoothAllowedDevices allowed_devices;

  // Setup request for device #1.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options1 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter1 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter1->services.emplace();
  scan_filter1->services->push_back(kGlucoseUUID);
  options1->filters.emplace();
  options1->filters->push_back(std::move(scan_filter1));

  options1->optional_services.push_back(kHeartRateUUID);

  // Setup request for device #2.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter2 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter2->services.emplace();
  scan_filter2->services->push_back(kBatteryServiceUUID);
  options2->filters.emplace();
  options2->filters->push_back(std::move(scan_filter2));

  options2->optional_services.push_back(kBloodPressureUUID);

  // Add devices to map.
  const blink::WebBluetoothDeviceId& device_id1 =
      allowed_devices.AddDevice(kDeviceAddress1, options1);
  const blink::WebBluetoothDeviceId& device_id2 =
      allowed_devices.AddDevice(kDeviceAddress2, options2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id1));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kHeartRateUUID));

  EXPECT_TRUE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id2));
  EXPECT_TRUE(allowed_devices.IsAllowedToAccessService(device_id2,
                                                       kBatteryServiceUUID));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id2, kBloodPressureUUID));

  // Try to access non-allowed services.
  EXPECT_FALSE(allowed_devices.IsAllowedToAccessService(device_id1,
                                                        kBatteryServiceUUID));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kBloodPressureUUID));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id1, kCyclingPowerUUID));

  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id2, kGlucoseUUID));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id2, kHeartRateUUID));
  EXPECT_FALSE(
      allowed_devices.IsAllowedToAccessService(device_id2, kCyclingPowerUUID));
}

TEST_F(BluetoothAllowedDevicesTest, AllowedServices_TwoOriginsOneDevice) {
  auto allowed_devices_map = std::make_unique<BluetoothAllowedDevicesMap>();
  content::BluetoothAllowedDevices& allowed_devices1 =
      allowed_devices_map->GetOrCreateAllowedDevices(
          url::Origin::Create(GURL("https://www.example1.com")));
  content::BluetoothAllowedDevices& allowed_devices2 =
      allowed_devices_map->GetOrCreateAllowedDevices(
          url::Origin::Create(GURL("https://www.example2.com")));
  // Setup request #1 for device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options1 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter1 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter1->services.emplace();
  scan_filter1->services->push_back(kGlucoseUUID);
  options1->filters.emplace();
  options1->filters->push_back(std::move(scan_filter1));

  options1->optional_services.push_back(kHeartRateUUID);

  // Setup request #2 for device.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter2 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter2->services.emplace();
  scan_filter2->services->push_back(kBatteryServiceUUID);
  options2->filters.emplace();
  options2->filters->push_back(std::move(scan_filter2));

  options2->optional_services.push_back(kBloodPressureUUID);

  // Add devices to map.
  const blink::WebBluetoothDeviceId& device_id1 =
      allowed_devices1.AddDevice(kDeviceAddress1, options1);
  const blink::WebBluetoothDeviceId& device_id2 =
      allowed_devices2.AddDevice(kDeviceAddress1, options2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices1.IsAllowedToAccessAtLeastOneService(device_id1));
  EXPECT_TRUE(
      allowed_devices1.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_TRUE(
      allowed_devices1.IsAllowedToAccessService(device_id1, kHeartRateUUID));

  EXPECT_TRUE(allowed_devices2.IsAllowedToAccessAtLeastOneService(device_id2));
  EXPECT_TRUE(allowed_devices2.IsAllowedToAccessService(device_id2,
                                                        kBatteryServiceUUID));
  EXPECT_TRUE(allowed_devices2.IsAllowedToAccessService(device_id2,
                                                        kBloodPressureUUID));

  // Try to access non-allowed services.
  EXPECT_FALSE(allowed_devices1.IsAllowedToAccessService(device_id1,
                                                         kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices1.IsAllowedToAccessService(device_id1,
                                                         kBloodPressureUUID));

  EXPECT_FALSE(allowed_devices1.IsAllowedToAccessAtLeastOneService(device_id2));
  EXPECT_FALSE(
      allowed_devices1.IsAllowedToAccessService(device_id2, kGlucoseUUID));
  EXPECT_FALSE(
      allowed_devices1.IsAllowedToAccessService(device_id2, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices1.IsAllowedToAccessService(device_id2,
                                                         kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices1.IsAllowedToAccessService(device_id2,
                                                         kBloodPressureUUID));

  EXPECT_FALSE(
      allowed_devices2.IsAllowedToAccessService(device_id2, kGlucoseUUID));
  EXPECT_FALSE(
      allowed_devices2.IsAllowedToAccessService(device_id2, kHeartRateUUID));

  EXPECT_FALSE(allowed_devices2.IsAllowedToAccessAtLeastOneService(device_id1));
  EXPECT_FALSE(
      allowed_devices2.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_FALSE(
      allowed_devices2.IsAllowedToAccessService(device_id1, kHeartRateUUID));
  EXPECT_FALSE(allowed_devices2.IsAllowedToAccessService(device_id1,
                                                         kBatteryServiceUUID));
  EXPECT_FALSE(allowed_devices2.IsAllowedToAccessService(device_id1,
                                                         kBloodPressureUUID));
}

TEST_F(BluetoothAllowedDevicesTest, MergeServices) {
  BluetoothAllowedDevices allowed_devices;

  // Setup first request.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options1 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter1 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter1->services.emplace();
  scan_filter1->services->push_back(kGlucoseUUID);
  options1->filters.emplace();
  options1->filters->push_back(std::move(scan_filter1));

  options1->optional_services.push_back(kBatteryServiceUUID);

  // Add to map.
  const blink::WebBluetoothDeviceId device_id1 =
      allowed_devices.AddDevice(kDeviceAddress1, options1);

  // Setup second request.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options2 =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter2 =
      blink::mojom::WebBluetoothLeScanFilter::New();

  scan_filter2->services.emplace();
  scan_filter2->services->push_back(kHeartRateUUID);
  options2->filters.emplace();
  options2->filters->push_back(std::move(scan_filter2));

  options2->optional_services.push_back(kBloodPressureUUID);

  // Add to map again.
  const blink::WebBluetoothDeviceId device_id2 =
      allowed_devices.AddDevice(kDeviceAddress1, options2);

  EXPECT_EQ(device_id1, device_id2);

  EXPECT_TRUE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id1));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kGlucoseUUID));
  EXPECT_TRUE(allowed_devices.IsAllowedToAccessService(device_id1,
                                                       kBatteryServiceUUID));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kHeartRateUUID));
  EXPECT_TRUE(
      allowed_devices.IsAllowedToAccessService(device_id1, kBloodPressureUUID));
}

TEST_F(BluetoothAllowedDevicesTest, CorrectIdFormat) {
  BluetoothAllowedDevices allowed_devices;

  const blink::WebBluetoothDeviceId& device_id =
      allowed_devices.AddDevice(kDeviceAddress1, empty_options_);

  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(device_id.str()));
}

TEST_F(BluetoothAllowedDevicesTest, NoFilterServices) {
  BluetoothAllowedDevices allowed_devices;

  // Setup request.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options =
      blink::mojom::WebBluetoothRequestDeviceOptions::New();
  blink::mojom::WebBluetoothLeScanFilterPtr scan_filter =
      blink::mojom::WebBluetoothLeScanFilter::New();

  options->filters.emplace();
  options->filters->push_back(std::move(scan_filter));

  // Add to map.
  const blink::WebBluetoothDeviceId device_id =
      allowed_devices.AddDevice(kDeviceAddress1, options);

  EXPECT_FALSE(allowed_devices.IsAllowedToAccessAtLeastOneService(device_id));
}

}  // namespace content

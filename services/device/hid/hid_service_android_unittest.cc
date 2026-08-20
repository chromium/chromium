// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_android.h"

#include <algorithm>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class HidServiceAndroidTest : public testing::Test {
 public:
  HidServiceAndroidTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(HidServiceAndroidTest, OnAllDevicesRemoved) {
  auto service = std::make_unique<HidServiceAndroid>();

  service->OnDeviceAdded(/*device_id=*/1, /*vendor_id=*/0x1234,
                         /*product_id=*/0x5678, /*product_name=*/"Test Device",
                         /*serial_number=*/"12345",
                         /*physical_address=*/"", /*transport_type=*/3,
                         /*report_descriptor=*/{});

  // Verify the device was added.
  base::test::TestFuture<std::vector<mojom::HidDeviceInfoPtr>> add_future;
  service->GetDevices(add_future.GetCallback());
  EXPECT_FALSE(add_future.Get().empty());

  // Call OnAllDevicesRemoved and verify the map is cleared.
  service->OnAllDevicesRemoved();
  base::test::TestFuture<std::vector<mojom::HidDeviceInfoPtr>> remove_future;
  service->GetDevices(remove_future.GetCallback());
  EXPECT_TRUE(remove_future.Get().empty());
}

TEST_F(HidServiceAndroidTest, PhysicalDeviceIdBluetooth) {
  auto service = std::make_unique<HidServiceAndroid>();

  constexpr int kTransportBluetooth = 5;
  // Two Bluetooth devices connected to the same host adapter share the same
  // host MAC physical_address, but have unique device serial_number MACs.
  service->OnDeviceAdded(/*device_id=*/1, /*vendor_id=*/0x1234,
                         /*product_id=*/0x5678,
                         /*product_name=*/"BT Device 1",
                         /*serial_number=*/"AA:BB:CC:DD:EE:01",
                         /*physical_address=*/"11:22:33:44:55:66",
                         /*transport_type=*/kTransportBluetooth,
                         /*report_descriptor=*/{});

  service->OnDeviceAdded(/*device_id=*/2, /*vendor_id=*/0x1234,
                         /*product_id=*/0x5678,
                         /*product_name=*/"BT Device 2",
                         /*serial_number=*/"AA:BB:CC:DD:EE:02",
                         /*physical_address=*/"11:22:33:44:55:66",
                         /*transport_type=*/kTransportBluetooth,
                         /*report_descriptor=*/{});

  base::test::TestFuture<std::vector<mojom::HidDeviceInfoPtr>> future;
  service->GetDevices(future.GetCallback());
  auto devices = future.Take();
  ASSERT_EQ(devices.size(), 2u);
  std::ranges::sort(devices, {}, &mojom::HidDeviceInfo::product_name);

  EXPECT_EQ(devices[0]->physical_device_id, "AA:BB:CC:DD:EE:01");
  EXPECT_EQ(devices[1]->physical_device_id, "AA:BB:CC:DD:EE:02");
  EXPECT_NE(devices[0]->physical_device_id, devices[1]->physical_device_id);
}

TEST_F(HidServiceAndroidTest, PhysicalDeviceIdUsb) {
  auto service = std::make_unique<HidServiceAndroid>();

  constexpr int kTransportUsb = 3;
  service->OnDeviceAdded(/*device_id=*/1, /*vendor_id=*/0x1234,
                         /*product_id=*/0x5678,
                         /*product_name=*/"USB Device",
                         /*serial_number=*/"",
                         /*physical_address=*/"usb-xhci-1.2",
                         /*transport_type=*/kTransportUsb,
                         /*report_descriptor=*/{});

  base::test::TestFuture<std::vector<mojom::HidDeviceInfoPtr>> future;
  service->GetDevices(future.GetCallback());
  const auto& devices = future.Get();
  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]->physical_device_id, "usb-xhci-1.2");
}

TEST_F(HidServiceAndroidTest, PhysicalDeviceIdFallback) {
  auto service = std::make_unique<HidServiceAndroid>();

  constexpr int kTransportUsb = 3;
  // If neither serial_number nor physical_address is available, fallback to
  // platform_device_id ("1").
  service->OnDeviceAdded(/*device_id=*/1, /*vendor_id=*/0x1234,
                         /*product_id=*/0x5678,
                         /*product_name=*/"Generic Device",
                         /*serial_number=*/"",
                         /*physical_address=*/"",
                         /*transport_type=*/kTransportUsb,
                         /*report_descriptor=*/{});

  base::test::TestFuture<std::vector<mojom::HidDeviceInfoPtr>> future;
  service->GetDevices(future.GetCallback());
  const auto& devices = future.Get();
  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]->physical_device_id, "1");
}

}  // namespace device

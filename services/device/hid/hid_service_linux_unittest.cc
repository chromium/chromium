// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/hid/hid_service_linux.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr char kSubsystemBluetooth[] = "bluetooth";
constexpr char kSubsystemHid[] = "hid";
constexpr char kSubsystemHidraw[] = "hidraw";
constexpr char kSubsystemMisc[] = "misc";
constexpr char kSubsystemUsb[] = "usb";

constexpr char kDevnodeHidraw0[] = "/dev/hidraw0";

constexpr char kDevtypeUsbDevice[] = "usb_device";
constexpr char kDevtypeUsbInterface[] = "usb_interface";
constexpr char kDevtypeLink[] = "link";

constexpr char kPropertyValueHidName[] = "product-name";

class HidServiceLinuxTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void AddFakeDevice(base::FilePath syspath,
                     std::string subsystem,
                     std::optional<std::string> devnode = std::nullopt,
                     std::optional<std::string> devtype = std::nullopt,
                     std::map<std::string, std::string> properties = {}) {
    fake_udev_.AddFakeDevice("fake-device", syspath.value(),
                             std::move(subsystem), std::move(devnode),
                             std::move(devtype), /*sysattrs=*/{},
                             std::move(properties));
  }

  base::FilePath GetTempDir() { return temp_dir_.GetPath(); }

 private:
  testing::FakeUdevLoader fake_udev_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(HidServiceLinuxTest, EnumerateUsbHidDevice) {
  constexpr char kPropertyValueHidId[] = "0003:0000ABCD:00001234";
  constexpr char kPropertyValueHidUniq[] = "serial-number";

  // Construct paths for the hidraw device and its ancestors up to the USB
  // device.
  auto usb_device_path =
      GetTempDir().Append("sys/devices/pci0000:00/0000:00:0.0/usb2/1-1");
  auto usb_interface_path = usb_device_path.Append("1-1:1.1");
  auto hid_path = usb_interface_path.Append("0003:ABCD:1234.0000");
  auto hidraw_path = hid_path.Append("hidraw/hidraw0");

  // The HID service reads the report descriptor from a file in the |hid_path|
  // directory.
  auto report_descriptor_path = hid_path.Append("report_descriptor");
  uint8_t data = 0;
  ASSERT_TRUE(base::CreateDirectory(hid_path));
  ASSERT_TRUE(
      base::WriteFile(report_descriptor_path, base::span_from_ref(data)));

  // Add the fake HID device as well as its ancestors up to the USB device node.
  // Ancestors must be added starting from the closest to the root to ensure
  // that ancestor device info is available when the hidraw device is added.
  AddFakeDevice(usb_device_path, kSubsystemUsb, /*devnode=*/std::nullopt,
                kDevtypeUsbDevice);
  AddFakeDevice(usb_interface_path, kSubsystemUsb, /*devnode=*/std::nullopt,
                kDevtypeUsbInterface);
  AddFakeDevice(hid_path, kSubsystemHid, /*devnode=*/std::nullopt,
                /*devtype=*/std::nullopt, /*properties=*/
                {
                    {"HID_ID", kPropertyValueHidId},
                    {"HID_UNIQ", kPropertyValueHidUniq},
                    {"HID_NAME", kPropertyValueHidName},
                });
  AddFakeDevice(hidraw_path, kSubsystemHidraw, kDevnodeHidraw0);

  std::vector<mojom::HidDeviceInfoPtr> devices;
  base::RunLoop loop;
  auto hid_service = std::make_unique<HidServiceLinux>();
  hid_service->GetDevices(
      base::BindLambdaForTesting([&](std::vector<mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        loop.Quit();
      }));
  loop.Run();

  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]->physical_device_id, usb_device_path.value());
  EXPECT_EQ(devices[0]->vendor_id, 0xabcd);
  EXPECT_EQ(devices[0]->product_id, 0x1234);
  EXPECT_EQ(devices[0]->product_name, kPropertyValueHidName);
  EXPECT_EQ(devices[0]->serial_number, kPropertyValueHidUniq);
  EXPECT_EQ(devices[0]->bus_type, mojom::HidBusType::kHIDBusTypeUSB);
  ASSERT_EQ(devices[0]->report_descriptor.size(), 1u);
  EXPECT_EQ(devices[0]->report_descriptor[0], 0u);
  EXPECT_TRUE(devices[0]->collections.empty());
  EXPECT_FALSE(devices[0]->has_report_id);
  EXPECT_EQ(devices[0]->max_input_report_size, 0u);
  EXPECT_EQ(devices[0]->max_output_report_size, 0u);
  EXPECT_EQ(devices[0]->max_feature_report_size, 0u);
  EXPECT_EQ(devices[0]->device_node, kDevnodeHidraw0);
  EXPECT_TRUE(devices[0]->protected_input_report_ids);
  EXPECT_TRUE(devices[0]->protected_input_report_ids->empty());
  EXPECT_TRUE(devices[0]->protected_output_report_ids);
  EXPECT_TRUE(devices[0]->protected_output_report_ids->empty());
  EXPECT_TRUE(devices[0]->protected_feature_report_ids);
  EXPECT_TRUE(devices[0]->protected_feature_report_ids->empty());
}

TEST_F(HidServiceLinuxTest, EnumerateBluetoothClassicHidDevice) {
  constexpr char kPropertyValueHidId[] = "0005:0000ABCD:00001234";
  constexpr char kPropertyValueHidUniq[] = "aa:bb:cc:dd:ee:ff";

  // Construct paths for the hidraw device and its ancestors up to the Bluetooth
  // link.
  auto bt_link_path = GetTempDir().Append(
      "sys/devices/pci0000:00/0000:00:0.0/usb2/1-1/1-1:1.0/bluetooth/hci0/"
      "hci0:0");
  auto hid_path = bt_link_path.Append("0005:ABCD:1234.0000");
  auto hidraw_path = hid_path.Append("hidraw/hidraw0");

  // The HID service reads the report descriptor from a file in the |hid_path|
  // directory.
  auto report_descriptor_path = hid_path.Append("report_descriptor");
  uint8_t data = 0;
  ASSERT_TRUE(base::CreateDirectory(hid_path));
  ASSERT_TRUE(
      base::WriteFile(report_descriptor_path, base::span_from_ref(data)));

  // Add the fake HID device as well as its ancestors up to the Bluetooth link.
  // Ancestors must be added starting from the closest to the root to ensure
  // that ancestor device info is available when the hidraw device is added.
  AddFakeDevice(bt_link_path, kSubsystemBluetooth, /*devnode=*/std::nullopt,
                kDevtypeLink);
  AddFakeDevice(hid_path, kSubsystemHid, /*devnode=*/std::nullopt,
                /*devtype=*/std::nullopt, /*properties=*/
                {
                    {"HID_ID", kPropertyValueHidId},
                    {"HID_UNIQ", kPropertyValueHidUniq},
                    {"HID_NAME", kPropertyValueHidName},
                });
  AddFakeDevice(hidraw_path, kSubsystemHidraw, kDevnodeHidraw0);

  std::vector<mojom::HidDeviceInfoPtr> devices;
  base::RunLoop loop;
  auto hid_service = std::make_unique<HidServiceLinux>();
  hid_service->GetDevices(
      base::BindLambdaForTesting([&](std::vector<mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        loop.Quit();
      }));
  loop.Run();

  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]->physical_device_id, hid_path.value());
  EXPECT_EQ(devices[0]->vendor_id, 0xabcd);
  EXPECT_EQ(devices[0]->product_id, 0x1234);
  EXPECT_EQ(devices[0]->product_name, kPropertyValueHidName);
  EXPECT_EQ(devices[0]->serial_number, kPropertyValueHidUniq);
  EXPECT_EQ(devices[0]->bus_type, mojom::HidBusType::kHIDBusTypeBluetooth);
  ASSERT_EQ(devices[0]->report_descriptor.size(), 1u);
  EXPECT_EQ(devices[0]->report_descriptor[0], 0u);
  EXPECT_TRUE(devices[0]->collections.empty());
  EXPECT_FALSE(devices[0]->has_report_id);
  EXPECT_EQ(devices[0]->max_input_report_size, 0u);
  EXPECT_EQ(devices[0]->max_output_report_size, 0u);
  EXPECT_EQ(devices[0]->max_feature_report_size, 0u);
  EXPECT_EQ(devices[0]->device_node, kDevnodeHidraw0);
  EXPECT_TRUE(devices[0]->protected_input_report_ids);
  EXPECT_TRUE(devices[0]->protected_input_report_ids->empty());
  EXPECT_TRUE(devices[0]->protected_output_report_ids);
  EXPECT_TRUE(devices[0]->protected_output_report_ids->empty());
  EXPECT_TRUE(devices[0]->protected_feature_report_ids);
  EXPECT_TRUE(devices[0]->protected_feature_report_ids->empty());
}

TEST_F(HidServiceLinuxTest, EnumerateBleHidDevice) {
  constexpr char kPropertyValueHidId[] = "0005:0000ABCD:00001234";
  constexpr char kPropertyValueHidUniq[] = "aa:bb:cc:dd:ee:ff";

  // Construct paths for the hidraw device and its ancestors up to the uhid
  // device node.
  auto uhid_path = GetTempDir().Append("sys/devices/virtual/misc/uhid/");
  auto hid_path = uhid_path.Append("0005:ABCD:1234.0000");
  auto hidraw_path = hid_path.Append("hidraw/hidraw0");

  // The HID service reads the report descriptor from a file in the |hid_path|
  // directory.
  auto report_descriptor_path = hid_path.Append("report_descriptor");
  uint8_t data = 0;
  ASSERT_TRUE(base::CreateDirectory(hid_path));
  ASSERT_TRUE(
      base::WriteFile(report_descriptor_path, base::span_from_ref(data)));

  // Add the fake HID device as well as its ancestors up to the Bluetooth link.
  // Ancestors must be added starting from the closest to the root to ensure
  // that ancestor device info is available when the hidraw device is added.
  AddFakeDevice(uhid_path, kSubsystemMisc);
  AddFakeDevice(hid_path, kSubsystemHid, /*devnode=*/std::nullopt,
                /*devtype=*/std::nullopt, /*properties=*/
                {
                    {"HID_ID", kPropertyValueHidId},
                    {"HID_UNIQ", kPropertyValueHidUniq},
                    {"HID_NAME", kPropertyValueHidName},
                });
  AddFakeDevice(hidraw_path, kSubsystemHidraw, kDevnodeHidraw0);

  std::vector<mojom::HidDeviceInfoPtr> devices;
  base::RunLoop loop;
  auto hid_service = std::make_unique<HidServiceLinux>();
  hid_service->GetDevices(
      base::BindLambdaForTesting([&](std::vector<mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        loop.Quit();
      }));
  loop.Run();

  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0]->physical_device_id, hid_path.value());
  EXPECT_EQ(devices[0]->vendor_id, 0xabcd);
  EXPECT_EQ(devices[0]->product_id, 0x1234);
  EXPECT_EQ(devices[0]->product_name, kPropertyValueHidName);
  EXPECT_EQ(devices[0]->serial_number, kPropertyValueHidUniq);
  EXPECT_EQ(devices[0]->bus_type, mojom::HidBusType::kHIDBusTypeBluetooth);
  ASSERT_EQ(devices[0]->report_descriptor.size(), 1u);
  EXPECT_EQ(devices[0]->report_descriptor[0], 0u);
  EXPECT_TRUE(devices[0]->collections.empty());
  EXPECT_FALSE(devices[0]->has_report_id);
  EXPECT_EQ(devices[0]->max_input_report_size, 0u);
  EXPECT_EQ(devices[0]->max_output_report_size, 0u);
  EXPECT_EQ(devices[0]->max_feature_report_size, 0u);
  EXPECT_EQ(devices[0]->device_node, kDevnodeHidraw0);
  EXPECT_TRUE(devices[0]->protected_input_report_ids);
  EXPECT_TRUE(devices[0]->protected_input_report_ids->empty());
  EXPECT_TRUE(devices[0]->protected_output_report_ids);
  EXPECT_TRUE(devices[0]->protected_output_report_ids->empty());
  EXPECT_TRUE(devices[0]->protected_feature_report_ids);
  EXPECT_TRUE(devices[0]->protected_feature_report_ids->empty());
}

}  // namespace device

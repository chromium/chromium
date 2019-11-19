// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_manager_test.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/webusb_descriptors.h"
#include "url/gurl.h"

namespace device {
namespace usb {
namespace {

class TestUsbDevice : public UsbDevice {
 public:
  TestUsbDevice(const std::string& name,
                const std::string& serial_number,
                const GURL& landing_page);

  // device::UsbDevice overrides:
  void Open(OpenCallback callback) override;

 private:
  ~TestUsbDevice() override;

  DISALLOW_COPY_AND_ASSIGN(TestUsbDevice);
};

TestUsbDevice::TestUsbDevice(const std::string& name,
                             const std::string& serial_number,
                             const GURL& landing_page)
    : UsbDevice(0x0210,
                0xff,
                0xff,
                0xff,
                0x0000,
                0x000,
                0x0100,
                base::string16(),
                base::UTF8ToUTF16(name),
                base::UTF8ToUTF16(serial_number),
                0,
                0) {
  device_info_->webusb_landing_page = landing_page;
}

void TestUsbDevice::Open(OpenCallback callback) {
  std::move(callback).Run(nullptr);
}

TestUsbDevice::~TestUsbDevice() {}

}  // namespace

DeviceManagerTest::DeviceManagerTest(UsbService* usb_service)
    : usb_service_(usb_service) {}

DeviceManagerTest::~DeviceManagerTest() {}

void DeviceManagerTest::BindReceiver(
    mojo::PendingReceiver<device::mojom::UsbDeviceManagerTest> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceManagerTest::AddDeviceForTesting(
    const std::string& name,
    const std::string& serial_number,
    const std::string& landing_page,
    AddDeviceForTestingCallback callback) {
  if (usb_service_) {
    GURL landing_page_url(landing_page);
    if (!landing_page_url.is_valid()) {
      std::move(callback).Run(false, "Landing page URL is invalid.");
      return;
    }

    usb_service_->AddDeviceForTesting(
        new TestUsbDevice(name, serial_number, landing_page_url));
    std::move(callback).Run(true, "Added.");
  } else {
    std::move(callback).Run(false, "USB service unavailable.");
  }
}

void DeviceManagerTest::RemoveDeviceForTesting(
    const std::string& guid,
    RemoveDeviceForTestingCallback callback) {
  if (usb_service_)
    usb_service_->RemoveDeviceForTesting(guid);

  std::move(callback).Run();
}

void DeviceManagerTest::GetTestDevices(GetTestDevicesCallback callback) {
  std::vector<scoped_refptr<UsbDevice>> devices;
  if (usb_service_)
    usb_service_->GetTestDevices(&devices);

  std::vector<mojom::TestDeviceInfoPtr> result;
  result.reserve(devices.size());
  for (const auto& device : devices) {
    auto device_info = mojom::TestDeviceInfo::New();
    device_info->guid = device->guid();
    device_info->name = base::UTF16ToUTF8(device->product_string());
    device_info->serial_number = base::UTF16ToUTF8(device->serial_number());
    device_info->landing_page = device->webusb_landing_page();
    result.push_back(std::move(device_info));
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace usb
}  // namespace device

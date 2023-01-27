// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_TEST_H_
#define SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_TEST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/usb_manager_test.mojom.h"
#include "services/device/usb/usb_service.h"

namespace device::usb {

class DeviceManagerTest : public mojom::UsbDeviceManagerTest {
 public:
  // |usb_service| is owned by the USB DeviceManagerImpl instance in the
  // DeviceService and once created it will keep alive until the UsbService
  // is distroyed.
  explicit DeviceManagerTest(UsbService* usb_service);

  DeviceManagerTest(const DeviceManagerTest&) = delete;
  DeviceManagerTest& operator=(const DeviceManagerTest&) = delete;

  ~DeviceManagerTest() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::UsbDeviceManagerTest> receiver);

 private:
  // mojom::DeviceManagerTest overrides:
  void AddDeviceForTesting(const std::string& name,
                           const std::string& serial_number,
                           const std::string& landing_page,
                           AddDeviceForTestingCallback callback) override;
  void RemoveDeviceForTesting(const std::string& guid,
                              RemoveDeviceForTestingCallback callback) override;
  void GetTestDevices(GetTestDevicesCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::UsbDeviceManagerTest> receivers_;
  raw_ptr<UsbService> usb_service_;
};

}  // namespace device::usb

#endif  // SERVICES_DEVICE_USB_MOJO_DEVICE_MANAGER_TEST_H_

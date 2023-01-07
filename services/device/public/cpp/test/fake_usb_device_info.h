// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_INFO_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_INFO_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "url/gurl.h"

namespace device {

// This class acts like device::UsbDevice and provides mojom::UsbDeviceInfo.
// It should be used together with FakeUsbDeviceManager just for testing.
// For convenience, a default configuration will be set if the chosen
// constructor does not explicitly set these.
class FakeUsbDeviceInfo : public base::RefCounted<FakeUsbDeviceInfo> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // This method is called when the FakeUsbDeviceManager that created this
    // object detects that the device has been disconnected from the host.
    virtual void OnDeviceRemoved(scoped_refptr<FakeUsbDeviceInfo> device);
  };

  FakeUsbDeviceInfo(uint16_t usb_version,
                    uint8_t device_class,
                    uint8_t device_subclass,
                    uint8_t device_protocol,
                    uint16_t device_version,
                    uint16_t vendor_id,
                    uint16_t product_id,
                    uint32_t bus_number,
                    uint32_t port_number,
                    const std::string& manufacturer_string,
                    const std::string& product_string,
                    const std::string& serial_number);
  FakeUsbDeviceInfo(uint16_t vendor_id,
                    uint16_t product_id,
                    const std::string& manufacturer_string,
                    const std::string& product_string,
                    const std::string& serial_number);
  FakeUsbDeviceInfo(uint16_t vendor_id,
                    uint16_t product_id,
                    const std::string& manufacturer_string,
                    const std::string& product_string,
                    const std::string& serial_number,
                    std::vector<mojom::UsbConfigurationInfoPtr> configurations);
  FakeUsbDeviceInfo(uint16_t vendor_id,
                    uint16_t product_id,
                    const std::string& manufacturer_string,
                    const std::string& product_string,
                    const std::string& serial_number,
                    const GURL& webusb_landing_page);
  FakeUsbDeviceInfo(uint16_t vendor_id, uint16_t product_id);
  FakeUsbDeviceInfo(uint16_t vendor_id,
                    uint16_t product_id,
                    uint8_t device_class,
                    std::vector<mojom::UsbConfigurationInfoPtr> configurations);

  FakeUsbDeviceInfo(const FakeUsbDeviceInfo&) = delete;
  FakeUsbDeviceInfo& operator=(const FakeUsbDeviceInfo&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyDeviceRemoved();

  std::string guid() { return device_info_.guid; }
  const mojom::UsbDeviceInfo& GetDeviceInfo() { return device_info_; }
  void AddConfig(mojom::UsbConfigurationInfoPtr config);
  bool SetActiveConfig(uint8_t value);
  void SetMockDevice(MockUsbMojoDevice* device) { mock_device_ = device; }
  MockUsbMojoDevice* mock_device() const { return mock_device_; }

  static mojom::UsbConfigurationInfoPtr CreateConfiguration(
      uint8_t device_class,
      uint8_t subclass_code,
      uint8_t protocol_code,
      uint8_t configuration_value = 1);

 protected:
  friend class base::RefCounted<FakeUsbDeviceInfo>;
  virtual ~FakeUsbDeviceInfo();

 private:
  void SetDefault();
  mojom::UsbDeviceInfo device_info_;
  base::ObserverList<Observer> observer_list_;
  raw_ptr<MockUsbMojoDevice> mock_device_ = nullptr;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_USB_DEVICE_INFO_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_WIN_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_WIN_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "services/device/usb/usb_device.h"

namespace device {

struct UsbDeviceDescriptor;

class UsbDeviceWin : public UsbDevice {
 public:
  // UsbDevice implementation:
  void Open(OpenCallback callback) override;

 protected:
  friend class UsbServiceWin;
  friend class UsbDeviceHandleWin;

  // Called by UsbServiceWin only.
  UsbDeviceWin(const std::string& device_path,
               const std::string& hub_path,
               uint32_t bus_number,
               uint32_t port_number,
               const std::string& driver_name);

  ~UsbDeviceWin() override;

  const std::string& device_path() const { return device_path_; }
  const std::string& driver_name() const { return driver_name_; }

  // Opens the device's parent hub in order to read the device, configuration
  // and string descriptors.
  void ReadDescriptors(base::OnceCallback<void(bool)> callback);

 private:
  void OnReadDescriptors(base::OnceCallback<void(bool)> callback,
                         scoped_refptr<UsbDeviceHandle> device_handle,
                         std::unique_ptr<UsbDeviceDescriptor> descriptor);
  void OnReadStringDescriptors(
      base::OnceCallback<void(bool)> callback,
      scoped_refptr<UsbDeviceHandle> device_handle,
      uint8_t i_manufacturer,
      uint8_t i_product,
      uint8_t i_serial_number,
      std::unique_ptr<std::map<uint8_t, base::string16>> string_map);
  void OnOpenedToReadWebUsbDescriptors(
      base::OnceCallback<void(bool)> callback,
      scoped_refptr<UsbDeviceHandle> device_handle);
  void OnReadWebUsbDescriptors(base::OnceCallback<void(bool)> callback,
                               scoped_refptr<UsbDeviceHandle> device_handle,
                               const GURL& landing_page);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const std::string device_path_;
  const std::string hub_path_;
  const std::string driver_name_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_WIN_H_

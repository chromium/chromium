// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_WIN_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_WIN_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/usb/usb_device.h"

namespace device {

struct UsbDeviceDescriptor;
struct WebUsbPlatformCapabilityDescriptor;

class UsbDeviceWin : public UsbDevice {
 public:
  struct FunctionInfo {
    int interface_number;
    std::wstring driver;
    std::wstring path;
  };

  enum class DriverType {
    kUnsupported,
    kWinUSB,
    kComposite,
  };

  UsbDeviceWin(const std::wstring& device_path,
               const std::wstring& hub_path,
               const base::flat_map<int, FunctionInfo>& functions,
               uint32_t bus_number,
               uint32_t port_number,
               DriverType driver_type);

  UsbDeviceWin(const UsbDeviceWin&) = delete;
  UsbDeviceWin& operator=(const UsbDeviceWin&) = delete;

  // UsbDevice implementation:
  void Open(OpenCallback callback) override;

 protected:
  friend class UsbServiceWin;
  friend class UsbDeviceHandleWin;

  ~UsbDeviceWin() override;

  const std::wstring& device_path() const { return device_path_; }
  const base::flat_map<int, FunctionInfo>& functions() const {
    return functions_;
  }
  DriverType driver_type() const { return driver_type_; }

  // Opens the device's parent hub in order to read the device, configuration
  // and string descriptors.
  void ReadDescriptors(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      base::OnceCallback<void(bool)> callback);

  void UpdateFunction(int interface_number, const FunctionInfo& function_info);

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
      std::unique_ptr<std::map<uint8_t, std::u16string>> string_map);
  void OnReadWebUsbCapabilityDescriptor(
      base::OnceCallback<void(bool)> callback,
      scoped_refptr<UsbDeviceHandle> device_handle,
      const std::optional<WebUsbPlatformCapabilityDescriptor>& descriptor);
  void OnOpenedToReadWebUsbLandingPage(
      base::OnceCallback<void(bool)> callback,
      uint8_t vendor_code,
      uint8_t landing_page_id,
      scoped_refptr<UsbDeviceHandle> device_handle);
  void OnReadWebUsbLandingPage(base::OnceCallback<void(bool)> callback,
                               scoped_refptr<UsbDeviceHandle> device_handle,
                               const GURL& landing_page);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const std::wstring device_path_;
  const std::wstring hub_path_;
  base::flat_map<int, FunctionInfo> functions_;
  const DriverType driver_type_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_WIN_H_

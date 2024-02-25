// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_win.h"

#include <windows.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_device_handle_win.h"
#include "services/device/usb/webusb_descriptors.h"

namespace device {

namespace {
const uint16_t kUsbVersion2_1 = 0x0210;
}  // namespace

UsbDeviceWin::UsbDeviceWin(const std::wstring& device_path,
                           const std::wstring& hub_path,
                           const base::flat_map<int, FunctionInfo>& functions,
                           uint32_t bus_number,
                           uint32_t port_number,
                           DriverType driver_type)
    : UsbDevice(bus_number, port_number),
      device_path_(device_path),
      hub_path_(hub_path),
      functions_(functions),
      driver_type_(driver_type) {}

UsbDeviceWin::~UsbDeviceWin() {}

void UsbDeviceWin::Open(OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<UsbDeviceHandle> device_handle;
  if (driver_type_ != DriverType::kUnsupported) {
    device_handle = new UsbDeviceHandleWin(this);
    handles().push_back(device_handle.get());
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(device_handle)));
}

void UsbDeviceWin::ReadDescriptors(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<UsbDeviceHandle> device_handle;
  base::win::ScopedHandle handle(
      CreateFile(hub_path_.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                 OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
  if (handle.IsValid()) {
    device_handle = new UsbDeviceHandleWin(this, std::move(handle),
                                           std::move(blocking_task_runner));
  } else {
    USB_PLOG(ERROR) << "Failed to open " << hub_path_;
    std::move(callback).Run(false);
    return;
  }

  ReadUsbDescriptors(device_handle,
                     base::BindOnce(&UsbDeviceWin::OnReadDescriptors, this,
                                    std::move(callback), device_handle));
}

void UsbDeviceWin::UpdateFunction(int interface_number,
                                  const FunctionInfo& function_info) {
  functions_[interface_number] = function_info;

  for (UsbDeviceHandle* handle : handles()) {
    // This is safe because only this class only adds instance of
    // UsbDeviceHandleWin to handles().
    static_cast<UsbDeviceHandleWin*>(handle)->UpdateFunction(
        interface_number, function_info.driver, function_info.path);
  }
}

void UsbDeviceWin::OnReadDescriptors(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle,
    std::unique_ptr<UsbDeviceDescriptor> descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!descriptor) {
    USB_LOG(ERROR) << "Failed to read descriptors from " << device_path_ << ".";
    device_handle->Close();
    std::move(callback).Run(false);
    return;
  }

  // Keep |guid|, |bus_number| and |port_number| before updating the
  // |device_info_|.
  descriptor->device_info->guid = device_info_->guid,
  descriptor->device_info->bus_number = device_info_->bus_number,
  descriptor->device_info->port_number = device_info_->port_number,
  device_info_ = std::move(descriptor->device_info);

  // The active configuration was set after reading the node connection info
  // from the hub driver. If it wasn't valid, assume the first configuration.
  if (!GetActiveConfiguration() && !configurations().empty())
    ActiveConfigurationChanged(configurations()[0]->configuration_value);

  auto string_map = std::make_unique<std::map<uint8_t, std::u16string>>();
  if (descriptor->i_manufacturer)
    (*string_map)[descriptor->i_manufacturer];
  if (descriptor->i_product)
    (*string_map)[descriptor->i_product];
  if (descriptor->i_serial_number)
    (*string_map)[descriptor->i_serial_number];

  ReadUsbStringDescriptors(
      device_handle, std::move(string_map),
      base::BindOnce(&UsbDeviceWin::OnReadStringDescriptors, this,
                     std::move(callback), device_handle,
                     descriptor->i_manufacturer, descriptor->i_product,
                     descriptor->i_serial_number));
}

void UsbDeviceWin::OnReadStringDescriptors(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle,
    uint8_t i_manufacturer,
    uint8_t i_product,
    uint8_t i_serial_number,
    std::unique_ptr<std::map<uint8_t, std::u16string>> string_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (i_manufacturer)
    device_info_->manufacturer_name = (*string_map)[i_manufacturer];
  if (i_product)
    device_info_->product_name = (*string_map)[i_product];
  if (i_serial_number)
    device_info_->serial_number = (*string_map)[i_serial_number];

  if (usb_version() >= kUsbVersion2_1) {
    ReadWebUsbCapabilityDescriptor(
        device_handle,
        base::BindOnce(&UsbDeviceWin::OnReadWebUsbCapabilityDescriptor, this,
                       std::move(callback), device_handle));
  } else {
    device_handle->Close();
    std::move(callback).Run(true);
  }
}

void UsbDeviceWin::OnReadWebUsbCapabilityDescriptor(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle,
    const std::optional<WebUsbPlatformCapabilityDescriptor>& descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_handle->Close();

  if (!descriptor || !descriptor->landing_page_id) {
    std::move(callback).Run(true);
    return;
  }

  Open(base::BindOnce(&UsbDeviceWin::OnOpenedToReadWebUsbLandingPage, this,
                      std::move(callback), descriptor->vendor_code,
                      descriptor->landing_page_id));
}

void UsbDeviceWin::OnOpenedToReadWebUsbLandingPage(
    base::OnceCallback<void(bool)> callback,
    uint8_t vendor_code,
    uint8_t landing_page_id,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_handle) {
    USB_LOG(ERROR) << "Failed to open device to read WebUSB descriptors.";
    // Failure to read WebUSB descriptors is not fatal.
    std::move(callback).Run(true);
    return;
  }

  ReadWebUsbLandingPage(
      vendor_code, landing_page_id, device_handle,
      base::BindOnce(&UsbDeviceWin::OnReadWebUsbLandingPage, this,
                     std::move(callback), device_handle));
}

void UsbDeviceWin::OnReadWebUsbLandingPage(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle,
    const GURL& landing_page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device_info_->webusb_landing_page = landing_page;

  device_handle->Close();
  std::move(callback).Run(true);
}

}  // namespace device

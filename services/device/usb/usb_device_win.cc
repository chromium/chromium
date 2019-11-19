// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_win.h"

#include <windows.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_device_handle_win.h"
#include "services/device/usb/webusb_descriptors.h"

namespace device {

namespace {
const uint16_t kUsbVersion2_1 = 0x0210;
}  // namespace

UsbDeviceWin::UsbDeviceWin(const std::string& device_path,
                           const std::string& hub_path,
                           uint32_t bus_number,
                           uint32_t port_number,
                           const std::string& driver_name)
    : UsbDevice(bus_number, port_number),
      device_path_(device_path),
      hub_path_(hub_path),
      driver_name_(driver_name) {}

UsbDeviceWin::~UsbDeviceWin() {}

void UsbDeviceWin::Open(OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<UsbDeviceHandle> device_handle;
  if (base::EqualsCaseInsensitiveASCII(driver_name_, "winusb"))
    device_handle = new UsbDeviceHandleWin(this, false);
  // TODO: Support composite devices.
  // else if (base::EqualsCaseInsensitiveASCII(driver_name_, "usbccgp"))
  //  device_handle = new UsbDeviceHandleWin(this, true);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), device_handle));
}

void UsbDeviceWin::ReadDescriptors(base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<UsbDeviceHandle> device_handle;
  base::win::ScopedHandle handle(
      CreateFileA(hub_path_.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                  OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
  if (handle.IsValid()) {
    device_handle = new UsbDeviceHandleWin(this, std::move(handle));
  } else {
    USB_PLOG(ERROR) << "Failed to open " << hub_path_;
    std::move(callback).Run(false);
    return;
  }

  ReadUsbDescriptors(device_handle,
                     base::BindOnce(&UsbDeviceWin::OnReadDescriptors, this,
                                    std::move(callback), device_handle));
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

  // Keep |bus_number| and |port_number| before updating the |device_info_|.
  descriptor->device_info->bus_number = device_info_->bus_number,
  descriptor->device_info->port_number = device_info_->port_number,
  device_info_ = std::move(descriptor->device_info);

  // WinUSB only supports the configuration 1.
  ActiveConfigurationChanged(1);

  auto string_map = std::make_unique<std::map<uint8_t, base::string16>>();
  if (descriptor->i_manufacturer)
    (*string_map)[descriptor->i_manufacturer] = base::string16();
  if (descriptor->i_product)
    (*string_map)[descriptor->i_product] = base::string16();
  if (descriptor->i_serial_number)
    (*string_map)[descriptor->i_serial_number] = base::string16();

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
    std::unique_ptr<std::map<uint8_t, base::string16>> string_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device_handle->Close();

  if (i_manufacturer)
    device_info_->manufacturer_name = (*string_map)[i_manufacturer];
  if (i_product)
    device_info_->product_name = (*string_map)[i_product];
  if (i_serial_number)
    device_info_->serial_number = (*string_map)[i_serial_number];

  if (usb_version() >= kUsbVersion2_1) {
    Open(base::BindOnce(&UsbDeviceWin::OnOpenedToReadWebUsbDescriptors, this,
                        std::move(callback)));
  } else {
    std::move(callback).Run(true);
  }
}

void UsbDeviceWin::OnOpenedToReadWebUsbDescriptors(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_handle) {
    USB_LOG(ERROR) << "Failed to open device to read WebUSB descriptors.";
    // Failure to read WebUSB descriptors is not fatal.
    std::move(callback).Run(true);
    return;
  }

  ReadWebUsbDescriptors(
      device_handle, base::BindOnce(&UsbDeviceWin::OnReadWebUsbDescriptors,
                                    this, std::move(callback), device_handle));
}

void UsbDeviceWin::OnReadWebUsbDescriptors(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle,
    const GURL& landing_page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  device_info_->webusb_landing_page = landing_page;

  device_handle->Close();
  std::move(callback).Run(true);
}

}  // namespace device

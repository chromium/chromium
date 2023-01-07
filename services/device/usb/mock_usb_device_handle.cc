// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mock_usb_device_handle.h"

#include "services/device/usb/usb_device.h"

namespace device {

MockUsbDeviceHandle::MockUsbDeviceHandle(UsbDevice* device) : device_(device) {}

scoped_refptr<UsbDevice> MockUsbDeviceHandle::GetDevice() const {
  return device_.get();
}

MockUsbDeviceHandle::~MockUsbDeviceHandle() = default;

}  // namespace device

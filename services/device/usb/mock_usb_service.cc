// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mock_usb_service.h"

#include <string>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "services/device/usb/usb_device.h"

namespace device {

MockUsbService::MockUsbService() : UsbService() {}

MockUsbService::~MockUsbService() = default;

void MockUsbService::AddDevice(scoped_refptr<UsbDevice> device) {
  devices()[device->guid()] = device;
  NotifyDeviceAdded(device);
}

void MockUsbService::RemoveDevice(scoped_refptr<UsbDevice> device) {
  devices().erase(device->guid());
  UsbService::NotifyDeviceRemoved(device);
}

}  // namespace device

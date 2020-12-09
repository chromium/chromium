// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mock_usb_service.h"

#include <string>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "services/device/usb/usb_device.h"

using testing::_;
using testing::Invoke;

namespace device {

MockUsbService::MockUsbService() {
  // The simpler Invoke(this, &UsbService::GetDevices) doesn't seem to work.
  ON_CALL(*this, GetDevices(_, _))
      .WillByDefault(Invoke([&](bool allow_restricted_devices,
                                GetDevicesCallback callback) {
        UsbService::GetDevices(allow_restricted_devices, std::move(callback));
      }));
}

MockUsbService::~MockUsbService() = default;

void MockUsbService::AddDevice(scoped_refptr<UsbDevice> device,
                               bool is_restricted_device) {
  devices()[device->guid()] = device;
  NotifyDeviceAdded(device, is_restricted_device);
}

void MockUsbService::RemoveDevice(scoped_refptr<UsbDevice> device,
                                  bool is_restricted_device) {
  devices().erase(device->guid());
  UsbService::NotifyDeviceRemoved(device, is_restricted_device);
}

}  // namespace device

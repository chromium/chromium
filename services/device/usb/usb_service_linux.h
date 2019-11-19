// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_SERVICE_LINUX_H_
#define SERVICES_DEVICE_USB_USB_SERVICE_LINUX_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "services/device/usb/usb_service.h"

namespace device {

struct UsbDeviceDescriptor;
class UsbDeviceLinux;

class UsbServiceLinux final : public UsbService {
 public:
  UsbServiceLinux();
  ~UsbServiceLinux() override;

  // device::UsbService implementation
  void GetDevices(const GetDevicesCallback& callback) override;

 private:
  using DeviceMap =
      std::unordered_map<std::string, scoped_refptr<UsbDeviceLinux>>;

  class BlockingTaskRunnerHelper;

  void OnDeviceAdded(const std::string& device_path,
                     std::unique_ptr<UsbDeviceDescriptor> descriptor);
  void DeviceReady(scoped_refptr<UsbDeviceLinux> device, bool success);
  void OnDeviceRemoved(const std::string& device_path);
  void HelperStarted();

  bool enumeration_ready() {
    return helper_started_ && first_enumeration_countdown_ == 0;
  }

  // |helper_started_| is set once OnDeviceAdded has been called for all devices
  // initially found on the system. |first_enumeration_countdown_| is then
  // decremented as DeviceReady is called for these devices.
  // |enumeration_callbacks_| holds the callbacks passed to GetDevices before
  // this process completes and the device list is ready.
  bool helper_started_ = false;
  uint32_t first_enumeration_countdown_ = 0;
  std::list<GetDevicesCallback> enumeration_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<BlockingTaskRunnerHelper, base::OnTaskRunnerDeleter> helper_;
  DeviceMap devices_by_path_;

  base::WeakPtrFactory<UsbServiceLinux> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsbServiceLinux);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_SERVICE_LINUX_H_

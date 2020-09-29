// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_SERVICE_WIN_H_
#define SERVICES_DEVICE_USB_USB_SERVICE_WIN_H_

#include "services/device/usb/usb_service.h"

#include <list>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "device/base/device_monitor_win.h"
#include "services/device/usb/usb_device_win.h"

namespace device {

class UsbServiceWin final : public DeviceMonitorWin::Observer,
                            public UsbService {
 public:
  UsbServiceWin();
  ~UsbServiceWin() override;

 private:
  class BlockingTaskRunnerHelper;

  // device::UsbService implementation
  void GetDevices(GetDevicesCallback callback) override;

  // device::DeviceMonitorWin::Observer implementation
  void OnDeviceAdded(const GUID& class_guid,
                     const std::wstring& device_path) override;
  void OnDeviceRemoved(const GUID& class_guid,
                       const std::wstring& device_path) override;

  // Methods called by BlockingThreadHelper
  void HelperStarted();
  void CreateDeviceObject(
      const std::wstring& device_path,
      const std::wstring& hub_path,
      const base::flat_map<int, UsbDeviceWin::FunctionInfo>& functions,
      uint32_t bus_number,
      uint32_t port_number,
      bool is_supported,
      const std::wstring& driver_name);
  void UpdateFunction(const std::wstring& device_path,
                      int interface_number,
                      const UsbDeviceWin::FunctionInfo& function_info);

  void DeviceReady(scoped_refptr<UsbDeviceWin> device,
                   const std::wstring& driver_name,
                   bool success);

  bool enumeration_ready() {
    return helper_started_ && first_enumeration_countdown_ == 0;
  }

  // Enumeration callbacks are queued until an enumeration completes.
  bool helper_started_ = false;
  uint32_t first_enumeration_countdown_ = 0;
  std::list<GetDevicesCallback> enumeration_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<BlockingTaskRunnerHelper, base::OnTaskRunnerDeleter> helper_;
  std::unordered_map<std::wstring, scoped_refptr<UsbDeviceWin>>
      devices_by_path_;

  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;

  base::WeakPtrFactory<UsbServiceWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsbServiceWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_SERVICE_WIN_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_DEVICE_MONITOR_WIN_H_
#define DEVICE_BASE_DEVICE_MONITOR_WIN_H_

#include <string>

#include "base/observer_list.h"
#include "base/win/windows_types.h"
#include "device/base/device_base_export.h"

namespace device {

// Use an instance of this class to observe devices being added and removed
// from the system, matched by device interface GUID.
class DEVICE_BASE_EXPORT DeviceMonitorWin {
 public:
  class DEVICE_BASE_EXPORT Observer {
   public:
    virtual void OnDeviceAdded(const GUID& class_guid,
                               const std::wstring& device_path);
    virtual void OnDeviceRemoved(const GUID& class_guid,
                                 const std::wstring& device_path);
  };

  ~DeviceMonitorWin();

  static DeviceMonitorWin* GetForDeviceInterface(const GUID& guid);
  static DeviceMonitorWin* GetForAllInterfaces();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class DeviceMonitorMessageWindow;

  DeviceMonitorWin();

  void NotifyDeviceAdded(const GUID& class_guid,
                         const std::wstring& device_path);
  void NotifyDeviceRemoved(const GUID& class_guid,
                           const std::wstring& device_path);

  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace device

#endif  // DEVICE_BASE_DEVICE_MONITOR_WIN_H_

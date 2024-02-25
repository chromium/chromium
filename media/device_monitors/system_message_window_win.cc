// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/device_monitors/system_message_window_win.h"

#include <dbt.h>
#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/system/system_monitor.h"
#include "base/win/wrapped_window_proc.h"
#include "media/audio/win/core_audio_util_win.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace media {

namespace {
// A static map from a device category guid to base::SystemMonitor::DeviceType.
struct DeviceCategoryToType {
  const GUID device_category;
  const base::SystemMonitor::DeviceType device_type;
};

const std::vector<DeviceCategoryToType>& GetDeviceCategoryToType() {
  static const base::NoDestructor<std::vector<DeviceCategoryToType>>
      device_category_to_type(
          {{KSCATEGORY_AUDIO, base::SystemMonitor::DEVTYPE_AUDIO},
           {KSCATEGORY_VIDEO, base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE}});
  return *device_category_to_type;
}

}  // namespace

// Manages the device notification handles for SystemMessageWindowWin.
class SystemMessageWindowWin::DeviceNotifications {
 public:
  DeviceNotifications() = delete;

  explicit DeviceNotifications(HWND hwnd)
      : notifications_(std::size(GetDeviceCategoryToType())) {
    Register(hwnd);
  }

  DeviceNotifications(const DeviceNotifications&) = delete;
  DeviceNotifications& operator=(const DeviceNotifications&) = delete;

  ~DeviceNotifications() { Unregister(); }

  void Register(HWND hwnd) {
    // Request to receive device notifications.  All applications receive basic
    // notifications via WM_DEVICECHANGE but in order to receive detailed device
    // arrival and removal messages, we need to register.
    DEV_BROADCAST_DEVICEINTERFACE filter = {0};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    bool core_audio_support = media::CoreAudioUtil::IsSupported();
    for (size_t i = 0; i < GetDeviceCategoryToType().size(); ++i) {
      // If CoreAudio is supported, AudioDeviceListenerWin will
      // take care of monitoring audio devices.
      if (core_audio_support &&
          KSCATEGORY_AUDIO == GetDeviceCategoryToType()[i].device_category) {
        continue;
      }

      filter.dbcc_classguid = GetDeviceCategoryToType()[i].device_category;
      DCHECK_EQ(notifications_[i], static_cast<HDEVNOTIFY>(NULL));
      notifications_[i] = RegisterDeviceNotification(
          hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
      DPLOG_IF(ERROR, !notifications_[i])
          << "RegisterDeviceNotification failed";
    }
  }

  void Unregister() {
    for (size_t i = 0; i < notifications_.size(); ++i) {
      if (notifications_[i]) {
        UnregisterDeviceNotification(notifications_[i]);
        notifications_[i] = NULL;
      }
    }
  }

 private:
  std::vector<HDEVNOTIFY> notifications_;
};

SystemMessageWindowWin::SystemMessageWindowWin() {
  // base:Unretained() is safe because the observer handles the correct cleanup
  // if either the SingletonHwnd or forwarded object is destroyed first.
  singleton_hwnd_observer_ =
      std::make_unique<gfx::SingletonHwndObserver>(base::BindRepeating(
          &SystemMessageWindowWin::WndProc, base::Unretained(this)));
  device_notifications_ = std::make_unique<DeviceNotifications>(
      gfx::SingletonHwnd::GetInstance()->hwnd());
}

SystemMessageWindowWin::~SystemMessageWindowWin() {
}

LRESULT SystemMessageWindowWin::OnDeviceChange(UINT event_type, LPARAM data) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  base::SystemMonitor::DeviceType device_type =
      base::SystemMonitor::DEVTYPE_UNKNOWN;
  switch (event_type) {
    case DBT_DEVNODES_CHANGED:
      // For this notification, we're happy with the default DEVTYPE_UNKNOWN.
      break;

    case DBT_DEVICEREMOVECOMPLETE:
    case DBT_DEVICEARRIVAL: {
      // This notification has more details about the specific device that
      // was added or removed.  See if this is a category we're interested
      // in monitoring and if so report the specific device type.  If we don't
      // find the category in our map, ignore the notification and do not
      // notify the system monitor.
      DEV_BROADCAST_DEVICEINTERFACE* device_interface =
          reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(data);
      if (device_interface->dbcc_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
        return TRUE;
      for (const auto& map_entry : GetDeviceCategoryToType()) {
        if (map_entry.device_category == device_interface->dbcc_classguid) {
          device_type = map_entry.device_type;
          break;
        }
      }

      // Devices that we do not have a DEVTYPE_ for, get detected via
      // DBT_DEVNODES_CHANGED, so we avoid sending additional notifications
      // for those here.
      if (device_type == base::SystemMonitor::DEVTYPE_UNKNOWN)
        return TRUE;
      break;
    }

    default:
      return TRUE;
  }

  monitor->ProcessDevicesChanged(device_type);

  return TRUE;
}

void SystemMessageWindowWin::WndProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam) {
  if (message == WM_DEVICECHANGE) {
    OnDeviceChange(static_cast<UINT>(wparam), lparam);
  }
}

}  // namespace media

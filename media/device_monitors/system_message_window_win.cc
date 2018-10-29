// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/device_monitors/system_message_window_win.h"

#include <dbt.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/system/system_monitor.h"
#include "base/win/wrapped_window_proc.h"
#include "media/audio/win/core_audio_util_win.h"

namespace media {

namespace {
const wchar_t kWindowClassName[] = L"Chrome_SystemMessageWindow";

// A static map from a device category guid to base::SystemMonitor::DeviceType.
struct {
  const GUID device_category;
  const base::SystemMonitor::DeviceType device_type;
} const kDeviceCategoryMap[] = {
    {KSCATEGORY_AUDIO, base::SystemMonitor::DEVTYPE_AUDIO},
    {KSCATEGORY_VIDEO, base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE},
};
}  // namespace

// Manages the device notification handles for SystemMessageWindowWin.
class SystemMessageWindowWin::DeviceNotifications {
 public:
  explicit DeviceNotifications(HWND hwnd) : notifications_() { Register(hwnd); }

  ~DeviceNotifications() { Unregister(); }

  void Register(HWND hwnd) {
    // Request to receive device notifications.  All applications receive basic
    // notifications via WM_DEVICECHANGE but in order to receive detailed device
    // arrival and removal messages, we need to register.
    DEV_BROADCAST_DEVICEINTERFACE filter = {0};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    bool core_audio_support = media::CoreAudioUtil::IsSupported();
    for (size_t i = 0; i < arraysize(kDeviceCategoryMap); ++i) {
      // If CoreAudio is supported, AudioDeviceListenerWin will
      // take care of monitoring audio devices.
      if (core_audio_support &&
          KSCATEGORY_AUDIO == kDeviceCategoryMap[i].device_category) {
        continue;
      }

      filter.dbcc_classguid = kDeviceCategoryMap[i].device_category;
      DCHECK_EQ(notifications_[i], static_cast<HDEVNOTIFY>(NULL));
      notifications_[i] = RegisterDeviceNotification(
          hwnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
      DPLOG_IF(ERROR, !notifications_[i])
          << "RegisterDeviceNotification failed";
    }
  }

  void Unregister() {
    for (size_t i = 0; i < arraysize(notifications_); ++i) {
      if (notifications_[i]) {
        UnregisterDeviceNotification(notifications_[i]);
        notifications_[i] = NULL;
      }
    }
  }

 private:
  HDEVNOTIFY notifications_[arraysize(kDeviceCategoryMap)];

  DISALLOW_IMPLICIT_CONSTRUCTORS(DeviceNotifications);
};

SystemMessageWindowWin::SystemMessageWindowWin() {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kWindowClassName,
      &base::win::WrappedWindowProc<SystemMessageWindowWin::WndProcThunk>, 0, 0,
      0, NULL, NULL, NULL, NULL, NULL, &window_class);
  instance_ = window_class.hInstance;
  ATOM clazz = RegisterClassEx(&window_class);
  DCHECK(clazz);

  window_ =
      CreateWindow(kWindowClassName, 0, 0, 0, 0, 0, 0, 0, 0, instance_, 0);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  device_notifications_.reset(new DeviceNotifications(window_));
}

SystemMessageWindowWin::~SystemMessageWindowWin() {
  if (window_) {
    DestroyWindow(window_);
    UnregisterClass(kWindowClassName, instance_);
  }
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
      for (const auto& map_entry : kDeviceCategoryMap) {
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

LRESULT CALLBACK SystemMessageWindowWin::WndProc(HWND hwnd,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  switch (message) {
    case WM_DEVICECHANGE:
      return OnDeviceChange(static_cast<UINT>(wparam), lparam);
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace media

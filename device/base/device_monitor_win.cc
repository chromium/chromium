// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/device_monitor_win.h"

// windows.h must be included before dbt.h.
#include <windows.h>

#include <dbt.h>

#include <map>
#include <memory>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/message_window.h"

namespace device {

class DeviceMonitorMessageWindow;

namespace {

const wchar_t kWindowClassName[] = L"DeviceMonitorMessageWindow";
DeviceMonitorMessageWindow* g_message_window;

// Provides basic comparability for GUIDs so that they can be used as keys to an
// STL map.
struct CompareGUID {
  bool operator()(const GUID& a, const GUID& b) const {
    return memcmp(&a, &b, sizeof a) < 0;
  }
};
}

// This singleton class manages a shared message window for all registered
// device notification observers. It vends one instance of DeviceManagerWin for
// each unique GUID it sees.
class DeviceMonitorMessageWindow {
 public:
  static DeviceMonitorMessageWindow* GetInstance() {
    if (!g_message_window) {
      g_message_window = new DeviceMonitorMessageWindow();
      if (g_message_window->Init()) {
        base::AtExitManager::RegisterTask(
            base::BindOnce(&base::DeletePointer<DeviceMonitorMessageWindow>,
                           base::Unretained(g_message_window)));
      } else {
        delete g_message_window;
        g_message_window = nullptr;
      }
    }
    return g_message_window;
  }

  DeviceMonitorMessageWindow(const DeviceMonitorMessageWindow&) = delete;
  DeviceMonitorMessageWindow& operator=(const DeviceMonitorMessageWindow&) =
      delete;

  DeviceMonitorWin* GetForDeviceInterface(const GUID& device_interface) {
    std::unique_ptr<DeviceMonitorWin>& device_monitor =
        device_monitors_[device_interface];
    if (!device_monitor) {
      device_monitor.reset(new DeviceMonitorWin());
    }
    return device_monitor.get();
  }

  DeviceMonitorWin* GetForAllInterfaces() { return &all_device_monitor_; }

 private:
  friend void base::DeletePointer<DeviceMonitorMessageWindow>(
      DeviceMonitorMessageWindow* message_window);

  DeviceMonitorMessageWindow() {}

  ~DeviceMonitorMessageWindow() {
    if (notify_handle_) {
      UnregisterDeviceNotification(notify_handle_);
    }
  }

  bool Init() {
    window_ = std::make_unique<base::win::MessageWindow>();
    if (!window_->CreateNamed(
            base::BindRepeating(&DeviceMonitorMessageWindow::HandleMessage,
                                base::Unretained(this)),
            kWindowClassName)) {
      LOG(ERROR) << "Failed to create message window: " << kWindowClassName;
      return false;
    }

    DEV_BROADCAST_DEVICEINTERFACE db = {sizeof(DEV_BROADCAST_DEVICEINTERFACE),
                                        DBT_DEVTYP_DEVICEINTERFACE};
    notify_handle_ = RegisterDeviceNotification(
        window_->hwnd(), &db,
        DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!notify_handle_) {
      PLOG(ERROR) << "Failed to register for device notifications";
      return false;
    }

    return true;
  }

  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result) {
    if (message == WM_DEVICECHANGE &&
        (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE)) {
      DEV_BROADCAST_HDR* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lparam);
      if (hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
        return false;

      DEV_BROADCAST_DEVICEINTERFACE* db =
          reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(hdr);

      DeviceMonitorWin* device_monitor = nullptr;
      const auto& map_entry = device_monitors_.find(db->dbcc_classguid);
      if (map_entry != device_monitors_.end())
        device_monitor = map_entry->second.get();

      std::wstring device_path(db->dbcc_name);
      DCHECK(base::IsStringASCII(device_path));
      device_path = base::ToLowerASCII(device_path);

      if (wparam == DBT_DEVICEARRIVAL) {
        if (device_monitor) {
          device_monitor->NotifyDeviceAdded(db->dbcc_classguid, device_path);
        }
        all_device_monitor_.NotifyDeviceAdded(db->dbcc_classguid, device_path);
      } else if (wparam == DBT_DEVICEREMOVECOMPLETE) {
        if (device_monitor) {
          device_monitor->NotifyDeviceRemoved(db->dbcc_classguid, device_path);
        }
        all_device_monitor_.NotifyDeviceRemoved(db->dbcc_classguid,
                                                device_path);
      }
      *result = NULL;
      return true;
    }
    return false;
  }

  std::map<GUID, std::unique_ptr<DeviceMonitorWin>, CompareGUID>
      device_monitors_;
  DeviceMonitorWin all_device_monitor_;
  std::unique_ptr<base::win::MessageWindow> window_;
  HDEVNOTIFY notify_handle_ = NULL;
};

void DeviceMonitorWin::Observer::OnDeviceAdded(
    const GUID& class_guid,
    const std::wstring& device_path) {}

void DeviceMonitorWin::Observer::OnDeviceRemoved(
    const GUID& class_guid,
    const std::wstring& device_path) {}

// static
DeviceMonitorWin* DeviceMonitorWin::GetForDeviceInterface(
    const GUID& device_interface) {
  DeviceMonitorMessageWindow* message_window =
      DeviceMonitorMessageWindow::GetInstance();
  if (message_window) {
    return message_window->GetForDeviceInterface(device_interface);
  }
  return nullptr;
}

// static
DeviceMonitorWin* DeviceMonitorWin::GetForAllInterfaces() {
  DeviceMonitorMessageWindow* message_window =
      DeviceMonitorMessageWindow::GetInstance();
  if (message_window) {
    return message_window->GetForAllInterfaces();
  }
  return nullptr;
}

DeviceMonitorWin::~DeviceMonitorWin() {}

void DeviceMonitorWin::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceMonitorWin::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

DeviceMonitorWin::DeviceMonitorWin() {}

void DeviceMonitorWin::NotifyDeviceAdded(const GUID& class_guid,
                                         const std::wstring& device_path) {
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(class_guid, device_path);
}

void DeviceMonitorWin::NotifyDeviceRemoved(const GUID& class_guid,
                                           const std::wstring& device_path) {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(class_guid, device_path);
}

}  // namespace device

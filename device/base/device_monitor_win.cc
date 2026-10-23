// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/device_monitor_win.h"

// windows.h must be included before dbt.h.
#include <windows.h>

#include <dbt.h>

#include <algorithm>
#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace device {

namespace {

// Provides basic comparability for GUIDs so that they can be used as keys to an
// STL map.
struct CompareGUID {
  bool operator()(const GUID& a, const GUID& b) const {
    return std::lexicographical_compare(
        base::byte_span_from_ref(a).begin(), base::byte_span_from_ref(a).end(),
        base::byte_span_from_ref(b).begin(), base::byte_span_from_ref(b).end());
  }
};
}  // namespace

// This singleton class manages device notification observers sharing the
// process-wide gfx::SingletonHwnd. It vends one instance of DeviceManagerWin
// for each unique GUID it sees.
class DeviceMonitorMessageWindow {
 public:
  // Returns nullptr if initialization fails. Initialization is retried on the
  // next call, since it fails when called from a thread without a UI message
  // pump.
  static DeviceMonitorMessageWindow* GetInstance() {
    // Never destroyed, like the gfx::SingletonHwnd it listens on, so that
    // observers that outlive the AtExitManager are not left with dangling
    // pointers to the DeviceMonitorWin instances it owns.
    static base::NoDestructor<DeviceMonitorMessageWindow> instance;
    DCHECK_CALLED_ON_VALID_SEQUENCE(instance->sequence_checker_);
    if (!instance->initialized_) {
      instance->initialized_ = instance->Init();
    }
    return instance->initialized_ ? instance.get() : nullptr;
  }

  DeviceMonitorMessageWindow(const DeviceMonitorMessageWindow&) = delete;
  DeviceMonitorMessageWindow& operator=(const DeviceMonitorMessageWindow&) =
      delete;

  DeviceMonitorWin* GetForDeviceInterface(const GUID& device_interface) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::unique_ptr<DeviceMonitorWin>& device_monitor =
        device_monitors_[device_interface];
    if (!device_monitor) {
      device_monitor.reset(new DeviceMonitorWin());
    }
    return device_monitor.get();
  }

  DeviceMonitorWin* GetForAllInterfaces() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return &all_device_monitor_;
  }

 private:
  friend class base::NoDestructor<DeviceMonitorMessageWindow>;

  DeviceMonitorMessageWindow() = default;
  ~DeviceMonitorMessageWindow() = delete;

  bool Init() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // gfx::SingletonHwnd is a process-wide singleton which only creates its
    // window if it is first instantiated on a thread running a UI message
    // pump. Bail out before touching it so that a caller on the wrong thread
    // cannot leave every other consumer of the singleton without a window.
    if (!base::CurrentUIThread::IsSet()) {
      LOG(ERROR) << "Device notifications require a UI message pump";
      return false;
    }

    HWND hwnd = gfx::SingletonHwnd::GetInstance()->hwnd();
    if (!hwnd) {
      LOG(ERROR) << "Failed to get the singleton message window";
      return false;
    }

    // base::Unretained() is safe because this object is never destroyed.
    subscription_ =
        gfx::SingletonHwnd::GetInstance()->RegisterCallback(base::BindRepeating(
            &DeviceMonitorMessageWindow::OnWndProc, base::Unretained(this)));

    DEV_BROADCAST_DEVICEINTERFACE db = {sizeof(DEV_BROADCAST_DEVICEINTERFACE),
                                        DBT_DEVTYP_DEVICEINTERFACE};
    notify_handle_ = RegisterDeviceNotification(
        hwnd, &db,
        DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
    if (!notify_handle_) {
      PLOG(ERROR) << "Failed to register for device notifications";
      subscription_ = {};
      return false;
    }

    return true;
  }

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (message != WM_DEVICECHANGE ||
        (wparam != DBT_DEVICEARRIVAL && wparam != DBT_DEVICEREMOVECOMPLETE)) {
      return;
    }

    DEV_BROADCAST_HDR* hdr = reinterpret_cast<DEV_BROADCAST_HDR*>(lparam);
    if (!hdr || hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) {
      return;
    }

    DEV_BROADCAST_DEVICEINTERFACE* db =
        reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(hdr);

    DeviceMonitorWin* device_monitor = nullptr;
    const auto& map_entry = device_monitors_.find(db->dbcc_classguid);
    if (map_entry != device_monitors_.end()) {
      device_monitor = map_entry->second.get();
    }

    std::wstring device_path(db->dbcc_name);
    DCHECK(base::IsStringASCII(device_path));
    device_path = base::ToLowerASCII(device_path);

    if (wparam == DBT_DEVICEARRIVAL) {
      if (device_monitor) {
        device_monitor->NotifyDeviceAdded(db->dbcc_classguid, device_path);
      }
      all_device_monitor_.NotifyDeviceAdded(db->dbcc_classguid, device_path);
    } else {
      if (device_monitor) {
        device_monitor->NotifyDeviceRemoved(db->dbcc_classguid, device_path);
      }
      all_device_monitor_.NotifyDeviceRemoved(db->dbcc_classguid, device_path);
    }
  }

  std::map<GUID, std::unique_ptr<DeviceMonitorWin>, CompareGUID>
      device_monitors_;
  DeviceMonitorWin all_device_monitor_;
  HDEVNOTIFY notify_handle_ = NULL;
  base::CallbackListSubscription subscription_;
  bool initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void DeviceMonitorWin::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

DeviceMonitorWin::DeviceMonitorWin() {}

void DeviceMonitorWin::NotifyDeviceAdded(const GUID& class_guid,
                                         const std::wstring& device_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnDeviceAdded(class_guid, device_path);
  }
}

void DeviceMonitorWin::NotifyDeviceRemoved(const GUID& class_guid,
                                           const std::wstring& device_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnDeviceRemoved(class_guid, device_path);
  }
}

}  // namespace device

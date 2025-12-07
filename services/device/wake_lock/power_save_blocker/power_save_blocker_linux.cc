// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/name_has_owner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/display/screen.h"
#include "ui/gfx/switches.h"

namespace device {

namespace {

enum class DBusApi {
  kGnome,                   // org.gnome.SessionManager
  kFreedesktopPower,        // org.freedesktop.PowerManagement
  kFreedesktopScreensaver,  // org.freedesktop.ScreenSaver
};

// Inhibit flags defined in the org.gnome.SessionManager interface.
// Can be OR'd together and passed as argument to the Inhibit() method
// to specify which power management features we want to suspend.
enum class GnomeApiInhibitFlags {
  kLogout = 1,
  kSwitchUser = 2,
  kSuspendSession = 4,
  kMarkSessionIdle = 8
};

struct DbusServiceInfo {
  const char* service_name;
  const char* interface_name;
  const char* object_path;
};

constexpr auto kServiceInfos = base::MakeFixedFlatMap<DBusApi, DbusServiceInfo>(
    {{DBusApi::kGnome,
      {"org.gnome.SessionManager", "org.gnome.SessionManager",
       "/org/gnome/SessionManager"}},
     {DBusApi::kFreedesktopPower,
      {"org.freedesktop.PowerManagement",
       "org.freedesktop.PowerManagement.Inhibit",
       "/org/freedesktop/PowerManagement/Inhibit"}},
     {DBusApi::kFreedesktopScreensaver,
      {"org.freedesktop.ScreenSaver", "org.freedesktop.ScreenSaver",
       "/org/freedesktop/ScreenSaver"}}});

bool ShouldPreventDisplaySleep(mojom::WakeLockType type) {
  switch (type) {
    case mojom::WakeLockType::kPreventAppSuspension:
      return false;
    case mojom::WakeLockType::kPreventDisplaySleep:
    case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
      return true;
  }
}

const char* GetUninhibitMethodName(DBusApi api) {
  switch (api) {
    case DBusApi::kGnome:
      return "Uninhibit";
    case DBusApi::kFreedesktopPower:
    case DBusApi::kFreedesktopScreensaver:
      return "UnInhibit";
  }
}

}  // namespace

class PowerSaveBlocker::Delegate {
 public:
  Delegate(mojom::WakeLockType type, const std::string& description)
      : type_(type), description_(description) {}

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  ~Delegate() = default;

  void Init() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bus_ = dbus_thread_linux::GetSharedSessionBus();
    if (ShouldBlock()) {
      ApplyBlock();
    }
    SetScreenSaverSuspended(true);
  }

  void CleanUp() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Invalidate weak pointers to cancel any pending D-Bus callbacks.
    weak_ptr_factory_.InvalidateWeakPtrs();

    if (ShouldBlock()) {
      RemoveBlock();
    }
    SetScreenSaverSuspended(false);
  }

 private:
  struct InhibitCookie {
    DBusApi api;
    uint32_t cookie;
  };

  // Returns true if ApplyBlock() / RemoveBlock() should be called.
  bool ShouldBlock() const {
    // Power saving APIs are not accessible in headless mode.
    return !base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kHeadless);
  }

  // Applies the power save block.
  void ApplyBlock() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // First, try to inhibit using the GNOME API, since it can inhibit both the
    // screensaver and power-saving with one method call. If this fails,
    // the callbacks will handle falling back to the Freedesktop APIs.
    DoInhibitCall(DBusApi::kGnome);
  }

  // Removes the power save block.
  void RemoveBlock() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(bus_);  // RemoveBlock() should only be called once.

    for (const auto& inhibit_cookie : inhibit_cookies_) {
      Uninhibit(inhibit_cookie);
    }

    inhibit_cookies_.clear();
  }

  void FallBackToFreedesktopApis() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (ShouldPreventDisplaySleep(type_)) {
      DoInhibitCall(DBusApi::kFreedesktopScreensaver);
    }
    DoInhibitCall(DBusApi::kFreedesktopPower);
  }

  // Makes the Inhibit method call after ensuring the service exists.
  void DoInhibitCall(DBusApi api) {
    const DbusServiceInfo& service_info = kServiceInfos.at(api);
    if (base::Contains(api_availability_cache_, api)) {
      OnInhibitServiceAvailable(api, api_availability_cache_[api]);
    } else {
      dbus_utils::NameHasOwner(
          bus_.get(), service_info.service_name,
          base::BindOnce(&Delegate::OnInhibitServiceHasOwner,
                         weak_ptr_factory_.GetWeakPtr(), api));
    }
  }

  void OnInhibitServiceHasOwner(DBusApi api, std::optional<bool> has_owner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const bool available = has_owner.value_or(false);
    api_availability_cache_[api] = available;
    OnInhibitServiceAvailable(api, available);
  }

  void OnInhibitServiceAvailable(DBusApi api, bool available) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!available) {
      if (api == DBusApi::kGnome) {
        // GNOME service doesn't exist, fall back to Freedesktop APIs.
        FallBackToFreedesktopApis();
      }
      // If Freedesktop services don't exist, we just ignore them.
      return;
    }

    const DbusServiceInfo& service_info = kServiceInfos.at(api);
    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        service_info.service_name, dbus::ObjectPath(service_info.object_path));

    dbus::MethodCall method_call(service_info.interface_name, "Inhibit");
    dbus::MessageWriter writer(&method_call);

    switch (api) {
      case DBusApi::kGnome:
        // The arguments of the method are:
        //     app_id:        The application identifier
        //     toplevel_xid:  The toplevel X window identifier
        //     reason:        The reason for the inhibition
        //     flags:         Flags that specify what should be inhibited
        writer.AppendString(
            base::CommandLine::ForCurrentProcess()->GetProgram().value());
        writer.AppendUint32(0);  // toplevel_xid
        writer.AppendString(description_);
        {
          uint32_t flags = 0;
          switch (type_) {
            case mojom::WakeLockType::kPreventDisplaySleep:
            case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
              flags |=
                  static_cast<uint32_t>(GnomeApiInhibitFlags::kMarkSessionIdle);
              flags |=
                  static_cast<uint32_t>(GnomeApiInhibitFlags::kSuspendSession);
              break;
            case mojom::WakeLockType::kPreventAppSuspension:
              flags |=
                  static_cast<uint32_t>(GnomeApiInhibitFlags::kSuspendSession);
              break;
          }
          writer.AppendUint32(flags);
        }
        break;
      case DBusApi::kFreedesktopPower:
      case DBusApi::kFreedesktopScreensaver:
        // The arguments of the method are:
        //     app_id:        The application identifier
        //     reason:        The reason for the inhibition
        writer.AppendString(
            base::CommandLine::ForCurrentProcess()->GetProgram().value());
        writer.AppendString(description_);
        break;
    }

    object_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&Delegate::OnInhibitResponse,
                       weak_ptr_factory_.GetWeakPtr(), api));
  }

  void OnInhibitResponse(DBusApi api, dbus::Response* response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!response) {
      LOG(ERROR) << "No response to Inhibit() request for "
                 << kServiceInfos.at(api).service_name;
      if (api == DBusApi::kGnome) {
        // Fall back if the method call to the GNOME service failed.
        FallBackToFreedesktopApis();
      }
      return;
    }

    // The method returns an inhibit_cookie, used to uniquely identify
    // this request. It should be used as an argument to Uninhibit()
    // in order to remove the request.
    dbus::MessageReader reader(response);
    uint32_t cookie;
    if (!reader.PopUint32(&cookie)) {
      LOG(ERROR) << "Invalid Inhibit() response: " << response->ToString();
      if (api == DBusApi::kGnome) {
        FallBackToFreedesktopApis();
      }
      return;
    }

    inhibit_cookies_.push_back({api, cookie});
  }

  // Makes the Uninhibit method call given an InhibitCookie saved by a prior
  // call to Inhibit().
  void Uninhibit(const InhibitCookie& inhibit_cookie) {
    const DbusServiceInfo& service_info = kServiceInfos.at(inhibit_cookie.api);

    dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(
        service_info.service_name, dbus::ObjectPath(service_info.object_path));

    dbus::MethodCall method_call(service_info.interface_name,
                                 GetUninhibitMethodName(inhibit_cookie.api));
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(inhibit_cookie.cookie);

    // We don't care about checking the result. We assume it works; we can't
    // really do anything about it anyway if it fails.
    object_proxy->CallMethod(&method_call,
                             dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                             base::BindOnce(&Delegate::OnUninhibitResponse,
                                            weak_ptr_factory_.GetWeakPtr()));
  }

  void OnUninhibitResponse(dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "No response to Uninhibit() request!";
    }
  }

  void SetScreenSaverSuspended(bool suspend) {
    if (suspend) {
      DCHECK(!screen_saver_suspender_);
      // The screen can be nullptr in tests.
      if (auto* const screen = display::Screen::Get()) {
        screen_saver_suspender_ = screen->SuspendScreenSaver();
      }
    } else {
      screen_saver_suspender_.reset();
    }
  }

  const mojom::WakeLockType type_;
  const std::string description_;

  scoped_refptr<dbus::Bus> bus_;

  std::vector<InhibitCookie> inhibit_cookies_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<display::Screen::ScreenSaverSuspender>
      screen_saver_suspender_;

  base::flat_map<DBusApi, bool> api_availability_cache_;

  base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
};

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : delegate_(ui_task_runner, type, description) {
  delegate_.AsyncCall(&PowerSaveBlocker::Delegate::Init);
}

PowerSaveBlocker::~PowerSaveBlocker() {
  delegate_.AsyncCall(&PowerSaveBlocker::Delegate::CleanUp);
}

}  // namespace device

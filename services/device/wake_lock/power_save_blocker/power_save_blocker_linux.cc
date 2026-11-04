// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "components/dbus/utils/name_has_owner.h"
#include "components/dbus/utils/variant.h"
#include "components/dbus/xdg/portal.h"
#include "components/dbus/xdg/portal_constants.h"
#include "components/dbus/xdg/request.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "services/device/public/cpp/device_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/switches.h"

namespace device {

namespace {

constexpr char kInhibitPortalInterface[] = "org.freedesktop.portal.Inhibit";

enum class DBusApi {
  kGnome,                   // org.gnome.SessionManager
  kFreedesktopPower,        // org.freedesktop.PowerManagement
  kFreedesktopScreensaver,  // org.freedesktop.ScreenSaver
};

// Inhibit flags defined in the org.gnome.SessionManager interface.
// Can be OR'd together and passed as argument to the Inhibit() method
// to specify which power management features we want to suspend.
// org.freedesktop.portal.Inhibit uses the same values.
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

DBusApi FallbackDBusApiForWakeLockType(mojom::WakeLockType type) {
  switch (type) {
    case mojom::WakeLockType::kPreventAppSuspension:
      return DBusApi::kFreedesktopPower;
    case mojom::WakeLockType::kPreventDisplaySleep:
    case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
      return DBusApi::kFreedesktopScreensaver;
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

GnomeApiInhibitFlags WakeLockTypeToGnomeInhibitFlag(mojom::WakeLockType type) {
  switch (type) {
    case mojom::WakeLockType::kPreventAppSuspension:
      return GnomeApiInhibitFlags::kSuspendSession;
    case mojom::WakeLockType::kPreventDisplaySleep:
    case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
      return GnomeApiInhibitFlags::kMarkSessionIdle;
  }
}

scoped_refptr<dbus::Bus>& BusOverride() {
  static base::NoDestructor<scoped_refptr<dbus::Bus>> bus;
  return *bus;
}

}  // namespace

// static
void PowerSaveBlocker::SetBusForTesting(scoped_refptr<dbus::Bus> bus) {
  BusOverride() = std::move(bus);
}

class PowerSaveBlocker::Delegate {
 public:
  Delegate(mojom::WakeLockType type, const std::string& description)
      : type_(type), description_(description) {}

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  ~Delegate() = default;

  void Init() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bus_ = BusOverride() ? BusOverride()
                         : dbus_thread_linux::GetSharedSessionBus();
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

    // In a Flatpak or Snap sandbox, the session bus APIs below typically need
    // extra permissions, so prefer the Inhibit portal there.
    if (base::FeatureList::IsEnabled(features::kWakeLockInhibitPortal) &&
        base::nix::GetAppSandbox() != base::nix::AppSandbox::kNone) {
      dbus_xdg::RequestXdgDesktopPortal(
          bus_.get(), kInhibitPortalInterface,
          base::BindOnce(&Delegate::OnPortalRequested,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    ApplyHostBlock();
  }

  void OnPortalRequested(uint32_t version) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (version == 0) {
      ApplyHostBlock();
      return;
    }
    InhibitWithPortal();
  }

  // Inhibits with org.freedesktop.portal.Inhibit. The inhibition lasts until
  // `portal_request_` is destroyed, which closes the Request.
  void InhibitWithPortal() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    dbus::ObjectProxy* portal_proxy =
        bus_->GetObjectProxy(dbus_xdg::kPortalServiceName,
                             dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
    dbus_xdg::Dictionary options;
    options.emplace("reason", dbus_utils::Variant::Wrap<"s">(description_));
    // The arguments of the method are:
    //     window:        Identifier for the window, or empty
    //     flags:         Flags that specify what should be inhibited
    //     options:       Vardict with optional further information
    portal_request_ = dbus_xdg::Request::CreateLongLived(
        bus_, portal_proxy, kInhibitPortalInterface, "Inhibit",
        std::move(options),
        base::BindOnce(&Delegate::OnPortalInhibitResult,
                       weak_ptr_factory_.GetWeakPtr()),
        std::string(),
        static_cast<uint32_t>(WakeLockTypeToGnomeInhibitFlag(type_)));
  }

  // Runs if the portal reports success or failure. Some portals report
  // neither, and the inhibition is active anyway.
  void OnPortalInhibitResult(dbus_xdg::Results results) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (results.has_value()) {
      return;
    }
    LOG(ERROR) << "Inhibit() request for " << kInhibitPortalInterface
               << " failed";
    portal_request_.reset();
    // Fall back to the session bus APIs, which may still be reachable.
    ApplyHostBlock();
  }

  // Applies the power save block without the portal.
  void ApplyHostBlock() {
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

    portal_request_.reset();
    for (const auto& inhibit_cookie : inhibit_cookies_) {
      Uninhibit(inhibit_cookie);
    }

    inhibit_cookies_.clear();
  }

  void FallBackToFreedesktopApis() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DoInhibitCall(FallbackDBusApiForWakeLockType(type_));
  }

  // Makes the Inhibit method call after ensuring the service exists.
  void DoInhibitCall(DBusApi api) {
    const DbusServiceInfo& service_info = kServiceInfos.at(api);
    if (api_availability_cache_.contains(api)) {
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
        writer.AppendUint32(
            static_cast<uint32_t>(WakeLockTypeToGnomeInhibitFlag(type_)));
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
        base::BindOnce(&Delegate::OnInhibitResponseThunk, bus_,
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

  // Static thunk to handle the response even if the Delegate is dead.
  static void OnInhibitResponseThunk(scoped_refptr<dbus::Bus> bus,
                                     base::WeakPtr<Delegate> delegate,
                                     DBusApi api,
                                     dbus::Response* response) {
    if (delegate) {
      delegate->OnInhibitResponse(api, response);
    } else if (response) {
      // The Delegate is gone, but we might have acquired a lock.
      // Parse the cookie and release it immediately to prevent leaks.
      dbus::MessageReader reader(response);
      uint32_t cookie;
      if (reader.PopUint32(&cookie)) {
        Uninhibit(bus, api, cookie);
      }
    }
  }

  // Makes the Uninhibit method call given an InhibitCookie saved by a prior
  // call to Inhibit().
  void Uninhibit(const InhibitCookie& inhibit_cookie) {
    Uninhibit(bus_, inhibit_cookie.api, inhibit_cookie.cookie);
  }

  // Static version of Uninhibit that can be called without a Delegate instance.
  static void Uninhibit(scoped_refptr<dbus::Bus> bus,
                        DBusApi api,
                        uint32_t cookie) {
    const DbusServiceInfo& service_info = kServiceInfos.at(api);

    dbus::ObjectProxy* object_proxy = bus->GetObjectProxy(
        service_info.service_name, dbus::ObjectPath(service_info.object_path));

    dbus::MethodCall method_call(service_info.interface_name,
                                 GetUninhibitMethodName(api));
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(cookie);

    // We don't care about checking the result. We assume it works; we can't
    // really do anything about it anyway if it fails.
    object_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce([](dbus::Response* response) {
          if (!response) {
            LOG(ERROR) << "No response to Uninhibit() request!";
          }
        }));
  }

  void SetScreenSaverSuspended(bool suspend) {
    // Only display wake locks should suspend the screensaver.
    // kPreventAppSuspension (eg. for audio-only playback) must still allow the
    // display to dim and sleep. On Wayland, suspending the screensaver creates
    // a zwp_idle_inhibitor_v1, which prevents the display from sleeping.
    if (type_ == mojom::WakeLockType::kPreventAppSuspension) {
      return;
    }
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

  // The portal's Inhibit() request. Destroying it releases the inhibition.
  std::unique_ptr<dbus_xdg::Request> portal_request_;

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

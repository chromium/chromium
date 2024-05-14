// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "ui/display/screen.h"
#include "ui/gfx/switches.h"

namespace device {

namespace {

enum DBusAPI {
  GNOME_API,                    // org.gnome.SessionManager
  FREEDESKTOP_POWER_API,        // org.freedesktop.PowerManagement
  FREEDESKTOP_SCREENSAVER_API,  // org.freedesktop.ScreenSaver
};

// Inhibit flags defined in the org.gnome.SessionManager interface.
// Can be OR'd together and passed as argument to the Inhibit() method
// to specify which power management features we want to suspend.
enum GnomeAPIInhibitFlags {
  INHIBIT_LOGOUT = 1,
  INHIBIT_SWITCH_USER = 2,
  INHIBIT_SUSPEND_SESSION = 4,
  INHIBIT_MARK_SESSION_IDLE = 8
};

const char kGnomeAPIServiceName[] = "org.gnome.SessionManager";
const char kGnomeAPIInterfaceName[] = "org.gnome.SessionManager";
const char kGnomeAPIObjectPath[] = "/org/gnome/SessionManager";

const char kFreeDesktopAPIPowerServiceName[] =
    "org.freedesktop.PowerManagement";
const char kFreeDesktopAPIPowerInterfaceName[] =
    "org.freedesktop.PowerManagement.Inhibit";
const char kFreeDesktopAPIPowerObjectPath[] =
    "/org/freedesktop/PowerManagement/Inhibit";

const char kFreeDesktopAPIScreenServiceName[] = "org.freedesktop.ScreenSaver";
const char kFreeDesktopAPIScreenInterfaceName[] = "org.freedesktop.ScreenSaver";
const char kFreeDesktopAPIScreenObjectPath[] = "/org/freedesktop/ScreenSaver";

const char kDbusMethodNameHasOwnerMethod[] = "NameHasOwner";

bool ServiceNameHasOwner(dbus::Bus* bus, const char* service_name) {
  dbus::ObjectProxy* dbus_proxy =
      bus->GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
  dbus::MethodCall name_has_owner_call(DBUS_INTERFACE_DBUS,
                                       kDbusMethodNameHasOwnerMethod);
  dbus::MessageWriter writer(&name_has_owner_call);
  writer.AppendString(service_name);
  std::unique_ptr<dbus::Response> name_has_owner_response =
      dbus_proxy
          ->CallMethodAndBlock(&name_has_owner_call,
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);
  dbus::MessageReader reader(name_has_owner_response.get());
  bool owned = false;
  return name_has_owner_response && reader.PopBool(&owned) && owned;
}

bool ShouldPreventDisplaySleep(mojom::WakeLockType type) {
  switch (type) {
    case mojom::WakeLockType::kPreventAppSuspension:
      return false;
    case mojom::WakeLockType::kPreventDisplaySleep:
    case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

const char* GetUninhibitMethodName(DBusAPI api) {
  switch (api) {
    case GNOME_API:
      return "Uninhibit";
    case FREEDESKTOP_POWER_API:
    case FREEDESKTOP_SCREENSAVER_API:
      return "UnInhibit";
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void GetDbusStringsForApi(DBusAPI api,
                          const char** service_name,
                          const char** interface_name,
                          const char** object_path) {
  switch (api) {
    case GNOME_API:
      *service_name = kGnomeAPIServiceName;
      *interface_name = kGnomeAPIInterfaceName;
      *object_path = kGnomeAPIObjectPath;
      return;
    case FREEDESKTOP_POWER_API:
      *service_name = kFreeDesktopAPIPowerServiceName;
      *interface_name = kFreeDesktopAPIPowerInterfaceName;
      *object_path = kFreeDesktopAPIPowerObjectPath;
      return;
    case FREEDESKTOP_SCREENSAVER_API:
      *service_name = kFreeDesktopAPIScreenServiceName;
      *interface_name = kFreeDesktopAPIScreenInterfaceName;
      *object_path = kFreeDesktopAPIScreenObjectPath;
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  // Picks an appropriate D-Bus API to use based on the desktop environment.
  Delegate(mojom::WakeLockType type,
           const std::string& description,
           scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
           scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner);

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  // Post a task to initialize the delegate on the UI thread, which will itself
  // then post a task to apply the power save block on the blocking task runner.
  void Init();

  // Post a task to remove the power save block on the blocking task runner,
  // unless it hasn't yet been applied, in which case we just prevent it from
  // applying.
  void CleanUp();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;

  struct InhibitCookie {
    DBusAPI api;
    uint32_t cookie;
  };

  ~Delegate() = default;

  // Returns true if ApplyBlock() / RemoveBlock() should be called.
  bool ShouldBlock() const;

  // Apply or remove the power save block, respectively. These methods should be
  // called once each, on the same thread, per instance. They block waiting for
  // the action to complete (with a timeout); the thread must thus allow I/O.
  void ApplyBlock();
  void RemoveBlock();

  // Makes the Inhibit method call.  Returns true and saves an entry to
  // |inhibit_cookies_| on success.
  bool Inhibit(DBusAPI api);

  // Makes the Uninhibit method call given an InhibitCookie saved by a prior
  // call to Inhibit().
  void Uninhibit(const InhibitCookie& inhibit_cookie);

  void SetScreenSaverSuspended(bool suspend);

  const mojom::WakeLockType type_;
  const std::string description_;

  scoped_refptr<dbus::Bus> bus_;

  std::vector<InhibitCookie> inhibit_cookies_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner_;

  std::unique_ptr<display::Screen::ScreenSaverSuspender>
      screen_saver_suspender_;
};

PowerSaveBlocker::Delegate::Delegate(
    mojom::WakeLockType type,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : type_(type),
      description_(description),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  // We're on the client's thread here, so we don't allocate the dbus::Bus
  // object yet. We'll do it later in ApplyBlock(), on the blocking task runner.
}

void PowerSaveBlocker::Delegate::Init() {
  if (ShouldBlock()) {
    blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Delegate::ApplyBlock, this));
  }

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Delegate::SetScreenSaverSuspended, this, true));
}

void PowerSaveBlocker::Delegate::CleanUp() {
  if (ShouldBlock()) {
    blocking_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Delegate::RemoveBlock, this));
  }

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Delegate::SetScreenSaverSuspended, this, false));
}

bool PowerSaveBlocker::Delegate::ShouldBlock() const {
  // Power saving APIs are not accessible in headless mode.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kHeadless);
}

void PowerSaveBlocker::Delegate::ApplyBlock() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!bus_);  // ApplyBlock() should only be called once.

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  bus_ = base::MakeRefCounted<dbus::Bus>(options);

  // First try to inhibit using the GNOME API, since we can inhibit both the
  // screensaver and power with one method call.
  if (!Inhibit(GNOME_API)) {
    // Couldn't inhibit using GNOME, so try the Freedesktop ScreenSaver API
    // next, if necessary.
    if (ShouldPreventDisplaySleep(type_))
      Inhibit(FREEDESKTOP_SCREENSAVER_API);
    // For every WakeLockType, we want to inhibit suspend/sleep/etc.
    Inhibit(FREEDESKTOP_POWER_API);
  }
}

void PowerSaveBlocker::Delegate::RemoveBlock() {
  DCHECK(blocking_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(bus_);  // RemoveBlock() should only be called once.

  for (const auto& inhibit_cookie : inhibit_cookies_)
    Uninhibit(inhibit_cookie);

  inhibit_cookies_.clear();
  bus_->ShutdownAndBlock();
  bus_.reset();
}

bool PowerSaveBlocker::Delegate::Inhibit(DBusAPI api) {
  const char* service_name;
  const char* interface_name;
  const char* object_path;
  GetDbusStringsForApi(api, &service_name, &interface_name, &object_path);

  // Check that the service name has an owner before making any calls,
  // Otherwise if the service does not exist, we will block for the default DBus
  // timeout, which can be large on some systems.
  if (!ServiceNameHasOwner(bus_.get(), service_name))
    return false;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name, dbus::ObjectPath(object_path));
  auto method_call =
      std::make_unique<dbus::MethodCall>(interface_name, "Inhibit");
  auto message_writer =
      std::make_unique<dbus::MessageWriter>(method_call.get());

  switch (api) {
    case GNOME_API:
      // The arguments of the method are:
      //     app_id:        The application identifier
      //     toplevel_xid:  The toplevel X window identifier
      //     reason:        The reason for the inhibit
      //     flags:         Flags that spefify what should be inhibited
      message_writer->AppendString(
          base::CommandLine::ForCurrentProcess()->GetProgram().value());
      message_writer->AppendUint32(0);  // should be toplevel_xid
      message_writer->AppendString(description_);
      {
        uint32_t flags = 0;
        switch (type_) {
          case mojom::WakeLockType::kPreventDisplaySleep:
          case mojom::WakeLockType::kPreventDisplaySleepAllowDimming:
            flags |= INHIBIT_MARK_SESSION_IDLE;
            flags |= INHIBIT_SUSPEND_SESSION;
            break;
          case mojom::WakeLockType::kPreventAppSuspension:
            flags |= INHIBIT_SUSPEND_SESSION;
            break;
        }
        message_writer->AppendUint32(flags);
      }
      break;
    case FREEDESKTOP_POWER_API:
    case FREEDESKTOP_SCREENSAVER_API:
      // The arguments of the method are:
      //     app_id:        The application identifier
      //     reason:        The reason for the inhibit
      message_writer->AppendString(
          base::CommandLine::ForCurrentProcess()->GetProgram().value());
      message_writer->AppendString(description_);
      break;
  }

  std::unique_ptr<dbus::Response> response =
      object_proxy
          ->CallMethodAndBlock(method_call.get(),
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);

  uint32_t cookie;
  if (response) {
    // The method returns an inhibit_cookie, used to uniquely identify
    // this request. It should be used as an argument to Uninhibit()
    // in order to remove the request.
    dbus::MessageReader message_reader(response.get());
    if (!message_reader.PopUint32(&cookie)) {
      LOG(ERROR) << "Invalid Inhibit() response: " << response->ToString();
      return false;
    }
  } else {
    LOG(ERROR) << "No response to Inhibit() request!";
    return false;
  }

  inhibit_cookies_.push_back({api, cookie});
  return true;
}

void PowerSaveBlocker::Delegate::Uninhibit(
    const InhibitCookie& inhibit_cookie) {
  const char* service_name;
  const char* interface_name;
  const char* object_path;
  GetDbusStringsForApi(inhibit_cookie.api, &service_name, &interface_name,
                       &object_path);

  DCHECK(ServiceNameHasOwner(bus_.get(), service_name));

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name, dbus::ObjectPath(object_path));
  auto method_call = std::make_unique<dbus::MethodCall>(
      interface_name, GetUninhibitMethodName(inhibit_cookie.api));
  auto message_writer =
      std::make_unique<dbus::MessageWriter>(method_call.get());
  message_writer->AppendUint32(inhibit_cookie.cookie);
  std::unique_ptr<dbus::Response> response =
      object_proxy
          ->CallMethodAndBlock(method_call.get(),
                               dbus::ObjectProxy::TIMEOUT_USE_DEFAULT)
          .value_or(nullptr);

  // We don't care about checking the result. We assume it works; we can't
  // really do anything about it anyway if it fails.
  if (!response)
    LOG(ERROR) << "No response to Uninhibit() request!";
}

void PowerSaveBlocker::Delegate::SetScreenSaverSuspended(bool suspend) {
  if (suspend) {
    DCHECK(!screen_saver_suspender_);
    // The screen can be nullptr in tests.
    if (auto* const screen = display::Screen::GetScreen())
      screen_saver_suspender_ = screen->SuspendScreenSaver();
  } else {
    screen_saver_suspender_.reset();
  }
}

PowerSaveBlocker::PowerSaveBlocker(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(base::MakeRefCounted<Delegate>(type,
                                               description,
                                               ui_task_runner,
                                               blocking_task_runner)),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  delegate_->Init();
}

PowerSaveBlocker::~PowerSaveBlocker() {
  delegate_->CleanUp();
}

}  // namespace device

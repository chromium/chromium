// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/battery/battery_status_manager_linux.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "base/version.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "dbus/values_util.h"
#include "services/device/battery/battery_status_manager_linux-inl.h"

namespace device {
namespace {
const char kBatteryNotifierThreadName[] = "BatteryStatusNotifier";

class UPowerProperties : public dbus::PropertySet {
 public:
  UPowerProperties(dbus::ObjectProxy* object_proxy,
                   const PropertyChangedCallback callback);
  ~UPowerProperties() override;

  base::Version daemon_version();

 private:
  dbus::Property<std::string> daemon_version_;

  DISALLOW_COPY_AND_ASSIGN(UPowerProperties);
};

UPowerProperties::UPowerProperties(dbus::ObjectProxy* object_proxy,
                                   const PropertyChangedCallback callback)
    : dbus::PropertySet(object_proxy, kUPowerInterfaceName, callback) {
  RegisterProperty(kUPowerPropertyDaemonVersion, &daemon_version_);
}

UPowerProperties::~UPowerProperties() {}

base::Version UPowerProperties::daemon_version() {
  return (daemon_version_.is_valid() || daemon_version_.GetAndBlock())
             ? base::Version(daemon_version_.value())
             : base::Version();
}

class UPowerObject {
 public:
  using PropertyChangedCallback = dbus::PropertySet::PropertyChangedCallback;

  UPowerObject(dbus::Bus* dbus,
               const PropertyChangedCallback property_changed_callback);
  ~UPowerObject();

  std::vector<dbus::ObjectPath> EnumerateDevices();
  dbus::ObjectPath GetDisplayDevice();

  dbus::ObjectProxy* proxy() { return proxy_; }
  UPowerProperties* properties() { return properties_.get(); }

 private:
  dbus::Bus* dbus_;           // Owned by the BatteryStatusNotificationThread.
  dbus::ObjectProxy* proxy_;  // Owned by the dbus.
  std::unique_ptr<UPowerProperties> properties_;

  DISALLOW_COPY_AND_ASSIGN(UPowerObject);
};

UPowerObject::UPowerObject(
    dbus::Bus* dbus,
    const PropertyChangedCallback property_changed_callback)
    : dbus_(dbus),
      proxy_(dbus_->GetObjectProxy(kUPowerServiceName,
                                   dbus::ObjectPath(kUPowerPath))),
      properties_(
          std::make_unique<UPowerProperties>(proxy_,
                                             property_changed_callback)) {}

UPowerObject::~UPowerObject() {
  properties_.reset();  // before the proxy is deleted.
  dbus_->RemoveObjectProxy(kUPowerServiceName, proxy_->object_path(),
                           base::DoNothing());
}

std::vector<dbus::ObjectPath> UPowerObject::EnumerateDevices() {
  std::vector<dbus::ObjectPath> paths;
  dbus::MethodCall method_call(kUPowerServiceName,
                               kUPowerMethodEnumerateDevices);
  std::unique_ptr<dbus::Response> response(proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));

  if (response) {
    dbus::MessageReader reader(response.get());
    reader.PopArrayOfObjectPaths(&paths);
  }
  return paths;
}

dbus::ObjectPath UPowerObject::GetDisplayDevice() {
  dbus::ObjectPath display_device_path;
  if (!proxy_)
    return display_device_path;

  dbus::MethodCall method_call(kUPowerServiceName,
                               kUPowerMethodGetDisplayDevice);
  std::unique_ptr<dbus::Response> response(proxy_->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));

  if (response) {
    dbus::MessageReader reader(response.get());
    reader.PopObjectPath(&display_device_path);
  }
  return display_device_path;
}

void UpdateNumberBatteriesHistogram(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("BatteryStatus.NumberBatteriesLinux", count, 1, 5,
                              6);
}

class BatteryProperties : public dbus::PropertySet {
 public:
  BatteryProperties(dbus::ObjectProxy* object_proxy,
                    const PropertyChangedCallback callback);
  ~BatteryProperties() override;

  void ConnectSignals() override;

  void Invalidate();

  bool is_present(bool default_value = false);
  double percentage(double default_value = 100);
  uint32_t state(uint32_t default_value = UPOWER_DEVICE_STATE_UNKNOWN);
  int64_t time_to_empty(int64_t default_value = 0);
  int64_t time_to_full(int64_t default_value = 0);
  uint32_t type(uint32_t default_value = UPOWER_DEVICE_TYPE_UNKNOWN);

 private:
  bool connected_ = false;
  dbus::Property<bool> is_present_;
  dbus::Property<double> percentage_;
  dbus::Property<uint32_t> state_;
  dbus::Property<int64_t> time_to_empty_;
  dbus::Property<int64_t> time_to_full_;
  dbus::Property<uint32_t> type_;

  DISALLOW_COPY_AND_ASSIGN(BatteryProperties);
};

BatteryProperties::BatteryProperties(dbus::ObjectProxy* object_proxy,
                                     const PropertyChangedCallback callback)
    : dbus::PropertySet(object_proxy, kUPowerDeviceInterfaceName, callback) {
  RegisterProperty(kUPowerDevicePropertyIsPresent, &is_present_);
  RegisterProperty(kUPowerDevicePropertyPercentage, &percentage_);
  RegisterProperty(kUPowerDevicePropertyState, &state_);
  RegisterProperty(kUPowerDevicePropertyTimeToEmpty, &time_to_empty_);
  RegisterProperty(kUPowerDevicePropertyTimeToFull, &time_to_full_);
  RegisterProperty(kUPowerDevicePropertyType, &type_);
}

BatteryProperties::~BatteryProperties() {}

void BatteryProperties::ConnectSignals() {
  if (!connected_) {
    connected_ = true;
    dbus::PropertySet::ConnectSignals();
  }
}

void BatteryProperties::Invalidate() {
  is_present_.set_valid(false);
  percentage_.set_valid(false);
  state_.set_valid(false);
  time_to_empty_.set_valid(false);
  time_to_full_.set_valid(false);
  type_.set_valid(false);
}

bool BatteryProperties::is_present(bool default_value) {
  return (is_present_.is_valid() || is_present_.GetAndBlock())
             ? is_present_.value()
             : default_value;
}

double BatteryProperties::percentage(double default_value) {
  return (percentage_.is_valid() || percentage_.GetAndBlock())
             ? percentage_.value()
             : default_value;
}

uint32_t BatteryProperties::state(uint32_t default_value) {
  return (state_.is_valid() || state_.GetAndBlock()) ? state_.value()
                                                     : default_value;
}

int64_t BatteryProperties::time_to_empty(int64_t default_value) {
  return (time_to_empty_.is_valid() || time_to_empty_.GetAndBlock())
             ? time_to_empty_.value()
             : default_value;
}

int64_t BatteryProperties::time_to_full(int64_t default_value) {
  return (time_to_full_.is_valid() || time_to_full_.GetAndBlock())
             ? time_to_full_.value()
             : default_value;
}

uint32_t BatteryProperties::type(uint32_t default_value) {
  return (type_.is_valid() || type_.GetAndBlock()) ? type_.value()
                                                   : default_value;
}

class BatteryObject {
 public:
  using PropertyChangedCallback = dbus::PropertySet::PropertyChangedCallback;

  BatteryObject(dbus::Bus* dbus,
                const dbus::ObjectPath& device_path,
                const PropertyChangedCallback& property_changed_callback);
  ~BatteryObject();

  bool IsValid() const;

  dbus::ObjectProxy* proxy() { return proxy_; }
  BatteryProperties* properties() { return properties_.get(); }

 private:
  dbus::Bus* dbus_;           // Owned by the BatteryStatusNotificationThread,
  dbus::ObjectProxy* proxy_;  // Owned by the dbus.
  std::unique_ptr<BatteryProperties> properties_;

  DISALLOW_COPY_AND_ASSIGN(BatteryObject);
};

BatteryObject::BatteryObject(
    dbus::Bus* dbus,
    const dbus::ObjectPath& device_path,
    const PropertyChangedCallback& property_changed_callback)
    : dbus_(dbus),
      proxy_(dbus_->GetObjectProxy(kUPowerServiceName, device_path)),
      properties_(
          std::make_unique<BatteryProperties>(proxy_,
                                              property_changed_callback)) {}

BatteryObject::~BatteryObject() {
  properties_.reset();  // before the proxy is deleted.
  dbus_->RemoveObjectProxy(kUPowerServiceName, proxy_->object_path(),
                           base::DoNothing());
}

bool BatteryObject::IsValid() const {
  return properties_->is_present() &&
         properties_->type() == UPOWER_DEVICE_TYPE_BATTERY;
}

mojom::BatteryStatus ComputeWebBatteryStatus(BatteryProperties* properties) {
  mojom::BatteryStatus status;
  uint32_t state = properties->state();
  status.charging = state != UPOWER_DEVICE_STATE_DISCHARGING &&
                    state != UPOWER_DEVICE_STATE_EMPTY;
  // Convert percentage to a value between 0 and 1 with 2 digits of precision.
  // This is to bring it in line with other platforms like Mac and Android where
  // we report level with 1% granularity. It also serves the purpose of reducing
  // the possibility of fingerprinting and triggers less level change events on
  // the blink side.
  // TODO(timvolodine): consider moving this rounding to the blink side.
  status.level = round(properties->percentage()) / 100.f;

  switch (state) {
    case UPOWER_DEVICE_STATE_CHARGING: {
      int64_t time_to_full = properties->time_to_full();
      status.charging_time = (time_to_full > 0)
                                 ? time_to_full
                                 : std::numeric_limits<double>::infinity();
      break;
    }
    case UPOWER_DEVICE_STATE_DISCHARGING: {
      int64_t time_to_empty = properties->time_to_empty();
      // Set dischargingTime if it's available. Otherwise leave the default
      // value which is +infinity.
      if (time_to_empty > 0)
        status.discharging_time = time_to_empty;
      status.charging_time = std::numeric_limits<double>::infinity();
      break;
    }
    case UPOWER_DEVICE_STATE_FULL: {
      break;
    }
    default: { status.charging_time = std::numeric_limits<double>::infinity(); }
  }
  return status;
}

}  // namespace

// Class that represents a dedicated thread which communicates with DBus to
// obtain battery information and receives battery change notifications.
class BatteryStatusManagerLinux::BatteryStatusNotificationThread
    : public base::Thread {
 public:
  explicit BatteryStatusNotificationThread(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : base::Thread(kBatteryNotifierThreadName), callback_(callback) {}

  ~BatteryStatusNotificationThread() override {
    // Make sure to shutdown the dbus connection if it is still open in the very
    // end. It needs to happen on the BatteryStatusNotificationThread.
    task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BatteryStatusNotificationThread::ShutdownDBusConnection,
                       base::Unretained(this)));

    // Drain the message queue of the BatteryStatusNotificationThread and stop.
    Stop();
  }

  void StartListening() {
    DCHECK(OnWatcherThread());

    if (upower_)
      return;

    if (!system_bus_)
      InitDBus();

    upower_ = std::make_unique<UPowerObject>(
        system_bus_.get(), UPowerObject::PropertyChangedCallback());
    upower_->proxy()->ConnectToSignal(
        kUPowerServiceName, kUPowerSignalDeviceAdded,
        base::Bind(&BatteryStatusNotificationThread::DeviceAdded,
                   base::Unretained(this)),
        base::DoNothing());
    upower_->proxy()->ConnectToSignal(
        kUPowerServiceName, kUPowerSignalDeviceRemoved,
        base::Bind(&BatteryStatusNotificationThread::DeviceRemoved,
                   base::Unretained(this)),
        base::DoNothing());

    FindBatteryDevice();
  }

  void StopListening() {
    DCHECK(OnWatcherThread());
    ShutdownDBusConnection();
  }

  void SetDBusForTesting(dbus::Bus* bus) { system_bus_ = bus; }

 private:
  bool OnWatcherThread() const {
    return task_runner()->BelongsToCurrentThread();
  }

  void InitDBus() {
    DCHECK(OnWatcherThread());

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    options.connection_type = dbus::Bus::PRIVATE;
    system_bus_ = base::MakeRefCounted<dbus::Bus>(options);
  }

  bool IsDaemonVersionBelow_0_99() {
    DCHECK(OnWatcherThread());

    base::Version daemon_version = upower_->properties()->daemon_version();
    return daemon_version.IsValid() &&
           daemon_version.CompareTo(base::Version("0.99")) < 0;
  }

  void FindBatteryDevice() {
    DCHECK(OnWatcherThread());

    // If ShutdownDBusConnection() has been called, the DBus connection will be
    // destroyed eventually. In the meanwhile, pending tasks can still end up
    // calling this class. Ignore these calls.
    if (!system_bus_)
      return;

    // Move the currently watched battery_ device to a stack-local variable such
    // that we can enumerate all devices (once more):
    // first testing the display device, then testing all devices from
    // EnumerateDevices. We will monitor the first battery device we find.
    // - That may be the same device we did monitor on entering this method;
    //   then we'll use the same BatteryObject instance, that was moved to
    //   current - see UseCurrentOrCreateBattery().
    // - Or it may be a new device; then the previously monitored BatteryObject
    //   instance (if any) is released on leaving this function.
    // - Or we may not find a battery device; then on leaving this function
    //   battery_ will be nullptr and the previously monitored BatteryObject
    //   instance (if any) is no longer a battery and will be released.
    std::unique_ptr<BatteryObject> current = std::move(battery_);
    auto UseCurrentOrCreateBattery =
        [&current, this](const dbus::ObjectPath& device_path) {
          if (current && current->proxy()->object_path() == device_path)
            return std::move(current);
          return CreateBattery(device_path);
        };

    dbus::ObjectPath display_device_path;
    if (!IsDaemonVersionBelow_0_99())
      display_device_path = upower_->GetDisplayDevice();
    if (display_device_path.IsValid()) {
      auto battery = UseCurrentOrCreateBattery(display_device_path);
      if (battery->IsValid())
        battery_ = std::move(battery);
    }

    if (!battery_) {
      int num_batteries = 0;
      for (const auto& device_path : upower_->EnumerateDevices()) {
        auto battery = UseCurrentOrCreateBattery(device_path);
        if (!battery->IsValid())
          continue;

        if (battery_) {
          // TODO(timvolodine): add support for multiple batteries. Currently we
          // only collect information from the first battery we encounter
          // (crbug.com/400780).
          LOG(WARNING) << "multiple batteries found, "
                       << "using status data of the first battery only.";
        } else {
          battery_ = std::move(battery);
        }
        num_batteries++;
      }

      UpdateNumberBatteriesHistogram(num_batteries);
    }

    if (!battery_) {
      callback_.Run(mojom::BatteryStatus());
      return;
    }

    battery_->properties()->ConnectSignals();
    NotifyBatteryStatus();

    if (IsDaemonVersionBelow_0_99()) {
      // UPower Version 0.99 replaced the Changed signal with the
      // PropertyChanged signal. For older versions we need to listen
      // to the Changed signal.
      battery_->proxy()->ConnectToSignal(
          kUPowerDeviceInterfaceName, kUPowerDeviceSignalChanged,
          base::Bind(&BatteryStatusNotificationThread::BatteryChanged,
                     base::Unretained(this)),
          base::DoNothing());
    }
  }

  void ShutdownDBusConnection() {
    DCHECK(OnWatcherThread());

    if (!system_bus_)
      return;

    battery_.reset();  // before the |system_bus_| is shut down.
    upower_.reset();

    // Shutdown DBus connection later because there may be pending tasks on
    // this thread.
    task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&dbus::Bus::ShutdownAndBlock, system_bus_));
    system_bus_ = nullptr;
  }

  std::unique_ptr<BatteryObject> CreateBattery(
      const dbus::ObjectPath& device_path) {
    return std::make_unique<BatteryObject>(
        system_bus_.get(), device_path,
        base::Bind(&BatteryStatusNotificationThread::BatteryPropertyChanged,
                   base::Unretained(this)));
  }

  void DeviceAdded(dbus::Signal* /* signal */) {
    DCHECK(OnWatcherThread());

    // Re-iterate all devices to see if we need to monitor the added battery
    // instead of the currently monitored battery.
    FindBatteryDevice();
  }

  void DeviceRemoved(dbus::Signal* signal) {
    DCHECK(OnWatcherThread());

    if (!battery_)
      return;

    // UPower specifies that the DeviceRemoved signal has an object-path as
    // argument, however IRL that signal was observed with a string argument,
    // so cover both cases (argument as string, as object-path and neither of
    // these) and call FindBatteryDevice() if either we couldn't get the
    // argument or the removed device-path is the battery_.
    dbus::MessageReader reader(signal);
    dbus::ObjectPath removed_device_path;
    switch (reader.GetDataType()) {
      case dbus::Message::DataType::STRING: {
        std::string removed_device_path_string;
        if (reader.PopString(&removed_device_path_string))
          removed_device_path = dbus::ObjectPath(removed_device_path_string);
        break;
      }

      case dbus::Message::DataType::OBJECT_PATH:
        reader.PopObjectPath(&removed_device_path);
        break;

      default:
        break;
    }

    if (!removed_device_path.IsValid() ||
        battery_->proxy()->object_path() == removed_device_path) {
      FindBatteryDevice();
    }
  }

  void BatteryPropertyChanged(const std::string& /* property_name */) {
    DCHECK(OnWatcherThread());
    NotifyBatteryStatus();
  }

  void BatteryChanged(dbus::Signal* /* signal */) {
    DCHECK(OnWatcherThread());
    DCHECK(battery_);
    battery_->properties()->Invalidate();
    NotifyBatteryStatus();
  }

  void NotifyBatteryStatus() {
    DCHECK(OnWatcherThread());

    if (!system_bus_ || !battery_ || notifying_battery_status_)
      return;

    // If the system uses a UPower daemon older than version 0.99
    // (see IsDaemonVersionBelow_0_99), then we are notified about changed
    // battery_ properties through the 'Changed' signal of the battery_
    // device (see BatteryChanged()). That is implemented to invalidate all
    // battery_ properties (so they are re-fetched from the dbus). Getting
    // the new property-value triggers a callback to BatteryPropertyChanged().
    // notifying_battery_status_ is set to avoid recursion and computing the
    // status too often.
    notifying_battery_status_ = true;
    callback_.Run(ComputeWebBatteryStatus(battery_->properties()));
    notifying_battery_status_ = false;
  }

  const BatteryStatusService::BatteryUpdateCallback callback_;
  scoped_refptr<dbus::Bus> system_bus_;
  std::unique_ptr<UPowerObject> upower_;
  std::unique_ptr<BatteryObject> battery_;
  bool notifying_battery_status_ = false;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusNotificationThread);
};

BatteryStatusManagerLinux::BatteryStatusManagerLinux(
    const BatteryStatusService::BatteryUpdateCallback& callback)
    : callback_(callback) {}

BatteryStatusManagerLinux::~BatteryStatusManagerLinux() {}

bool BatteryStatusManagerLinux::StartListeningBatteryChange() {
  if (!StartNotifierThreadIfNecessary())
    return false;

  notifier_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BatteryStatusNotificationThread::StartListening,
                     base::Unretained(notifier_thread_.get())));
  return true;
}

void BatteryStatusManagerLinux::StopListeningBatteryChange() {
  if (!notifier_thread_)
    return;

  notifier_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&BatteryStatusNotificationThread::StopListening,
                                base::Unretained(notifier_thread_.get())));
}

bool BatteryStatusManagerLinux::StartNotifierThreadIfNecessary() {
  if (notifier_thread_)
    return true;

  base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
  auto notifier_thread =
      std::make_unique<BatteryStatusNotificationThread>(callback_);
  if (!notifier_thread->StartWithOptions(thread_options)) {
    LOG(ERROR) << "Could not start the " << kBatteryNotifierThreadName
               << " thread";
    return false;
  }
  notifier_thread_ = std::move(notifier_thread);
  return true;
}

base::Thread* BatteryStatusManagerLinux::GetNotifierThreadForTesting() {
  return notifier_thread_.get();
}

// static
std::unique_ptr<BatteryStatusManagerLinux>
BatteryStatusManagerLinux::CreateForTesting(
    const BatteryStatusService::BatteryUpdateCallback& callback,
    dbus::Bus* bus) {
  auto manager = std::make_unique<BatteryStatusManagerLinux>(callback);
  if (!manager->StartNotifierThreadIfNecessary())
    return nullptr;
  manager->notifier_thread_->SetDBusForTesting(bus);
  return manager;
}

// static
std::unique_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return std::make_unique<BatteryStatusManagerLinux>(callback);
}

}  // namespace device

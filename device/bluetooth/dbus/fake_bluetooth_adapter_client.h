// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADAPTER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"

namespace bluez {

// FakeBluetoothAdapterClient simulates the behavior of the Bluetooth Daemon
// adapter objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAdapterClient
    : public BluetoothAdapterClient {
 public:
  struct Properties : public BluetoothAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeBluetoothAdapterClient();
  ~FakeBluetoothAdapterClient() override;
  int GetPauseCount() { return pause_count_; }
  int GetUnpauseCount() { return unpause_count_; }
  uint32_t set_long_term_keys_call_count() {
    return set_long_term_keys_call_count_;
  }

  // BluetoothAdapterClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetAdapters() override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void StartDiscovery(const dbus::ObjectPath& object_path,
                      ResponseCallback callback) override;
  void StopDiscovery(const dbus::ObjectPath& object_path,
                     ResponseCallback callback) override;
  void PauseDiscovery(const dbus::ObjectPath& object_path,
                      const base::Closure& callback,
                      ErrorCallback error_callback) override;
  void UnpauseDiscovery(const dbus::ObjectPath& object_path,
                        const base::Closure& callback,
                        ErrorCallback error_callback) override;
  void RemoveDevice(const dbus::ObjectPath& object_path,
                    const dbus::ObjectPath& device_path,
                    const base::Closure& callback,
                    ErrorCallback error_callback) override;
  void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                          const DiscoveryFilter& discovery_filter,
                          const base::Closure& callback,
                          ErrorCallback error_callback) override;
  void CreateServiceRecord(const dbus::ObjectPath& object_path,
                           const bluez::BluetoothServiceRecordBlueZ& record,
                           const ServiceRecordCallback& callback,
                           ErrorCallback error_callback) override;
  void RemoveServiceRecord(const dbus::ObjectPath& object_path,
                           uint32_t handle,
                           const base::Closure& callback,
                           ErrorCallback error_callback) override;
  void SetLongTermKeys(const dbus::ObjectPath& object_path,
                       const std::vector<std::vector<uint8_t>>& long_term_keys,
                       ErrorCallback error_callback) override;

  // Sets the current simulation timeout interval.
  void SetSimulationIntervalMs(int interval_ms);

  // Returns current discovery filter in use by this adapter.
  DiscoveryFilter* GetDiscoveryFilter();

  // Make SetDiscoveryFilter fail when called next time.
  void MakeSetDiscoveryFilterFail();

  // Make StartDiscovery fail when called next time.
  void MakeStartDiscoveryFail();

  // Mark the adapter and second adapter as visible or invisible.
  void SetVisible(bool visible);
  void SetSecondVisible(bool visible);

  // Set adapter UUIDs
  void SetUUIDs(const std::vector<std::string>& uuids);
  void SetSecondUUIDs(const std::vector<std::string>& uuids);

  // Set discoverable timeout
  void SetDiscoverableTimeout(uint32_t timeout);

  // Object path, name and addresses of the adapters we emulate.
  static const char kAdapterPath[];
  static const char kAdapterName[];
  static const char kAdapterAddress[];

  static const char kSecondAdapterPath[];
  static const char kSecondAdapterName[];
  static const char kSecondAdapterAddress[];

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const std::string& property_name);

  // Posts the delayed task represented by |callback| onto the current
  // message loop to be executed after |simulation_interval_ms_| milliseconds.
  void PostDelayedTask(base::OnceClosure callback);

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;

  // Static properties we return.
  std::unique_ptr<Properties> properties_;
  std::unique_ptr<Properties> second_properties_;

  // Whether the adapter and second adapter should be visible or not.
  bool visible_;
  bool second_visible_;

  // Number of times we've been asked to discover.
  int discovering_count_;

  // Number of times we've been asked to pause discovery.
  int pause_count_;

  // Number of times we've been asked to unpause discovery.
  int unpause_count_;

  // Current discovery filter
  std::unique_ptr<DiscoveryFilter> discovery_filter_;

  // When set, next call to SetDiscoveryFilter would fail.
  bool set_discovery_filter_should_fail_;

  // When set, next call to StartDiscovery would fail.
  bool set_start_discovery_should_fail_ = false;

  // Current timeout interval used when posting delayed tasks.
  int simulation_interval_ms_;

  // Last used handle value issued for a service record.
  uint32_t last_handle_;

  // Service records manually registered with this adapter by handle.
  std::map<uint32_t, BluetoothServiceRecordBlueZ> records_;

  uint32_t set_long_term_keys_call_count_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADAPTER_CLIENT_H_

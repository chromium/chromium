// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"

namespace bluez {

// FakeBluetoothGattCharacteristicClient simulates the behavior of the
// Bluetooth Daemon GATT characteristic objects and is used in test cases in
// place of a mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattCharacteristicClient
    : public BluetoothGattCharacteristicClient {
 public:
  struct Properties : public BluetoothGattCharacteristicClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeBluetoothGattCharacteristicClient();
  ~FakeBluetoothGattCharacteristicClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;

  // BluetoothGattCharacteristicClient overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetCharacteristics() override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void ReadValue(const dbus::ObjectPath& object_path,
                 ValueCallback callback,
                 ErrorCallback error_callback) override;
  void WriteValue(const dbus::ObjectPath& object_path,
                  const std::vector<uint8_t>& value,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void PrepareWriteValue(const dbus::ObjectPath& object_path,
                         const std::vector<uint8_t>& value,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override;
#if defined(OS_CHROMEOS)
  void StartNotify(
      const dbus::ObjectPath& object_path,
      device::BluetoothGattCharacteristic::NotificationType notification_type,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#else
  void StartNotify(const dbus::ObjectPath& object_path,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override;
#endif

  void StopNotify(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;

  // Makes the group of characteristics belonging to a particular GATT based
  // profile available under the GATT service with object path |service_path|.
  // Characteristic paths are hierarchical to service paths.
  void ExposeHeartRateCharacteristics(const dbus::ObjectPath& service_path);
  void HideHeartRateCharacteristics();

  // Returns whether or not the heart rate characteristics are visible and
  // performs the appropriate assertions.
  bool IsHeartRateVisible() const;

  // Makes this characteristic client really slow.
  // So slow, that it is guaranteed that |requests| requests will
  // come in while the client is doing the previous request.
  // Setting |requests| to zero will cause all delayed actions to
  // complete immediately.
  void SetExtraProcessing(size_t requests);

  size_t GetExtraProcessing() const;

  // Sets whether the client is authorized or not.
  // Defaults to authorized.
  void SetAuthorized(bool authorized) { authorized_ = authorized; }

  // Get the current Authorization state.
  bool IsAuthorized() const { return authorized_; }

  // Whether the client is Authenticated
  // Defaults to authenticated.
  void SetAuthenticated(bool authenticated) { authenticated_ = authenticated; }

  // Get the current Authenticated state.
  bool IsAuthenticated() const { return authenticated_; }

  // Returns the current object paths of exposed characteristics. If the
  // characteristic is not visible, returns an invalid empty path.
  dbus::ObjectPath GetHeartRateMeasurementPath() const;
  dbus::ObjectPath GetBodySensorLocationPath() const;
  dbus::ObjectPath GetHeartRateControlPointPath() const;

  // Object path components and UUIDs of GATT characteristics.
  // Heart Rate Service:
  static const char kHeartRateMeasurementPathComponent[];
  static const char kHeartRateMeasurementUUID[];
  static const char kBodySensorLocationPathComponent[];
  static const char kBodySensorLocationUUID[];
  static const char kHeartRateControlPointPathComponent[];
  static const char kHeartRateControlPointUUID[];

 private:
  // Property callback passed when we create Properties structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Notifies observers.
  void NotifyCharacteristicAdded(const dbus::ObjectPath& object_path);
  void NotifyCharacteristicRemoved(const dbus::ObjectPath& object_path);

  // Schedules a heart rate measurement value change, if the heart rate
  // characteristics are visible.
  void ScheduleHeartRateMeasurementValueChange();

  // Returns a random Heart Rate Measurement value. All the fields of the value
  // are populated according to the the fake behavior. The measurement value
  // is a random value within a reasonable range.
  std::vector<uint8_t> GetHeartRateMeasurementValue();

  // Callback that executes a delayed ReadValue action by updating the
  // appropriate "Value" property and invoking the ValueCallback.
  void DelayedReadValueCallback(const dbus::ObjectPath& object_path,
                                ValueCallback callback,
                                const std::vector<uint8_t>& value);

  // If true, characteristics of the Heart Rate Service are visible. Use
  // IsHeartRateVisible() to check the value.
  bool heart_rate_visible_;

  // If true, the client is authorized to read and write.
  bool authorized_;

  // If true, the client is authenticated.
  bool authenticated_;

  // Total calories burned, used for the Heart Rate Measurement characteristic.
  uint16_t calories_burned_;

  // Static properties returned for simulated characteristics for the Heart
  // Rate Service. These pointers are not NULL only if the characteristics are
  // actually exposed.
  std::unique_ptr<Properties> heart_rate_measurement_properties_;
  std::unique_ptr<Properties> body_sensor_location_properties_;
  std::unique_ptr<Properties> heart_rate_control_point_properties_;

  // Object paths of the exposed characteristics. If a characteristic is not
  // exposed, these will be empty.
  std::string heart_rate_measurement_path_;
  std::string heart_rate_measurement_ccc_desc_path_;
  std::string body_sensor_location_path_;
  std::string heart_rate_control_point_path_;

  // Number of extra requests that need to come in simulating slowness.
  size_t extra_requests_;

  // Current countdowns for extra requests for various actions.
  struct DelayedCallback {
   public:
    DelayedCallback(base::OnceClosure callback, size_t delay);
    ~DelayedCallback();

    base::OnceClosure callback_;
    size_t delay_;
  };

  // Map of delayed callbacks.
  std::map<std::string, DelayedCallback*> action_extra_requests_;

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeBluetoothGattCharacteristicClient> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattCharacteristicClient);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_CHARACTERISTIC_CLIENT_H_

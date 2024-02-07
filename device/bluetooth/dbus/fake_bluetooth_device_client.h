// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_DEVICE_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_DEVICE_CLIENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"

namespace bluez {

// FakeBluetoothDeviceClient simulates the behavior of the Bluetooth Daemon
// device objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothDeviceClient
    : public BluetoothDeviceClient {
 public:
  struct Properties : public BluetoothDeviceClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  struct SimulatedPairingOptions {
    SimulatedPairingOptions();
    ~SimulatedPairingOptions();

    bool incoming = false;
    std::string pairing_method;
    std::string pairing_auth_token;
    std::string pairing_action;
  };

  // Stores properties of a device that is about to be created.
  struct IncomingDeviceProperties {
    IncomingDeviceProperties();
    ~IncomingDeviceProperties();

    std::string device_address;
    std::string device_alias;
    int device_class = 0;
    std::string device_name;
    std::string device_path;
    bool is_trusted = true;
    bool incoming = false;
    std::string pairing_action;
    std::string pairing_auth_token;
    std::string pairing_method;
  };

  FakeBluetoothDeviceClient();
  ~FakeBluetoothDeviceClient() override;

  // Causes Connect() calls to never finish.
  void LeaveConnectionsPending();

  // BluetoothDeviceClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void Connect(const dbus::ObjectPath& object_path,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  void ConnectClassic(const dbus::ObjectPath& object_path,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override;
  void ConnectLE(const dbus::ObjectPath& object_path,
                 base::OnceClosure callback,
                 ErrorCallback error_callback) override;
  void Disconnect(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  void DisconnectLE(const dbus::ObjectPath& object_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override;
  void ConnectProfile(const dbus::ObjectPath& object_path,
                      const std::string& uuid,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override;
  void DisconnectProfile(const dbus::ObjectPath& object_path,
                         const std::string& uuid,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override;
  void Pair(const dbus::ObjectPath& object_path,
            base::OnceClosure callback,
            ErrorCallback error_callback) override;
  void CancelPairing(const dbus::ObjectPath& object_path,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override;
  void GetConnInfo(const dbus::ObjectPath& object_path,
                   ConnInfoCallback callback,
                   ErrorCallback error_callback) override;
  void SetLEConnectionParameters(const dbus::ObjectPath& object_path,
                                 const ConnectionParameters& conn_params,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override;
  void GetServiceRecords(const dbus::ObjectPath& object_path,
                         ServiceRecordsCallback callback,
                         ErrorCallback error_callback) override;
  void ExecuteWrite(const dbus::ObjectPath& object_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override;
  void AbortWrite(const dbus::ObjectPath& object_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;

  void SetSimulationIntervalMs(int interval_ms);

  // Simulates discovery of devices for the given adapter.
  void BeginDiscoverySimulation(const dbus::ObjectPath& adapter_path);
  void EndDiscoverySimulation(const dbus::ObjectPath& adapter_path);

  // Simulates incoming pairing of devices for the given adapter.
  void BeginIncomingPairingSimulation(const dbus::ObjectPath& adapter_path);
  void EndIncomingPairingSimulation(const dbus::ObjectPath& adapter_path);

  // Creates a device from the set we return for the given adapter.
  void CreateDevice(const dbus::ObjectPath& adapter_path,
                    const dbus::ObjectPath& device_path);

  // Creates a device with the given properties.
  void CreateDeviceWithProperties(const dbus::ObjectPath& adapter_path,
                                  const IncomingDeviceProperties& props);

  // Creates and returns a list of dictionary objects as a Value, which contain
  // all the data from the constants for devices with predefined behavior.
  base::Value GetBluetoothDevicesAsDictionaries() const;

  SimulatedPairingOptions* GetPairingOptions(
      const dbus::ObjectPath& object_path);

  // Removes a device from the set we return for the given adapter.
  void RemoveDevice(const dbus::ObjectPath& adapter_path,
                    const dbus::ObjectPath& device_path);

  // Simulates a pairing for the device with the given D-Bus object path,
  // |object_path|. Set |incoming_request| to true if simulating an incoming
  // pairing request, false for an outgoing one. On successful completion
  // |callback| will be called, on failure, |error_callback| is called.
  void SimulatePairing(const dbus::ObjectPath& object_path,
                       bool incoming_request,
                       base::OnceClosure callback,
                       ErrorCallback error_callback);

  // Updates the connection properties of the fake device that will be returned
  // by GetConnInfo.
  void UpdateConnectionInfo(uint16_t connection_rssi,
                            uint16_t transmit_power,
                            uint16_t max_transmit_power);

  // Test specific functions:
  // Remove all test devices from this client.
  void RemoveAllDevices();

  // Create a test Bluetooth device with the given properties.
  void CreateTestDevice(
      const dbus::ObjectPath& adapter_path,
      const std::optional<std::string> name,
      const std::string alias,
      const std::string device_address,
      const std::vector<std::string>& service_uuids,
      device::BluetoothTransport type,
      const std::map<std::string, std::vector<uint8_t>>& service_data,
      const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data);

  void set_delay_start_discovery(bool value) { delay_start_discovery_ = value; }

  // Updates the inquiry RSSI property of fake device with object path
  // |object_path| to |rssi|, if the fake device exists.
  void UpdateDeviceRSSI(const dbus::ObjectPath& object_path, int16_t rssi);

  // Updates service UUIDs, service data, and manufacturer data of the fake
  // device with the given |object_path|. The |service_uuids| values are
  // appended, while |service_data| and |manufacturer_data| are merged into the
  // existing property values. Has no effect if the fake device does not exist.
  void UpdateServiceAndManufacturerData(
      const dbus::ObjectPath& object_path,
      const std::vector<std::string>& service_uuids,
      const std::map<std::string, std::vector<uint8_t>>& service_data,
      const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data);

  // Updates the EIR property of fake device with object path |object_path| to
  // |eir|, if the fake device exists.
  void UpdateEIR(const dbus::ObjectPath& object_path,
                 const std::vector<uint8_t>& eir);

  // Adds a pending prepare write request to |object_path|.
  void AddPrepareWriteRequest(const dbus::ObjectPath& object_path,
                              const std::vector<uint8_t>& value);

  static const char kTestPinCode[];
  static const int kTestPassKey;

  static const char kPairingMethodNone[];
  static const char kPairingMethodPinCode[];
  static const char kPairingMethodPassKey[];

  static const char kPairingActionConfirmation[];
  static const char kPairingActionDisplay[];
  static const char kPairingActionFail[];
  static const char kPairingActionRequest[];

  // Object paths, names, addresses and bluetooth classes of the devices
  // we can emulate.
  static const char kPairedDevicePath[];
  static const char kPairedDeviceName[];
  static const char kPairedDeviceAlias[];
  static const char kPairedDeviceAddress[];
  static const uint32_t kPairedDeviceClass;

  static const char kLegacyAutopairPath[];
  static const char kLegacyAutopairName[];
  static const char kLegacyAutopairAddress[];
  static const uint32_t kLegacyAutopairClass;

  static const char kDisplayPinCodePath[];
  static const char kDisplayPinCodeName[];
  static const char kDisplayPinCodeAddress[];
  static const uint32_t kDisplayPinCodeClass;

  static const char kVanishingDevicePath[];
  static const char kVanishingDeviceName[];
  static const char kVanishingDeviceAddress[];
  static const uint32_t kVanishingDeviceClass;

  static const char kConnectUnpairablePath[];
  static const char kConnectUnpairableName[];
  static const char kConnectUnpairableAddress[];
  static const uint32_t kConnectUnpairableClass;

  static const char kDisplayPasskeyPath[];
  static const char kDisplayPasskeyName[];
  static const char kDisplayPasskeyAddress[];
  static const uint32_t kDisplayPasskeyClass;

  static const char kRequestPinCodePath[];
  static const char kRequestPinCodeName[];
  static const char kRequestPinCodeAddress[];
  static const uint32_t kRequestPinCodeClass;

  static const char kConfirmPasskeyPath[];
  static const char kConfirmPasskeyName[];
  static const char kConfirmPasskeyAddress[];
  static const uint32_t kConfirmPasskeyClass;

  static const char kRequestPasskeyPath[];
  static const char kRequestPasskeyName[];
  static const char kRequestPasskeyAddress[];
  static const uint32_t kRequestPasskeyClass;

  static const char kUnconnectableDevicePath[];
  static const char kUnconnectableDeviceName[];
  static const char kUnconnectableDeviceAddress[];
  static const uint32_t kUnconnectableDeviceClass;

  static const char kUnpairableDevicePath[];
  static const char kUnpairableDeviceName[];
  static const char kUnpairableDeviceAddress[];
  static const uint32_t kUnpairableDeviceClass;

  static const char kJustWorksPath[];
  static const char kJustWorksName[];
  static const char kJustWorksAddress[];
  static const uint32_t kJustWorksClass;

  static const char kLowEnergyPath[];
  static const char kLowEnergyName[];
  static const char kLowEnergyAddress[];
  static const uint32_t kLowEnergyClass;

  static const char kDualPath[];
  static const char kDualName[];
  static const char kDualAddress[];

  static const char kPairedUnconnectableDevicePath[];
  static const char kPairedUnconnectableDeviceName[];
  static const char kPairedUnconnectableDeviceAlias[];
  static const char kPairedUnconnectableDeviceAddress[];
  static const uint32_t kPairedUnconnectableDeviceClass;

  static const char kConnectedTrustedNotPairedDevicePath[];
  static const char kConnectedTrustedNotPairedDeviceAddress[];
  static const char kConnectedTrustedNotPairedDeviceName[];
  static const uint32_t kConnectedTrustedNotPairedDeviceClass;

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  void DiscoverySimulationTimer();
  void IncomingPairingSimulationTimer();

  void CompleteSimulatedPairing(const dbus::ObjectPath& object_path,
                                base::OnceClosure callback,
                                ErrorCallback error_callback);
  void TimeoutSimulatedPairing(const dbus::ObjectPath& object_path,
                               ErrorCallback error_callback);
  void CancelSimulatedPairing(const dbus::ObjectPath& object_path,
                              ErrorCallback error_callback);
  void RejectSimulatedPairing(const dbus::ObjectPath& object_path,
                              ErrorCallback error_callback);
  void FailSimulatedPairing(const dbus::ObjectPath& object_path,
                            ErrorCallback error_callback);
  void AddInputDeviceIfNeeded(const dbus::ObjectPath& object_path,
                              Properties* properties);

  // If fake device with |object_path| exists, sets its inquiry RSSI property
  // to false and notifies that the property changed.
  void InvalidateDeviceRSSI(const dbus::ObjectPath& object_path);

  void PinCodeCallback(const dbus::ObjectPath& object_path,
                       base::OnceClosure callback,
                       ErrorCallback error_callback,
                       BluetoothAgentServiceProvider::Delegate::Status status,
                       const std::string& pincode);
  void PasskeyCallback(const dbus::ObjectPath& object_path,
                       base::OnceClosure callback,
                       ErrorCallback error_callback,
                       BluetoothAgentServiceProvider::Delegate::Status status,
                       uint32_t passkey);
  void ConfirmationCallback(
      const dbus::ObjectPath& object_path,
      base::OnceClosure callback,
      ErrorCallback error_callback,
      BluetoothAgentServiceProvider::Delegate::Status status);
  void SimulateKeypress(uint16_t entered,
                        const dbus::ObjectPath& object_path,
                        base::OnceClosure callback,
                        ErrorCallback error_callback);

  void ConnectionCallback(
      const dbus::ObjectPath& object_path,
      base::OnceClosure callback,
      ErrorCallback error_callback,
      BluetoothProfileServiceProvider::Delegate::Status status);
  void DisconnectionCallback(
      const dbus::ObjectPath& object_path,
      base::OnceClosure callback,
      ErrorCallback error_callback,
      BluetoothProfileServiceProvider::Delegate::Status status);

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;

  using PropertiesMap =
      std::map<const dbus::ObjectPath, std::unique_ptr<Properties>>;
  PropertiesMap properties_map_;
  std::vector<dbus::ObjectPath> device_list_;

  // Properties which are used to decied which method of pairing should
  // be done on request.
  using PairingOptionsMap = std::map<const dbus::ObjectPath,
                                     std::unique_ptr<SimulatedPairingOptions>>;
  PairingOptionsMap pairing_options_map_;

  int simulation_interval_ms_;
  uint32_t discovery_simulation_step_;
  uint32_t incoming_pairing_simulation_step_;
  bool pairing_cancelled_;

  int16_t connection_rssi_;
  int16_t transmit_power_;
  int16_t max_transmit_power_;

  // Controls the fake behavior to allow more extensive UI testing without
  // having to cycle the discovery simulation.
  bool delay_start_discovery_;

  // Pending prepare write requests.
  std::vector<std::pair<dbus::ObjectPath, std::vector<uint8_t>>>
      prepare_write_requests_;

  bool should_leave_connections_pending_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_DEVICE_CLIENT_H_

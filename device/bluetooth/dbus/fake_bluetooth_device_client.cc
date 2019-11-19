// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_service_provider.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

// Default interval between simulated events.
const int kSimulationIntervalMs = 750;

// Minimum and maximum bounds for randomly generated RSSI values.
const int kMinRSSI = -90;
const int kMaxRSSI = -30;

// The default value of connection info properties from GetConnInfo().
const int kUnkownPower = 127;

// This is meant to delay the removal of a pre defined device until the
// developer has time to see it.
const int kVanishingDevicePairTimeMultiplier = 4;

// Meant to delay a pair request for an observable amount of time.
const int kIncomingSimulationPairTimeMultiplier = 45;

// Meant to delay a request that asks for pair requests for an observable
// amount of time.
const int kIncomingSimulationStartPairTimeMultiplier = 30;

// This allows the PIN code dialog to be shown for a long enough time to see
// the PIN code UI in detail.
const int kPinCodeDevicePairTimeMultiplier = 7;

// This allows the pairing dialog to be shown for a long enough time to see
// its UI in detail.
const int kSimulateNormalPairTimeMultiplier = 3;

void SimulatedProfileSocket(int fd) {
  // Simulate a server-side socket of a profile; read data from the socket,
  // write it back, and then close.
  char buf[1024];
  ssize_t len;
  ssize_t count;

  len = read(fd, buf, sizeof buf);
  if (len < 0) {
    close(fd);
    return;
  }

  count = len;
  len = write(fd, buf, count);
  if (len < 0) {
    close(fd);
    return;
  }

  close(fd);
}

void SimpleErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
  VLOG(1) << "Bluetooth Error: " << error_name << ": " << error_message;
}

BluetoothDeviceClient::ServiceRecordList CreateFakeServiceRecords() {
  BluetoothDeviceClient::ServiceRecordList records;

  std::unique_ptr<BluetoothServiceRecordBlueZ> record1 =
      std::make_unique<BluetoothServiceRecordBlueZ>();
  // ID 0 = handle.
  record1->AddRecordEntry(
      0x0, BluetoothServiceAttributeValueBlueZ(
               BluetoothServiceAttributeValueBlueZ::UINT, 4,
               std::make_unique<base::Value>(static_cast<int32_t>(0x1337))));
  // ID 1 = service class id list.
  std::unique_ptr<BluetoothServiceAttributeValueBlueZ::Sequence> class_id_list =
      std::make_unique<BluetoothServiceAttributeValueBlueZ::Sequence>();
  class_id_list->emplace_back(BluetoothServiceAttributeValueBlueZ::UUID, 4,
                              std::make_unique<base::Value>("1802"));
  record1->AddRecordEntry(
      0x1, BluetoothServiceAttributeValueBlueZ(std::move(class_id_list)));
  records.emplace_back(*record1);

  std::unique_ptr<BluetoothServiceRecordBlueZ> record2 =
      std::make_unique<BluetoothServiceRecordBlueZ>();
  // ID 0 = handle.
  record2->AddRecordEntry(
      0x0,
      BluetoothServiceAttributeValueBlueZ(
          BluetoothServiceAttributeValueBlueZ::UINT, 4,
          std::make_unique<base::Value>(static_cast<int32_t>(0xffffffff))));
  records.emplace_back(*record2);

  return records;
}

}  // namespace

const char FakeBluetoothDeviceClient::kTestPinCode[] = "123456";
const int FakeBluetoothDeviceClient::kTestPassKey = 123456;

const char FakeBluetoothDeviceClient::kPairingMethodNone[] = "None";
const char FakeBluetoothDeviceClient::kPairingMethodPinCode[] = "PIN Code";
const char FakeBluetoothDeviceClient::kPairingMethodPassKey[] = "PassKey";

const char FakeBluetoothDeviceClient::kPairingActionConfirmation[] =
    "Confirmation";
const char FakeBluetoothDeviceClient::kPairingActionDisplay[] = "Display";
const char FakeBluetoothDeviceClient::kPairingActionFail[] = "Fail";
const char FakeBluetoothDeviceClient::kPairingActionRequest[] = "Request";

const char FakeBluetoothDeviceClient::kPairedDevicePath[] = "/fake/hci0/dev0";
const char FakeBluetoothDeviceClient::kPairedDeviceAddress[] =
    "00:11:22:33:44:55";
const char FakeBluetoothDeviceClient::kPairedDeviceName[] =
    "Fake Device (name)";
const char FakeBluetoothDeviceClient::kPairedDeviceAlias[] =
    "Fake Device (alias)";
const uint32_t FakeBluetoothDeviceClient::kPairedDeviceClass = 0x000104;

const char FakeBluetoothDeviceClient::kLegacyAutopairPath[] = "/fake/hci0/dev1";
const char FakeBluetoothDeviceClient::kLegacyAutopairAddress[] =
    "28:CF:DA:00:00:00";
const char FakeBluetoothDeviceClient::kLegacyAutopairName[] =
    "Bluetooth 2.0 Mouse";
const uint32_t FakeBluetoothDeviceClient::kLegacyAutopairClass = 0x002580;

const char FakeBluetoothDeviceClient::kDisplayPinCodePath[] = "/fake/hci0/dev2";
const char FakeBluetoothDeviceClient::kDisplayPinCodeAddress[] =
    "28:37:37:00:00:00";
const char FakeBluetoothDeviceClient::kDisplayPinCodeName[] =
    "Bluetooth 2.0 Keyboard";
const uint32_t FakeBluetoothDeviceClient::kDisplayPinCodeClass = 0x002540;

const char FakeBluetoothDeviceClient::kVanishingDevicePath[] =
    "/fake/hci0/dev3";
const char FakeBluetoothDeviceClient::kVanishingDeviceAddress[] =
    "01:02:03:04:05:06";
const char FakeBluetoothDeviceClient::kVanishingDeviceName[] =
    "Vanishing Device";
const uint32_t FakeBluetoothDeviceClient::kVanishingDeviceClass = 0x000104;

const char FakeBluetoothDeviceClient::kConnectUnpairablePath[] =
    "/fake/hci0/dev4";
const char FakeBluetoothDeviceClient::kConnectUnpairableAddress[] =
    "7C:ED:8D:00:00:00";
const char FakeBluetoothDeviceClient::kConnectUnpairableName[] =
    "Unpairable Device";
const uint32_t FakeBluetoothDeviceClient::kConnectUnpairableClass = 0x002580;

const char FakeBluetoothDeviceClient::kDisplayPasskeyPath[] = "/fake/hci0/dev5";
const char FakeBluetoothDeviceClient::kDisplayPasskeyAddress[] =
    "00:0F:F6:00:00:00";
const char FakeBluetoothDeviceClient::kDisplayPasskeyName[] =
    "Bluetooth 2.1+ Keyboard";
const uint32_t FakeBluetoothDeviceClient::kDisplayPasskeyClass = 0x002540;

const char FakeBluetoothDeviceClient::kRequestPinCodePath[] = "/fake/hci0/dev6";
const char FakeBluetoothDeviceClient::kRequestPinCodeAddress[] =
    "00:24:BE:00:00:00";
const char FakeBluetoothDeviceClient::kRequestPinCodeName[] = "PIN Device";
const uint32_t FakeBluetoothDeviceClient::kRequestPinCodeClass = 0x240408;

const char FakeBluetoothDeviceClient::kConfirmPasskeyPath[] = "/fake/hci0/dev7";
const char FakeBluetoothDeviceClient::kConfirmPasskeyAddress[] =
    "20:7D:74:00:00:00";
const char FakeBluetoothDeviceClient::kConfirmPasskeyName[] = "Phone";
const uint32_t FakeBluetoothDeviceClient::kConfirmPasskeyClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kRequestPasskeyPath[] = "/fake/hci0/dev8";
const char FakeBluetoothDeviceClient::kRequestPasskeyAddress[] =
    "20:7D:74:00:00:01";
const char FakeBluetoothDeviceClient::kRequestPasskeyName[] = "Passkey Device";
const uint32_t FakeBluetoothDeviceClient::kRequestPasskeyClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kUnconnectableDevicePath[] =
    "/fake/hci0/dev9";
const char FakeBluetoothDeviceClient::kUnconnectableDeviceAddress[] =
    "20:7D:74:00:00:02";
const char FakeBluetoothDeviceClient::kUnconnectableDeviceName[] =
    "Unconnectable Device";
const uint32_t FakeBluetoothDeviceClient::kUnconnectableDeviceClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kUnpairableDevicePath[] =
    "/fake/hci0/devA";
const char FakeBluetoothDeviceClient::kUnpairableDeviceAddress[] =
    "20:7D:74:00:00:03";
const char FakeBluetoothDeviceClient::kUnpairableDeviceName[] =
    "Unpairable Device";
const uint32_t FakeBluetoothDeviceClient::kUnpairableDeviceClass = 0x002540;

const char FakeBluetoothDeviceClient::kJustWorksPath[] = "/fake/hci0/devB";
const char FakeBluetoothDeviceClient::kJustWorksAddress[] = "00:0C:8A:00:00:00";
const char FakeBluetoothDeviceClient::kJustWorksName[] = "Just-Works Device";
const uint32_t FakeBluetoothDeviceClient::kJustWorksClass = 0x240428;

const char FakeBluetoothDeviceClient::kLowEnergyPath[] = "/fake/hci0/devC";
const char FakeBluetoothDeviceClient::kLowEnergyAddress[] = "00:1A:11:00:15:30";
const char FakeBluetoothDeviceClient::kLowEnergyName[] =
    "Bluetooth 4.0 Heart Rate Monitor";
const uint32_t FakeBluetoothDeviceClient::kLowEnergyClass =
    0x000918;  // Major class "Health", Minor class "Heart/Pulse Rate Monitor."

const char FakeBluetoothDeviceClient::kDualPath[] = "/fake/hci0/devF";
const char FakeBluetoothDeviceClient::kDualAddress[] = "00:1A:11:00:15:40";
const char FakeBluetoothDeviceClient::kDualName[] =
    "Bluetooth 4.0 Battery Monitor";

const char FakeBluetoothDeviceClient::kPairedUnconnectableDevicePath[] =
    "/fake/hci0/devD";
const char FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress[] =
    "20:7D:74:00:00:04";
const char FakeBluetoothDeviceClient::kPairedUnconnectableDeviceName[] =
    "Paired Unconnectable Device (name)";
const char FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAlias[] =
    "Paired Unconnectable Device (alias)";
const uint32_t FakeBluetoothDeviceClient::kPairedUnconnectableDeviceClass =
    0x000104;

const char FakeBluetoothDeviceClient::kConnectedTrustedNotPairedDevicePath[] =
    "/fake/hci0/devE";
const char
    FakeBluetoothDeviceClient::kConnectedTrustedNotPairedDeviceAddress[] =
        "11:22:33:44:55:66";
const char FakeBluetoothDeviceClient::kConnectedTrustedNotPairedDeviceName[] =
    "Connected Pairable Device";
const uint32_t
    FakeBluetoothDeviceClient::kConnectedTrustedNotPairedDeviceClass = 0x7a020c;

FakeBluetoothDeviceClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothDeviceClient::Properties(
          NULL,
          bluetooth_device::kBluetoothDeviceInterface,
          callback) {}

FakeBluetoothDeviceClient::Properties::~Properties() = default;

void FakeBluetoothDeviceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothDeviceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothDeviceClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() == trusted.name()) {
    std::move(callback).Run(true);
    property->ReplaceValueWithSetValue();
  } else {
    std::move(callback).Run(false);
  }
}

FakeBluetoothDeviceClient::SimulatedPairingOptions::SimulatedPairingOptions() =
    default;

FakeBluetoothDeviceClient::SimulatedPairingOptions::~SimulatedPairingOptions() =
    default;

FakeBluetoothDeviceClient::IncomingDeviceProperties::
    IncomingDeviceProperties() = default;

FakeBluetoothDeviceClient::IncomingDeviceProperties::
    ~IncomingDeviceProperties() = default;

FakeBluetoothDeviceClient::FakeBluetoothDeviceClient()
    : simulation_interval_ms_(kSimulationIntervalMs),
      discovery_simulation_step_(0),
      incoming_pairing_simulation_step_(0),
      pairing_cancelled_(false),
      connection_rssi_(kUnkownPower),
      transmit_power_(kUnkownPower),
      max_transmit_power_(kUnkownPower),
      delay_start_discovery_(false),
      should_leave_connections_pending_(false) {
  std::unique_ptr<Properties> properties(new Properties(
      base::Bind(&FakeBluetoothDeviceClient::OnPropertyChanged,
                 base::Unretained(this), dbus::ObjectPath(kPairedDevicePath))));
  properties->address.ReplaceValue(kPairedDeviceAddress);
  properties->bluetooth_class.ReplaceValue(kPairedDeviceClass);
  properties->name.ReplaceValue(kPairedDeviceName);
  properties->name.set_valid(true);
  properties->alias.ReplaceValue(kPairedDeviceAlias);
  properties->paired.ReplaceValue(true);
  properties->trusted.ReplaceValue(true);
  properties->adapter.ReplaceValue(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  std::vector<std::string> uuids;
  uuids.push_back("00001800-0000-1000-8000-00805f9b34fb");
  uuids.push_back("00001801-0000-1000-8000-00805f9b34fb");
  properties->uuids.ReplaceValue(uuids);

  properties->modalias.ReplaceValue("usb:v05ACp030Dd0306");

  properties_map_.insert(std::make_pair(dbus::ObjectPath(kPairedDevicePath),
                                        std::move(properties)));
  device_list_.push_back(dbus::ObjectPath(kPairedDevicePath));

  properties.reset(new Properties(base::Bind(
      &FakeBluetoothDeviceClient::OnPropertyChanged, base::Unretained(this),
      dbus::ObjectPath(kPairedUnconnectableDevicePath))));
  properties->address.ReplaceValue(kPairedUnconnectableDeviceAddress);
  properties->bluetooth_class.ReplaceValue(kPairedUnconnectableDeviceClass);
  properties->name.ReplaceValue(kPairedUnconnectableDeviceName);
  properties->name.set_valid(true);
  properties->alias.ReplaceValue(kPairedUnconnectableDeviceAlias);
  properties->paired.ReplaceValue(true);
  properties->trusted.ReplaceValue(true);
  properties->adapter.ReplaceValue(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  properties->uuids.ReplaceValue(uuids);

  properties->modalias.ReplaceValue("usb:v05ACp030Dd0306");

  properties_map_.insert(std::make_pair(
      dbus::ObjectPath(kPairedUnconnectableDevicePath), std::move(properties)));
  device_list_.push_back(dbus::ObjectPath(kPairedUnconnectableDevicePath));
}

FakeBluetoothDeviceClient::~FakeBluetoothDeviceClient() = default;

void FakeBluetoothDeviceClient::LeaveConnectionsPending() {
  should_leave_connections_pending_ = true;
}

void FakeBluetoothDeviceClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothDeviceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothDeviceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeBluetoothDeviceClient::GetDevicesForAdapter(
    const dbus::ObjectPath& adapter_path) {
  if (adapter_path ==
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath))
    return device_list_;
  else
    return std::vector<dbus::ObjectPath>();
}

FakeBluetoothDeviceClient::Properties* FakeBluetoothDeviceClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second.get();
  return NULL;
}

FakeBluetoothDeviceClient::SimulatedPairingOptions*
FakeBluetoothDeviceClient::GetPairingOptions(
    const dbus::ObjectPath& object_path) {
  PairingOptionsMap::const_iterator iter =
      pairing_options_map_.find(object_path);
  if (iter != pairing_options_map_.end())
    return iter->second.get();
  return iter != pairing_options_map_.end() ? iter->second.get() : nullptr;
}

void FakeBluetoothDeviceClient::Connect(const dbus::ObjectPath& object_path,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  VLOG(1) << "Connect: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->connected.value() == true) {
    // Already connected.
    std::move(callback).Run();
    return;
  }

  if (should_leave_connections_pending_)
    return;

  if (properties->paired.value() != true &&
      object_path != dbus::ObjectPath(kConnectUnpairablePath) &&
      object_path != dbus::ObjectPath(kLowEnergyPath)) {
    // Must be paired.
    std::move(error_callback).Run(bluetooth_device::kErrorFailed, "Not paired");
    return;
  } else if (properties->paired.value() == true &&
             (object_path == dbus::ObjectPath(kUnconnectableDevicePath) ||
              object_path ==
                  dbus::ObjectPath(kPairedUnconnectableDevicePath))) {
    // Must not be paired
    std::move(error_callback)
        .Run(bluetooth_device::kErrorFailed, "Connection fails while paired");
    return;
  }

  // The device can be connected.
  properties->connected.ReplaceValue(true);
  std::move(callback).Run();

  // Expose GATT services if connected to LE device.
  if (object_path == dbus::ObjectPath(kLowEnergyPath)) {
    FakeBluetoothGattServiceClient* gatt_service_client =
        static_cast<FakeBluetoothGattServiceClient*>(
            bluez::BluezDBusManager::Get()->GetBluetoothGattServiceClient());
    gatt_service_client->ExposeHeartRateService(object_path);
    properties->services_resolved.ReplaceValue(true);
  }

  AddInputDeviceIfNeeded(object_path, properties);
}

void FakeBluetoothDeviceClient::Disconnect(const dbus::ObjectPath& object_path,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  VLOG(1) << "Disconnect: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (!properties->connected.value()) {
    std::move(error_callback)
        .Run(bluetooth_device::kErrorNotConnected, "Not Connected");
    return;
  }

  // Hide the Heart Rate Service if disconnected from LE device.
  if (object_path == dbus::ObjectPath(kLowEnergyPath)) {
    FakeBluetoothGattServiceClient* gatt_service_client =
        static_cast<FakeBluetoothGattServiceClient*>(
            bluez::BluezDBusManager::Get()->GetBluetoothGattServiceClient());
    gatt_service_client->HideHeartRateService();
  }

  std::move(callback).Run();
  properties->connected.ReplaceValue(false);
}

void FakeBluetoothDeviceClient::ConnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "ConnectProfile: " << object_path.value() << " " << uuid;

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothProfileManagerClient());
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(uuid);
  if (profile_service_provider == NULL) {
    std::move(error_callback).Run(kNoResponseError, "Missing profile");
    return;
  }

  if (object_path == dbus::ObjectPath(kPairedUnconnectableDevicePath)) {
    std::move(error_callback)
        .Run(bluetooth_device::kErrorFailed, "unconnectable");
    return;
  }

  // Make a socket pair of a compatible type with the type used by Bluetooth;
  // spin up a thread to simulate the server side and wrap the client side in
  // a D-Bus file descriptor object.
  int socket_type = SOCK_STREAM;
  if (uuid == FakeBluetoothProfileManagerClient::kL2capUuid)
    socket_type = SOCK_SEQPACKET;

  int fds[2];
  if (socketpair(AF_UNIX, socket_type, 0, fds) < 0) {
    std::move(error_callback).Run(kNoResponseError, "socketpair call failed");
    return;
  }

  int args;
  args = fcntl(fds[1], F_GETFL, NULL);
  if (args < 0) {
    std::move(error_callback)
        .Run(kNoResponseError, "failed to get socket flags");
    return;
  }

  args |= O_NONBLOCK;
  if (fcntl(fds[1], F_SETFL, args) < 0) {
    std::move(error_callback)
        .Run(kNoResponseError, "failed to set socket non-blocking");
    return;
  }

  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::MayBlock(),
                  base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                 base::BindOnce(&SimulatedProfileSocket, fds[0]));

  base::ScopedFD fd(fds[1]);

  // Post the new connection to the service provider.
  BluetoothProfileServiceProvider::Delegate::Options options;

  profile_service_provider->NewConnection(
      object_path, std::move(fd), options,
      base::BindOnce(&FakeBluetoothDeviceClient::ConnectionCallback,
                     base::Unretained(this), object_path, std::move(callback),
                     std::move(error_callback)));
}

void FakeBluetoothDeviceClient::DisconnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "DisconnectProfile: " << object_path.value() << " " << uuid;

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothProfileManagerClient());
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(uuid);
  if (profile_service_provider == NULL) {
    std::move(error_callback).Run(kNoResponseError, "Missing profile");
    return;
  }

  profile_service_provider->RequestDisconnection(
      object_path,
      base::BindOnce(&FakeBluetoothDeviceClient::DisconnectionCallback,
                     base::Unretained(this), object_path, std::move(callback),
                     std::move(error_callback)));
}

void FakeBluetoothDeviceClient::Pair(const dbus::ObjectPath& object_path,
                                     base::OnceClosure callback,
                                     ErrorCallback error_callback) {
  VLOG(1) << "Pair: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->paired.value() == true) {
    // Already paired.
    std::move(callback).Run();
    return;
  }

  SimulatePairing(object_path, false, std::move(callback),
                  std::move(error_callback));
}

void FakeBluetoothDeviceClient::CancelPairing(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "CancelPairing: " << object_path.value();
  pairing_cancelled_ = true;
  std::move(callback).Run();
}

void FakeBluetoothDeviceClient::GetConnInfo(const dbus::ObjectPath& object_path,
                                            ConnInfoCallback callback,
                                            ErrorCallback error_callback) {
  Properties* properties = GetProperties(object_path);
  if (!properties->connected.value()) {
    std::move(error_callback)
        .Run(bluetooth_device::kErrorNotConnected, "Not Connected");
    return;
  }

  std::move(callback).Run(connection_rssi_, transmit_power_,
                          max_transmit_power_);
}

void FakeBluetoothDeviceClient::SetLEConnectionParameters(
    const dbus::ObjectPath& object_path,
    const ConnectionParameters& conn_params,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  Properties* properties = GetProperties(object_path);
  if (!properties->type.is_valid() || properties->type.value() == kTypeBredr) {
    std::move(error_callback)
        .Run(bluetooth_device::kErrorFailed, "BR/EDR devices not supported");
    return;
  }

  std::move(callback).Run();
}

void FakeBluetoothDeviceClient::GetServiceRecords(
    const dbus::ObjectPath& object_path,
    ServiceRecordsCallback callback,
    ErrorCallback error_callback) {
  Properties* properties = GetProperties(object_path);
  if (!properties->connected.value()) {
    std::move(error_callback)
        .Run(bluetooth_device::kErrorNotConnected, "Not Connected");
    return;
  }
  std::move(callback).Run(CreateFakeServiceRecords());
}

void FakeBluetoothDeviceClient::ExecuteWrite(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  for (const auto& prepare_write_request : prepare_write_requests_) {
    bluez::BluezDBusManager::Get()
        ->GetBluetoothGattCharacteristicClient()
        ->WriteValue(prepare_write_request.first, prepare_write_request.second,
                     base::DoNothing(), base::DoNothing());
  }
  prepare_write_requests_.clear();
  std::move(callback).Run();
}

void FakeBluetoothDeviceClient::AbortWrite(const dbus::ObjectPath& object_path,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  prepare_write_requests_.clear();
  std::move(callback).Run();
}

void FakeBluetoothDeviceClient::BeginDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "starting discovery simulation";

  discovery_simulation_step_ = 1;
  int delay = delay_start_discovery_ ? simulation_interval_ms_ : 0;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                     base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(delay));
}

void FakeBluetoothDeviceClient::EndDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "stopping discovery simulation";
  discovery_simulation_step_ = 0;
  InvalidateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath));
}

void FakeBluetoothDeviceClient::BeginIncomingPairingSimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "starting incoming pairing simulation";

  incoming_pairing_simulation_step_ = 1;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::IncomingPairingSimulationTimer,
                     base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(
          kIncomingSimulationStartPairTimeMultiplier *
          simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::EndIncomingPairingSimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "stopping incoming pairing simulation";
  incoming_pairing_simulation_step_ = 0;
}

void FakeBluetoothDeviceClient::SetSimulationIntervalMs(int interval_ms) {
  simulation_interval_ms_ = interval_ms;
}

void FakeBluetoothDeviceClient::CreateDevice(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  if (base::Contains(device_list_, device_path))
    return;

  std::unique_ptr<Properties> properties(
      new Properties(base::Bind(&FakeBluetoothDeviceClient::OnPropertyChanged,
                                base::Unretained(this), device_path)));
  properties->adapter.ReplaceValue(adapter_path);
  properties->type.ReplaceValue(BluetoothDeviceClient::kTypeBredr);
  properties->type.set_valid(true);

  if (device_path == dbus::ObjectPath(kLegacyAutopairPath)) {
    properties->address.ReplaceValue(kLegacyAutopairAddress);
    properties->bluetooth_class.ReplaceValue(kLegacyAutopairClass);
    properties->name.ReplaceValue(kLegacyAutopairName);
    properties->name.set_valid(true);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kDisplayPinCodePath)) {
    properties->address.ReplaceValue(kDisplayPinCodeAddress);
    properties->bluetooth_class.ReplaceValue(kDisplayPinCodeClass);
    properties->name.ReplaceValue(kDisplayPinCodeName);
    properties->name.set_valid(true);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kVanishingDevicePath)) {
    properties->address.ReplaceValue(kVanishingDeviceAddress);
    properties->bluetooth_class.ReplaceValue(kVanishingDeviceClass);
    properties->name.ReplaceValue(kVanishingDeviceName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kConnectUnpairablePath)) {
    properties->address.ReplaceValue(kConnectUnpairableAddress);
    properties->bluetooth_class.ReplaceValue(kConnectUnpairableClass);
    properties->name.ReplaceValue(kConnectUnpairableName);
    properties->name.set_valid(true);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kDisplayPasskeyPath)) {
    properties->address.ReplaceValue(kDisplayPasskeyAddress);
    properties->bluetooth_class.ReplaceValue(kDisplayPasskeyClass);
    properties->name.ReplaceValue(kDisplayPasskeyName);
    properties->name.set_valid(true);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kRequestPinCodePath)) {
    properties->address.ReplaceValue(kRequestPinCodeAddress);
    properties->bluetooth_class.ReplaceValue(kRequestPinCodeClass);
    properties->name.ReplaceValue(kRequestPinCodeName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kConfirmPasskeyPath)) {
    properties->address.ReplaceValue(kConfirmPasskeyAddress);
    properties->bluetooth_class.ReplaceValue(kConfirmPasskeyClass);
    properties->name.ReplaceValue(kConfirmPasskeyName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kRequestPasskeyPath)) {
    properties->address.ReplaceValue(kRequestPasskeyAddress);
    properties->bluetooth_class.ReplaceValue(kRequestPasskeyClass);
    properties->name.ReplaceValue(kRequestPasskeyName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kUnconnectableDevicePath)) {
    properties->address.ReplaceValue(kUnconnectableDeviceAddress);
    properties->bluetooth_class.ReplaceValue(kUnconnectableDeviceClass);
    properties->name.ReplaceValue(kUnconnectableDeviceName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kUnpairableDevicePath)) {
    properties->address.ReplaceValue(kUnpairableDeviceAddress);
    properties->bluetooth_class.ReplaceValue(kUnpairableDeviceClass);
    properties->name.ReplaceValue(kUnpairableDeviceName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kJustWorksPath)) {
    properties->address.ReplaceValue(kJustWorksAddress);
    properties->bluetooth_class.ReplaceValue(kJustWorksClass);
    properties->name.ReplaceValue(kJustWorksName);
    properties->name.set_valid(true);

  } else if (device_path == dbus::ObjectPath(kLowEnergyPath)) {
    properties->address.ReplaceValue(kLowEnergyAddress);
    properties->bluetooth_class.ReplaceValue(kLowEnergyClass);
    properties->name.ReplaceValue(kLowEnergyName);
    properties->name.set_valid(true);
    properties->services_resolved.ReplaceValue(false);
    properties->type.ReplaceValue(BluetoothDeviceClient::kTypeLe);
    properties->uuids.ReplaceValue(std::vector<std::string>(
        {FakeBluetoothGattServiceClient::kHeartRateServiceUUID}));
  } else if (device_path == dbus::ObjectPath(kDualPath)) {
    properties->address.ReplaceValue(kDualAddress);
    properties->name.ReplaceValue(kDualName);
    properties->name.set_valid(true);
    properties->services_resolved.ReplaceValue(false);
    properties->type.ReplaceValue(BluetoothDeviceClient::kTypeDual);
    properties->uuids.ReplaceValue(std::vector<std::string>(
        {FakeBluetoothGattServiceClient::kGenericAccessServiceUUID,
         FakeBluetoothGattServiceClient::kHeartRateServiceUUID}));
  } else if (device_path ==
             dbus::ObjectPath(kConnectedTrustedNotPairedDevicePath)) {
    properties->address.ReplaceValue(kConnectedTrustedNotPairedDeviceAddress);
    properties->bluetooth_class.ReplaceValue(
        kConnectedTrustedNotPairedDeviceClass);
    properties->trusted.ReplaceValue(true);
    properties->connected.ReplaceValue(true);
    properties->paired.ReplaceValue(false);
    properties->name.ReplaceValue(kConnectedTrustedNotPairedDeviceName);
    properties->name.set_valid(true);
  } else {
    NOTREACHED();
  }

  properties_map_.insert(std::make_pair(device_path, std::move(properties)));
  device_list_.push_back(device_path);

  for (auto& observer : observers_)
    observer.DeviceAdded(device_path);
}

void FakeBluetoothDeviceClient::CreateDeviceWithProperties(
    const dbus::ObjectPath& adapter_path,
    const IncomingDeviceProperties& props) {
  dbus::ObjectPath device_path(props.device_path);
  if (base::Contains(device_list_, device_path))
    return;

  std::unique_ptr<Properties> properties(
      new Properties(base::Bind(&FakeBluetoothDeviceClient::OnPropertyChanged,
                                base::Unretained(this), device_path)));
  properties->adapter.ReplaceValue(adapter_path);
  properties->name.ReplaceValue(props.device_name);
  properties->name.set_valid(true);
  properties->alias.ReplaceValue(props.device_alias);
  properties->address.ReplaceValue(props.device_address);
  properties->bluetooth_class.ReplaceValue(props.device_class);
  properties->trusted.ReplaceValue(props.is_trusted);

  if (props.is_trusted)
    properties->paired.ReplaceValue(true);

  std::unique_ptr<SimulatedPairingOptions> options(new SimulatedPairingOptions);
  options->pairing_method = props.pairing_method;
  options->pairing_auth_token = props.pairing_auth_token;
  options->pairing_action = props.pairing_action;
  options->incoming = props.incoming;

  properties_map_.insert(std::make_pair(device_path, std::move(properties)));
  device_list_.push_back(device_path);
  pairing_options_map_.insert(std::make_pair(device_path, std::move(options)));
  for (auto& observer : observers_)
    observer.DeviceAdded(device_path);
}

std::unique_ptr<base::ListValue>
FakeBluetoothDeviceClient::GetBluetoothDevicesAsDictionaries() const {
  std::unique_ptr<base::ListValue> predefined_devices(new base::ListValue);
  std::unique_ptr<base::DictionaryValue> pairedDevice(
      new base::DictionaryValue);
  pairedDevice->SetString("path", kPairedDevicePath);
  pairedDevice->SetString("address", kPairedDeviceAddress);
  pairedDevice->SetString("name", kPairedDeviceName);
  pairedDevice->SetString("alias", kPairedDeviceName);
  pairedDevice->SetString("pairingMethod", "");
  pairedDevice->SetString("pairingAuthToken", "");
  pairedDevice->SetString("pairingAction", "");
  pairedDevice->SetInteger("classValue", kPairedDeviceClass);
  pairedDevice->SetBoolean("discoverable", true);
  pairedDevice->SetBoolean("isTrusted", true);
  pairedDevice->SetBoolean("paired", true);
  pairedDevice->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(pairedDevice));

  std::unique_ptr<base::DictionaryValue> legacyDevice(
      new base::DictionaryValue);
  legacyDevice->SetString("path", kLegacyAutopairPath);
  legacyDevice->SetString("address", kLegacyAutopairAddress);
  legacyDevice->SetString("name", kLegacyAutopairName);
  legacyDevice->SetString("alias", kLegacyAutopairName);
  legacyDevice->SetString("pairingMethod", "");
  legacyDevice->SetString("pairingAuthToken", "");
  legacyDevice->SetString("pairingAction", "");
  legacyDevice->SetInteger("classValue", kLegacyAutopairClass);
  legacyDevice->SetBoolean("isTrusted", true);
  legacyDevice->SetBoolean("discoverable", false);
  legacyDevice->SetBoolean("paired", false);
  legacyDevice->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(legacyDevice));

  std::unique_ptr<base::DictionaryValue> pin(new base::DictionaryValue);
  pin->SetString("path", kDisplayPinCodePath);
  pin->SetString("address", kDisplayPinCodeAddress);
  pin->SetString("name", kDisplayPinCodeName);
  pin->SetString("alias", kDisplayPinCodeName);
  pin->SetString("pairingMethod", kPairingMethodPinCode);
  pin->SetString("pairingAuthToken", kTestPinCode);
  pin->SetString("pairingAction", kPairingActionDisplay);
  pin->SetInteger("classValue", kDisplayPinCodeClass);
  pin->SetBoolean("isTrusted", false);
  pin->SetBoolean("discoverable", false);
  pin->SetBoolean("paired", false);
  pin->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(pin));

  std::unique_ptr<base::DictionaryValue> vanishing(new base::DictionaryValue);
  vanishing->SetString("path", kVanishingDevicePath);
  vanishing->SetString("address", kVanishingDeviceAddress);
  vanishing->SetString("name", kVanishingDeviceName);
  vanishing->SetString("alias", kVanishingDeviceName);
  vanishing->SetString("pairingMethod", "");
  vanishing->SetString("pairingAuthToken", "");
  vanishing->SetString("pairingAction", "");
  vanishing->SetInteger("classValue", kVanishingDeviceClass);
  vanishing->SetBoolean("isTrusted", false);
  vanishing->SetBoolean("discoverable", false);
  vanishing->SetBoolean("paired", false);
  vanishing->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(vanishing));

  std::unique_ptr<base::DictionaryValue> connect_unpairable(
      new base::DictionaryValue);
  connect_unpairable->SetString("path", kConnectUnpairablePath);
  connect_unpairable->SetString("address", kConnectUnpairableAddress);
  connect_unpairable->SetString("name", kConnectUnpairableName);
  connect_unpairable->SetString("pairingMethod", "");
  connect_unpairable->SetString("pairingAuthToken", "");
  connect_unpairable->SetString("pairingAction", "");
  connect_unpairable->SetString("alias", kConnectUnpairableName);
  connect_unpairable->SetInteger("classValue", kConnectUnpairableClass);
  connect_unpairable->SetBoolean("isTrusted", false);
  connect_unpairable->SetBoolean("discoverable", false);
  connect_unpairable->SetBoolean("paired", false);
  connect_unpairable->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(connect_unpairable));

  std::unique_ptr<base::DictionaryValue> passkey(new base::DictionaryValue);
  passkey->SetString("path", kDisplayPasskeyPath);
  passkey->SetString("address", kDisplayPasskeyAddress);
  passkey->SetString("name", kDisplayPasskeyName);
  passkey->SetString("alias", kDisplayPasskeyName);
  passkey->SetString("pairingMethod", kPairingMethodPassKey);
  passkey->SetInteger("pairingAuthToken", kTestPassKey);
  passkey->SetString("pairingAction", kPairingActionDisplay);
  passkey->SetInteger("classValue", kDisplayPasskeyClass);
  passkey->SetBoolean("isTrusted", false);
  passkey->SetBoolean("discoverable", false);
  passkey->SetBoolean("paired", false);
  passkey->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(passkey));

  std::unique_ptr<base::DictionaryValue> request_pin(new base::DictionaryValue);
  request_pin->SetString("path", kRequestPinCodePath);
  request_pin->SetString("address", kRequestPinCodeAddress);
  request_pin->SetString("name", kRequestPinCodeName);
  request_pin->SetString("alias", kRequestPinCodeName);
  request_pin->SetString("pairingMethod", "");
  request_pin->SetString("pairingAuthToken", "");
  request_pin->SetString("pairingAction", kPairingActionRequest);
  request_pin->SetInteger("classValue", kRequestPinCodeClass);
  request_pin->SetBoolean("isTrusted", false);
  request_pin->SetBoolean("discoverable", false);
  request_pin->SetBoolean("paired", false);
  request_pin->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(request_pin));

  std::unique_ptr<base::DictionaryValue> confirm(new base::DictionaryValue);
  confirm->SetString("path", kConfirmPasskeyPath);
  confirm->SetString("address", kConfirmPasskeyAddress);
  confirm->SetString("name", kConfirmPasskeyName);
  confirm->SetString("alias", kConfirmPasskeyName);
  confirm->SetString("pairingMethod", "");
  confirm->SetInteger("pairingAuthToken", kTestPassKey);
  confirm->SetString("pairingAction", kPairingActionConfirmation);
  confirm->SetInteger("classValue", kConfirmPasskeyClass);
  confirm->SetBoolean("isTrusted", false);
  confirm->SetBoolean("discoverable", false);
  confirm->SetBoolean("paired", false);
  confirm->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(confirm));

  std::unique_ptr<base::DictionaryValue> request_passkey(
      new base::DictionaryValue);
  request_passkey->SetString("path", kRequestPasskeyPath);
  request_passkey->SetString("address", kRequestPasskeyAddress);
  request_passkey->SetString("name", kRequestPasskeyName);
  request_passkey->SetString("alias", kRequestPasskeyName);
  request_passkey->SetString("pairingMethod", kPairingMethodPassKey);
  request_passkey->SetString("pairingAction", kPairingActionRequest);
  request_passkey->SetInteger("pairingAuthToken", kTestPassKey);
  request_passkey->SetInteger("classValue", kRequestPasskeyClass);
  request_passkey->SetBoolean("isTrusted", false);
  request_passkey->SetBoolean("discoverable", false);
  request_passkey->SetBoolean("paired", false);
  request_passkey->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(request_passkey));

  std::unique_ptr<base::DictionaryValue> unconnectable(
      new base::DictionaryValue);
  unconnectable->SetString("path", kUnconnectableDevicePath);
  unconnectable->SetString("address", kUnconnectableDeviceAddress);
  unconnectable->SetString("name", kUnconnectableDeviceName);
  unconnectable->SetString("alias", kUnconnectableDeviceName);
  unconnectable->SetString("pairingMethod", "");
  unconnectable->SetString("pairingAuthToken", "");
  unconnectable->SetString("pairingAction", "");
  unconnectable->SetInteger("classValue", kUnconnectableDeviceClass);
  unconnectable->SetBoolean("isTrusted", true);
  unconnectable->SetBoolean("discoverable", false);
  unconnectable->SetBoolean("paired", false);
  unconnectable->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(unconnectable));

  std::unique_ptr<base::DictionaryValue> unpairable(new base::DictionaryValue);
  unpairable->SetString("path", kUnpairableDevicePath);
  unpairable->SetString("address", kUnpairableDeviceAddress);
  unpairable->SetString("name", kUnpairableDeviceName);
  unpairable->SetString("alias", kUnpairableDeviceName);
  unpairable->SetString("pairingMethod", "");
  unpairable->SetString("pairingAuthToken", "");
  unpairable->SetString("pairingAction", kPairingActionFail);
  unpairable->SetInteger("classValue", kUnpairableDeviceClass);
  unpairable->SetBoolean("isTrusted", false);
  unpairable->SetBoolean("discoverable", false);
  unpairable->SetBoolean("paired", false);
  unpairable->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(unpairable));

  std::unique_ptr<base::DictionaryValue> just_works(new base::DictionaryValue);
  just_works->SetString("path", kJustWorksPath);
  just_works->SetString("address", kJustWorksAddress);
  just_works->SetString("name", kJustWorksName);
  just_works->SetString("alias", kJustWorksName);
  just_works->SetString("pairingMethod", "");
  just_works->SetString("pairingAuthToken", "");
  just_works->SetString("pairingAction", "");
  just_works->SetInteger("classValue", kJustWorksClass);
  just_works->SetBoolean("isTrusted", false);
  just_works->SetBoolean("discoverable", false);
  just_works->SetBoolean("paired", false);
  just_works->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(just_works));

  std::unique_ptr<base::DictionaryValue> low_energy(new base::DictionaryValue);
  low_energy->SetString("path", kLowEnergyPath);
  low_energy->SetString("address", kLowEnergyAddress);
  low_energy->SetString("name", kLowEnergyName);
  low_energy->SetString("alias", kLowEnergyName);
  low_energy->SetString("pairingMethod", "");
  low_energy->SetString("pairingAuthToken", "");
  low_energy->SetString("pairingAction", "");
  low_energy->SetInteger("classValue", kLowEnergyClass);
  low_energy->SetBoolean("isTrusted", false);
  low_energy->SetBoolean("discoverable", false);
  low_energy->SetBoolean("paireed", false);
  low_energy->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(low_energy));

  std::unique_ptr<base::DictionaryValue> paired_unconnectable(
      new base::DictionaryValue);
  paired_unconnectable->SetString("path", kPairedUnconnectableDevicePath);
  paired_unconnectable->SetString("address", kPairedUnconnectableDeviceAddress);
  paired_unconnectable->SetString("name", kPairedUnconnectableDeviceName);
  paired_unconnectable->SetString("pairingMethod", "");
  paired_unconnectable->SetString("pairingAuthToken", "");
  paired_unconnectable->SetString("pairingAction", "");
  paired_unconnectable->SetString("alias", kPairedUnconnectableDeviceName);
  paired_unconnectable->SetInteger("classValue",
                                   kPairedUnconnectableDeviceClass);
  paired_unconnectable->SetBoolean("isTrusted", false);
  paired_unconnectable->SetBoolean("discoverable", true);
  paired_unconnectable->SetBoolean("paired", true);
  paired_unconnectable->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(paired_unconnectable));

  std::unique_ptr<base::DictionaryValue> connected_trusted_not_paired(
      new base::DictionaryValue);
  connected_trusted_not_paired->SetString("path",
                                          kConnectedTrustedNotPairedDevicePath);
  connected_trusted_not_paired->SetString(
      "address", kConnectedTrustedNotPairedDeviceAddress);
  connected_trusted_not_paired->SetString("name",
                                          kConnectedTrustedNotPairedDeviceName);
  connected_trusted_not_paired->SetString("pairingMethod", "");
  connected_trusted_not_paired->SetString("pairingAuthToken", "");
  connected_trusted_not_paired->SetString("pairingAction", "");
  connected_trusted_not_paired->SetString("alias",
                                          kConnectedTrustedNotPairedDeviceName);
  connected_trusted_not_paired->SetInteger(
      "classValue", kConnectedTrustedNotPairedDeviceClass);
  connected_trusted_not_paired->SetBoolean("isTrusted", true);
  connected_trusted_not_paired->SetBoolean("discoverable", true);
  connected_trusted_not_paired->SetBoolean("paired", false);
  connected_trusted_not_paired->SetBoolean("incoming", false);
  predefined_devices->Append(std::move(connected_trusted_not_paired));

  return predefined_devices;
}

void FakeBluetoothDeviceClient::RemoveDevice(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  auto listiter =
      std::find(device_list_.begin(), device_list_.end(), device_path);
  if (listiter == device_list_.end())
    return;

  PropertiesMap::const_iterator iter = properties_map_.find(device_path);
  Properties* properties = iter->second.get();

  VLOG(1) << "removing device: " << properties->name.value();
  device_list_.erase(listiter);

  // Remove the Input interface if it exists. This should be called before the
  // BluetoothDeviceClient::Observer::DeviceRemoved because it deletes the
  // BluetoothDeviceBlueZ object, including the device_path referenced here.
  FakeBluetoothInputClient* fake_bluetooth_input_client =
      static_cast<FakeBluetoothInputClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothInputClient());
  fake_bluetooth_input_client->RemoveInputDevice(device_path);

  if (device_path == dbus::ObjectPath(kLowEnergyPath)) {
    FakeBluetoothGattServiceClient* gatt_service_client =
        static_cast<FakeBluetoothGattServiceClient*>(
            bluez::BluezDBusManager::Get()->GetBluetoothGattServiceClient());
    gatt_service_client->HideHeartRateService();
  }

  for (auto& observer : observers_)
    observer.DeviceRemoved(device_path);

  properties_map_.erase(iter);
  PairingOptionsMap::const_iterator options_iter =
      pairing_options_map_.find(device_path);

  if (options_iter != pairing_options_map_.end()) {
    pairing_options_map_.erase(options_iter);
  }
}

void FakeBluetoothDeviceClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  VLOG(2) << "Fake Bluetooth device property changed: " << object_path.value()
          << ": " << property_name;
  for (auto& observer : observers_)
    observer.DevicePropertyChanged(object_path, property_name);
}

void FakeBluetoothDeviceClient::DiscoverySimulationTimer() {
  if (!discovery_simulation_step_)
    return;

  // Timer fires every .75s, the numbers below are arbitrary to give a feel
  // for a discovery process.
  VLOG(1) << "discovery simulation, step " << discovery_simulation_step_;
  uint32_t initial_step = delay_start_discovery_ ? 2 : 1;
  if (discovery_simulation_step_ == initial_step) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kLegacyAutopairPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kLowEnergyPath));
    if (!delay_start_discovery_) {
      // Include a device that requires a pairing overlay in the UI.
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPinCodePath));
    }
  } else if (discovery_simulation_step_ == 4) {
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kDisplayPinCodePath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kVanishingDevicePath));

  } else if (discovery_simulation_step_ == 7) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kConnectUnpairablePath));
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));

  } else if (discovery_simulation_step_ == 8) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kDisplayPasskeyPath));
    if (delay_start_discovery_) {
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPinCodePath));
    }
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
  } else if (discovery_simulation_step_ == 10) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kConfirmPasskeyPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kRequestPasskeyPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kUnconnectableDevicePath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kUnpairableDevicePath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kJustWorksPath));
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));

  } else if (discovery_simulation_step_ == 13) {
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
    RemoveDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kVanishingDevicePath));
  } else if (discovery_simulation_step_ == 14) {
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
    return;
  }

  ++discovery_simulation_step_;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                     base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::IncomingPairingSimulationTimer() {
  if (!incoming_pairing_simulation_step_)
    return;

  VLOG(1) << "incoming pairing simulation, step "
          << incoming_pairing_simulation_step_;
  switch (incoming_pairing_simulation_step_) {
    case 1:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kConfirmPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kConfirmPasskeyPath), true,
                      base::DoNothing(), base::Bind(&SimpleErrorCallback));
      break;
    case 2:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kJustWorksPath));
      SimulatePairing(dbus::ObjectPath(kJustWorksPath), true, base::DoNothing(),
                      base::Bind(&SimpleErrorCallback));
      break;
    case 3:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kDisplayPinCodePath));
      SimulatePairing(dbus::ObjectPath(kDisplayPinCodePath), true,
                      base::DoNothing(), base::Bind(&SimpleErrorCallback));
      break;
    case 4:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kDisplayPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kDisplayPasskeyPath), true,
                      base::DoNothing(), base::Bind(&SimpleErrorCallback));
      break;
    case 5:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPinCodePath));
      SimulatePairing(dbus::ObjectPath(kRequestPinCodePath), true,
                      base::DoNothing(), base::Bind(&SimpleErrorCallback));
      break;
    case 6:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kRequestPasskeyPath), true,
                      base::DoNothing(), base::Bind(&SimpleErrorCallback));
      break;
    default:
      return;
  }

  ++incoming_pairing_simulation_step_;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::IncomingPairingSimulationTimer,
                     base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kIncomingSimulationPairTimeMultiplier *
                                        simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::SimulatePairing(
    const dbus::ObjectPath& object_path,
    bool incoming_request,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  pairing_cancelled_ = false;

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAgentManagerClient());
  FakeBluetoothAgentServiceProvider* agent_service_provider =
      fake_bluetooth_agent_manager_client->GetAgentServiceProvider();
  CHECK(agent_service_provider != NULL);

  // Grab the device's pairing properties.
  PairingOptionsMap::const_iterator iter =
      pairing_options_map_.find(object_path);

  // If the device with path |object_path| has simulated pairing properties
  // defined, then pair it based on its |pairing_method|.
  if (iter != pairing_options_map_.end()) {
    if (iter->second->pairing_action == kPairingActionFail) {
      // Fails the pairing with an org.bluez.Error.Failed error.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::FailSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
    } else if (iter->second->pairing_method == kPairingMethodNone ||
               iter->second->pairing_method.empty()) {
      if (!iter->second->incoming) {
        // Simply pair and connect the device.
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)),
            base::TimeDelta::FromMilliseconds(
                kSimulateNormalPairTimeMultiplier * simulation_interval_ms_));
      } else {
        agent_service_provider->RequestAuthorization(
            object_path,
            base::BindOnce(&FakeBluetoothDeviceClient::ConfirmationCallback,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)));
      }
    } else if (iter->second->pairing_method == kPairingMethodPinCode) {
      if (iter->second->pairing_action == kPairingActionDisplay) {
        // Display a Pincode, and wait before acting as if the other end
        // accepted it.
        agent_service_provider->DisplayPinCode(
            object_path, iter->second->pairing_auth_token);

        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)),
            base::TimeDelta::FromMilliseconds(kPinCodeDevicePairTimeMultiplier *
                                              simulation_interval_ms_));
      } else if (iter->second->pairing_action == kPairingActionRequest) {
        // Request a pin code.
        agent_service_provider->RequestPinCode(
            object_path,
            base::BindOnce(&FakeBluetoothDeviceClient::PinCodeCallback,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)));
      } else if (iter->second->pairing_action == kPairingActionConfirmation) {
        std::move(error_callback)
            .Run(kNoResponseError, "No confirm for pincode pairing.");
      }
    } else if (iter->second->pairing_method == kPairingMethodPassKey) {
      // Display a passkey, and each interval act as if another key was entered
      // for it.
      if (iter->second->pairing_action == kPairingActionDisplay) {
        agent_service_provider->DisplayPasskey(
            object_path, std::stoi(iter->second->pairing_auth_token), 0);

        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::SimulateKeypress,
                           base::Unretained(this), 1, object_path,
                           std::move(callback), std::move(error_callback)),
            base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
      } else if (iter->second->pairing_action == kPairingActionRequest) {
        agent_service_provider->RequestPasskey(
            object_path,
            base::BindOnce(&FakeBluetoothDeviceClient::PasskeyCallback,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)));
      } else if (iter->second->pairing_action == kPairingActionConfirmation) {
        agent_service_provider->RequestConfirmation(
            object_path, std::stoi(iter->second->pairing_auth_token),
            base::BindOnce(&FakeBluetoothDeviceClient::ConfirmationCallback,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)));
      }
    }
  } else {
    if (object_path == dbus::ObjectPath(kLegacyAutopairPath) ||
        object_path == dbus::ObjectPath(kConnectUnpairablePath) ||
        object_path == dbus::ObjectPath(kUnconnectableDevicePath) ||
        object_path == dbus::ObjectPath(kLowEnergyPath)) {
      // No need to call anything on the pairing delegate, just wait 3 times
      // the interval before acting as if the other end accepted it.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(kSimulateNormalPairTimeMultiplier *
                                            simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kDisplayPinCodePath)) {
      // Display a Pincode, and wait before acting as if the other end accepted
      // it.
      agent_service_provider->DisplayPinCode(object_path, kTestPinCode);

      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(kPinCodeDevicePairTimeMultiplier *
                                            simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kVanishingDevicePath)) {
      // The vanishing device simulates being too far away, and thus times out.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::TimeoutSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(kVanishingDevicePairTimeMultiplier *
                                            simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kDisplayPasskeyPath)) {
      // Display a passkey, and each interval act as if another key was entered
      // for it.
      agent_service_provider->DisplayPasskey(object_path, kTestPassKey, 0);

      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::SimulateKeypress,
                         base::Unretained(this), 1, object_path,
                         std::move(callback), std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kRequestPinCodePath)) {
      // Request a Pincode.
      agent_service_provider->RequestPinCode(
          object_path,
          base::BindOnce(&FakeBluetoothDeviceClient::PinCodeCallback,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)));

    } else if (object_path == dbus::ObjectPath(kConfirmPasskeyPath) ||
               object_path ==
                   dbus::ObjectPath(kConnectedTrustedNotPairedDevicePath)) {
      // Request confirmation of a Passkey.
      agent_service_provider->RequestConfirmation(
          object_path, kTestPassKey,
          base::BindOnce(&FakeBluetoothDeviceClient::ConfirmationCallback,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)));

    } else if (object_path == dbus::ObjectPath(kRequestPasskeyPath)) {
      // Request a Passkey from the user.
      agent_service_provider->RequestPasskey(
          object_path,
          base::BindOnce(&FakeBluetoothDeviceClient::PasskeyCallback,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)));

    } else if (object_path == dbus::ObjectPath(kUnpairableDevicePath)) {
      // Fails the pairing with an org.bluez.Error.Failed error.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::FailSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kJustWorksPath)) {
      if (incoming_request) {
        agent_service_provider->RequestAuthorization(
            object_path,
            base::BindOnce(&FakeBluetoothDeviceClient::ConfirmationCallback,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)));

      } else {
        // No need to call anything on the pairing delegate, just wait before
        // acting as if the other end accepted it.
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)),
            base::TimeDelta::FromMilliseconds(
                kSimulateNormalPairTimeMultiplier * simulation_interval_ms_));
      }

    } else {
      std::move(error_callback).Run(kNoResponseError, "No pairing fake");
    }
  }
}

void FakeBluetoothDeviceClient::CompleteSimulatedPairing(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "CompleteSimulatedPairing: " << object_path.value();
  if (pairing_cancelled_) {
    pairing_cancelled_ = false;

    std::move(error_callback)
        .Run(bluetooth_device::kErrorAuthenticationCanceled, "Cancelled");
  } else {
    Properties* properties = GetProperties(object_path);

    properties->paired.ReplaceValue(true);
    std::move(callback).Run();

    AddInputDeviceIfNeeded(object_path, properties);
  }
}

void FakeBluetoothDeviceClient::TimeoutSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  VLOG(1) << "TimeoutSimulatedPairing: " << object_path.value();

  std::move(error_callback)
      .Run(bluetooth_device::kErrorAuthenticationTimeout, "Timed out");
}

void FakeBluetoothDeviceClient::CancelSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  VLOG(1) << "CancelSimulatedPairing: " << object_path.value();

  std::move(error_callback)
      .Run(bluetooth_device::kErrorAuthenticationCanceled, "Canceled");
}

void FakeBluetoothDeviceClient::RejectSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  VLOG(1) << "RejectSimulatedPairing: " << object_path.value();

  std::move(error_callback)
      .Run(bluetooth_device::kErrorAuthenticationRejected, "Rejected");
}

void FakeBluetoothDeviceClient::FailSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  VLOG(1) << "FailSimulatedPairing: " << object_path.value();

  std::move(error_callback).Run(bluetooth_device::kErrorFailed, "Failed");
}

void FakeBluetoothDeviceClient::AddInputDeviceIfNeeded(
    const dbus::ObjectPath& object_path,
    Properties* properties) {
  // If the paired device is a HID device based on it's bluetooth class,
  // simulate the Input interface.
  FakeBluetoothInputClient* fake_bluetooth_input_client =
      static_cast<FakeBluetoothInputClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothInputClient());

  if ((properties->bluetooth_class.value() & 0x001f03) == 0x000500)
    fake_bluetooth_input_client->AddInputDevice(object_path);
}

void FakeBluetoothDeviceClient::InvalidateDeviceRSSI(
    const dbus::ObjectPath& object_path) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter == properties_map_.end()) {
    VLOG(2) << "Fake device does not exist: " << object_path.value();
    return;
  }
  Properties* properties = iter->second.get();
  DCHECK(properties);
  // Invalidate the value and notify that it changed.
  properties->rssi.set_valid(false);
  properties->rssi.ReplaceValue(0);
}

void FakeBluetoothDeviceClient::UpdateDeviceRSSI(
    const dbus::ObjectPath& object_path,
    int16_t rssi) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter == properties_map_.end()) {
    VLOG(2) << "Fake device does not exist: " << object_path.value();
    return;
  }
  Properties* properties = iter->second.get();
  DCHECK(properties);
  properties->rssi.set_valid(true);
  properties->rssi.ReplaceValue(rssi);
}

void FakeBluetoothDeviceClient::UpdateServiceAndManufacturerData(
    const dbus::ObjectPath& object_path,
    const std::vector<std::string>& service_uuids,
    const std::map<std::string, std::vector<uint8_t>>& service_data,
    const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter == properties_map_.end()) {
    VLOG(2) << "Fake device does not exist: " << object_path.value();
    return;
  }
  Properties* properties = iter->second.get();
  DCHECK(properties);
  properties->uuids.set_valid(true);
  properties->service_data.set_valid(true);
  properties->manufacturer_data.set_valid(true);

  // BlueZ caches all the previously received advertisements. To mimic BlueZ
  // caching behavior, merge the new data here with the existing data.
  // TODO(crbug.com/707039): once the BlueZ caching behavior is changed, this
  // needs to be updated as well.

  std::vector<std::string> merged_uuids = service_uuids;
  merged_uuids.insert(merged_uuids.begin(), properties->uuids.value().begin(),
                      properties->uuids.value().end());
  properties->uuids.ReplaceValue(merged_uuids);

  std::map<std::string, std::vector<uint8_t>> merged_service_data =
      service_data;
  merged_service_data.insert(properties->service_data.value().begin(),
                             properties->service_data.value().end());
  properties->service_data.ReplaceValue(merged_service_data);

  std::map<uint16_t, std::vector<uint8_t>> merged_manufacturer_data =
      manufacturer_data;
  merged_manufacturer_data.insert(properties->manufacturer_data.value().begin(),
                                  properties->manufacturer_data.value().end());
  properties->manufacturer_data.ReplaceValue(merged_manufacturer_data);
}

void FakeBluetoothDeviceClient::UpdateEIR(const dbus::ObjectPath& object_path,
                                          const std::vector<uint8_t>& eir) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter == properties_map_.end()) {
    VLOG(2) << "Fake device does not exist: " << object_path.value();
    return;
  }
  Properties* properties = iter->second.get();
  DCHECK(properties);
  properties->eir.set_valid(true);
  properties->eir.ReplaceValue(eir);
}

void FakeBluetoothDeviceClient::UpdateConnectionInfo(
    uint16_t connection_rssi,
    uint16_t transmit_power,
    uint16_t max_transmit_power) {
  connection_rssi_ = connection_rssi;
  transmit_power_ = transmit_power;
  max_transmit_power_ = max_transmit_power;
}

void FakeBluetoothDeviceClient::PinCodeCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status,
    const std::string& pincode) {
  VLOG(1) << "PinCodeCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    PairingOptionsMap::const_iterator iter =
        pairing_options_map_.find(object_path);

    bool success = true;

    // If the device has pairing options defined
    if (iter != pairing_options_map_.end()) {
      success = iter->second->pairing_auth_token == pincode;
    }

    if (success) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(kSimulateNormalPairTimeMultiplier *
                                            simulation_interval_ms_));
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
    }

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::PasskeyCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status,
    uint32_t passkey) {
  VLOG(1) << "PasskeyCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    PairingOptionsMap::const_iterator iter =
        pairing_options_map_.find(object_path);
    bool success = true;

    if (iter != pairing_options_map_.end()) {
      success = static_cast<uint32_t>(
                    std::stoi(iter->second->pairing_auth_token)) == passkey;
    }

    if (success) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(kSimulateNormalPairTimeMultiplier *
                                            simulation_interval_ms_));
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
    }

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::ConfirmationCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status) {
  VLOG(1) << "ConfirmationCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                       base::Unretained(this), object_path, std::move(callback),
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(kSimulateNormalPairTimeMultiplier *
                                          simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::SimulateKeypress(
    uint16_t entered,
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  VLOG(1) << "SimulateKeypress " << entered << ": " << object_path.value();

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAgentManagerClient());
  FakeBluetoothAgentServiceProvider* agent_service_provider =
      fake_bluetooth_agent_manager_client->GetAgentServiceProvider();

  // The agent service provider object could have been destroyed after the
  // pairing is canceled.
  if (!agent_service_provider)
    return;

  agent_service_provider->DisplayPasskey(object_path, kTestPassKey, entered);

  if (entered < 7) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::SimulateKeypress,
                       base::Unretained(this), entered + 1, object_path,
                       std::move(callback), std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                       base::Unretained(this), object_path, std::move(callback),
                       std::move(error_callback)),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::ConnectionCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothProfileServiceProvider::Delegate::Status status) {
  VLOG(1) << "ConnectionCallback: " << object_path.value();

  if (status == BluetoothProfileServiceProvider::Delegate::SUCCESS) {
    std::move(callback).Run();
  } else if (status == BluetoothProfileServiceProvider::Delegate::CANCELLED) {
    // TODO(keybuk): tear down this side of the connection
    std::move(error_callback).Run(bluetooth_device::kErrorFailed, "Canceled");
  } else if (status == BluetoothProfileServiceProvider::Delegate::REJECTED) {
    // TODO(keybuk): tear down this side of the connection
    std::move(error_callback).Run(bluetooth_device::kErrorFailed, "Rejected");
  }
}

void FakeBluetoothDeviceClient::DisconnectionCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothProfileServiceProvider::Delegate::Status status) {
  VLOG(1) << "DisconnectionCallback: " << object_path.value();

  if (status == BluetoothProfileServiceProvider::Delegate::SUCCESS) {
    // TODO(keybuk): tear down this side of the connection
    std::move(callback).Run();
  } else if (status == BluetoothProfileServiceProvider::Delegate::CANCELLED) {
    std::move(error_callback).Run(bluetooth_device::kErrorFailed, "Canceled");
  } else if (status == BluetoothProfileServiceProvider::Delegate::REJECTED) {
    std::move(error_callback).Run(bluetooth_device::kErrorFailed, "Rejected");
  }
}

void FakeBluetoothDeviceClient::RemoveAllDevices() {
  device_list_.clear();
}

void FakeBluetoothDeviceClient::CreateTestDevice(
    const dbus::ObjectPath& adapter_path,
    const base::Optional<std::string> name,
    const std::string alias,
    const std::string device_address,
    const std::vector<std::string>& service_uuids,
    device::BluetoothTransport type,
    const std::map<std::string, std::vector<uint8_t>>& service_data,
    const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data) {
  // Create a random device path.
  dbus::ObjectPath device_path;
  std::string id;
  do {
    // Construct an id that is valid according to the DBUS specification.
    base::Base64Encode(base::RandBytesAsString(10), &id);
    base::RemoveChars(id, "+/=", &id);
    device_path = dbus::ObjectPath(adapter_path.value() + "/dev" + id);
  } while (base::Contains(device_list_, device_path));

  std::unique_ptr<Properties> properties(
      new Properties(base::Bind(&FakeBluetoothDeviceClient::OnPropertyChanged,
                                base::Unretained(this), device_path)));
  properties->adapter.ReplaceValue(adapter_path);

  properties->address.ReplaceValue(device_address);
  properties->name.ReplaceValue(
      name.value_or("Invalid Device Name set in "
                    "FakeBluetoothDeviceClient::CreateTestDevice"));
  properties->name.set_valid(name.has_value());
  properties->alias.ReplaceValue(alias);

  properties->uuids.ReplaceValue(service_uuids);
  properties->bluetooth_class.ReplaceValue(
      0x1F00u);  // Unspecified Device Class

  switch (type) {
    case device::BLUETOOTH_TRANSPORT_CLASSIC:
      properties->type.ReplaceValue(BluetoothDeviceClient::kTypeBredr);
      break;
    case device::BLUETOOTH_TRANSPORT_LE:
      properties->type.ReplaceValue(BluetoothDeviceClient::kTypeLe);
      break;
    case device::BLUETOOTH_TRANSPORT_DUAL:
      properties->type.ReplaceValue(BluetoothDeviceClient::kTypeDual);
      break;
    default:
      NOTREACHED();
  }
  properties->type.set_valid(true);

  if (!service_data.empty()) {
    properties->service_data.ReplaceValue(service_data);
    properties->service_data.set_valid(true);
  }

  if (!manufacturer_data.empty()) {
    properties->manufacturer_data.ReplaceValue(manufacturer_data);
    properties->manufacturer_data.set_valid(true);
  }

  properties_map_.insert(std::make_pair(device_path, std::move(properties)));
  device_list_.push_back(device_path);
  for (auto& observer : observers_)
    observer.DeviceAdded(device_path);
}

void FakeBluetoothDeviceClient::AddPrepareWriteRequest(
    const dbus::ObjectPath& object_path,
    const std::vector<uint8_t>& value) {
  prepare_write_requests_.emplace_back(object_path, value);
}

}  // namespace bluez

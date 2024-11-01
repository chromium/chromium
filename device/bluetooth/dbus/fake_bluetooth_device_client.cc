// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
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
  DVLOG(1) << "Bluetooth Error: " << error_name << ": " << error_message;
}

BluetoothDeviceClient::ServiceRecordList CreateFakeServiceRecords() {
  BluetoothDeviceClient::ServiceRecordList records;

  std::unique_ptr<BluetoothServiceRecordBlueZ> record1 =
      std::make_unique<BluetoothServiceRecordBlueZ>();
  // ID 0 = handle.
  record1->AddRecordEntry(0x0, BluetoothServiceAttributeValueBlueZ(
                                   BluetoothServiceAttributeValueBlueZ::UINT, 4,
                                   base::Value(static_cast<int32_t>(0x1337))));
  // ID 1 = service class id list.
  std::unique_ptr<BluetoothServiceAttributeValueBlueZ::Sequence> class_id_list =
      std::make_unique<BluetoothServiceAttributeValueBlueZ::Sequence>();
  class_id_list->emplace_back(BluetoothServiceAttributeValueBlueZ::UUID, 4,
                              base::Value("1802"));
  record1->AddRecordEntry(
      0x1, BluetoothServiceAttributeValueBlueZ(std::move(class_id_list)));
  records.emplace_back(*record1);

  std::unique_ptr<BluetoothServiceRecordBlueZ> record2 =
      std::make_unique<BluetoothServiceRecordBlueZ>();
  // ID 0 = handle.
  record2->AddRecordEntry(0x0,
                          BluetoothServiceAttributeValueBlueZ(
                              BluetoothServiceAttributeValueBlueZ::UINT, 4,
                              base::Value(static_cast<int32_t>(0xffffffff))));
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
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothDeviceClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothDeviceClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
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
  auto properties = std::make_unique<Properties>(base::BindRepeating(
      &FakeBluetoothDeviceClient::OnPropertyChanged, base::Unretained(this),
      dbus::ObjectPath(kPairedDevicePath)));
  properties->address.ReplaceValue(kPairedDeviceAddress);
  properties->bluetooth_class.ReplaceValue(
      static_cast<int>(kPairedDeviceClass));
  properties->name.ReplaceValue(kPairedDeviceName);
  properties->name.set_valid(true);
  properties->alias.ReplaceValue(kPairedDeviceAlias);
  properties->paired.ReplaceValue(true);
  properties->bonded.ReplaceValue(true);
#if BUILDFLAG(IS_CHROMEOS)
  properties->trusted.ReplaceValue(true);
#else
  properties->trusted.ReplaceValue(false);
#endif
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

  properties = std::make_unique<Properties>(base::BindRepeating(
      &FakeBluetoothDeviceClient::OnPropertyChanged, base::Unretained(this),
      dbus::ObjectPath(kPairedUnconnectableDevicePath)));
  properties->address.ReplaceValue(kPairedUnconnectableDeviceAddress);
  properties->bluetooth_class.ReplaceValue(kPairedUnconnectableDeviceClass);
  properties->name.ReplaceValue(kPairedUnconnectableDeviceName);
  properties->name.set_valid(true);
  properties->alias.ReplaceValue(kPairedUnconnectableDeviceAlias);
  properties->paired.ReplaceValue(true);
  properties->bonded.ReplaceValue(true);
#if BUILDFLAG(IS_CHROMEOS)
  properties->trusted.ReplaceValue(true);
#else
  properties->trusted.ReplaceValue(false);
#endif
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
  DVLOG(1) << "Connect: " << object_path.value();
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
  if (object_path == dbus::ObjectPath(kLowEnergyPath))
    properties->connected_le.ReplaceValue(true);

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

void FakeBluetoothDeviceClient::ConnectClassic(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  Connect(object_path, std::move(callback), std::move(error_callback));
}

void FakeBluetoothDeviceClient::ConnectLE(const dbus::ObjectPath& object_path,
                                          base::OnceClosure callback,
                                          ErrorCallback error_callback) {
  Connect(object_path, std::move(callback), std::move(error_callback));
}

void FakeBluetoothDeviceClient::DisconnectLE(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  Disconnect(object_path, std::move(callback), std::move(error_callback));
}

void FakeBluetoothDeviceClient::Disconnect(const dbus::ObjectPath& object_path,
                                           base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  DVLOG(1) << "Disconnect: " << object_path.value();
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
  properties->connected_le.ReplaceValue(false);
}

void FakeBluetoothDeviceClient::ConnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "ConnectProfile: " << object_path.value() << " " << uuid;

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

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
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
  DVLOG(1) << "DisconnectProfile: " << object_path.value() << " " << uuid;

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
  DVLOG(1) << "Pair: " << object_path.value();
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
  DVLOG(1) << "CancelPairing: " << object_path.value();
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
                     bluetooth_gatt_characteristic::kTypeRequest,
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
  DVLOG(1) << "starting discovery simulation";

  discovery_simulation_step_ = 1;
  int delay = delay_start_discovery_ ? simulation_interval_ms_ : 0;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                     base::Unretained(this)),
      base::Milliseconds(delay));
}

void FakeBluetoothDeviceClient::EndDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  DVLOG(1) << "stopping discovery simulation";
  discovery_simulation_step_ = 0;
  InvalidateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath));
}

void FakeBluetoothDeviceClient::BeginIncomingPairingSimulation(
    const dbus::ObjectPath& adapter_path) {
  DVLOG(1) << "starting incoming pairing simulation";

  incoming_pairing_simulation_step_ = 1;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::IncomingPairingSimulationTimer,
                     base::Unretained(this)),
      base::Milliseconds(kIncomingSimulationStartPairTimeMultiplier *
                         simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::EndIncomingPairingSimulation(
    const dbus::ObjectPath& adapter_path) {
  DVLOG(1) << "stopping incoming pairing simulation";
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

  std::unique_ptr<Properties> properties(new Properties(
      base::BindRepeating(&FakeBluetoothDeviceClient::OnPropertyChanged,
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
    std::vector<uint8_t> eir = {0x0a, 0x0b, 0x0c};
    properties->eir.ReplaceValue(eir);
    properties->eir.set_valid(true);
    properties->rssi.ReplaceValue(-45);
    properties->rssi.set_valid(true);
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
#if BUILDFLAG(IS_CHROMEOS)
    properties->trusted.ReplaceValue(true);
#else
    properties->trusted.ReplaceValue(false);
#endif
    properties->connected.ReplaceValue(true);
    properties->connected_le.ReplaceValue(true);
    properties->paired.ReplaceValue(false);
    properties->bonded.ReplaceValue(false);
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

  std::unique_ptr<Properties> properties(new Properties(
      base::BindRepeating(&FakeBluetoothDeviceClient::OnPropertyChanged,
                          base::Unretained(this), device_path)));
  properties->adapter.ReplaceValue(adapter_path);
  properties->name.ReplaceValue(props.device_name);
  properties->name.set_valid(true);
  properties->alias.ReplaceValue(props.device_alias);
  properties->address.ReplaceValue(props.device_address);
  properties->bluetooth_class.ReplaceValue(props.device_class);
  properties->trusted.ReplaceValue(props.is_trusted);

  if (props.is_trusted) {
    properties->paired.ReplaceValue(true);
    properties->bonded.ReplaceValue(true);
  }

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

base::Value FakeBluetoothDeviceClient::GetBluetoothDevicesAsDictionaries()
    const {
  base::Value::List predefined_devices;

  base::Value::Dict paired_device;
  paired_device.Set("path", kPairedDevicePath);
  paired_device.Set("address", kPairedDeviceAddress);
  paired_device.Set("name", kPairedDeviceName);
  paired_device.Set("alias", kPairedDeviceName);
  paired_device.Set("pairingMethod", "");
  paired_device.Set("pairingAuthToken", "");
  paired_device.Set("pairingAction", "");
  paired_device.Set("classValue", static_cast<int>(kPairedDeviceClass));
  paired_device.Set("discoverable", true);
  paired_device.Set("isTrusted", true);
  paired_device.Set("paired", true);
  paired_device.Set("incoming", false);
  predefined_devices.Append(std::move(paired_device));

  base::Value::Dict legacy_device;
  legacy_device.Set("path", kLegacyAutopairPath);
  legacy_device.Set("address", kLegacyAutopairAddress);
  legacy_device.Set("name", kLegacyAutopairName);
  legacy_device.Set("alias", kLegacyAutopairName);
  legacy_device.Set("pairingMethod", "");
  legacy_device.Set("pairingAuthToken", "");
  legacy_device.Set("pairingAction", "");
  legacy_device.Set("classValue", static_cast<int>(kLegacyAutopairClass));
  legacy_device.Set("isTrusted", true);
  legacy_device.Set("discoverable", false);
  legacy_device.Set("paired", false);
  legacy_device.Set("incoming", false);
  predefined_devices.Append(std::move(legacy_device));

  base::Value::Dict pin;
  pin.Set("path", kDisplayPinCodePath);
  pin.Set("address", kDisplayPinCodeAddress);
  pin.Set("name", kDisplayPinCodeName);
  pin.Set("alias", kDisplayPinCodeName);
  pin.Set("pairingMethod", kPairingMethodPinCode);
  pin.Set("pairingAuthToken", kTestPinCode);
  pin.Set("pairingAction", kPairingActionDisplay);
  pin.Set("classValue", static_cast<int>(kDisplayPinCodeClass));
  pin.Set("isTrusted", false);
  pin.Set("discoverable", false);
  pin.Set("paired", false);
  pin.Set("incoming", false);
  predefined_devices.Append(std::move(pin));

  base::Value::Dict vanishing;
  vanishing.Set("path", kVanishingDevicePath);
  vanishing.Set("address", kVanishingDeviceAddress);
  vanishing.Set("name", kVanishingDeviceName);
  vanishing.Set("alias", kVanishingDeviceName);
  vanishing.Set("pairingMethod", "");
  vanishing.Set("pairingAuthToken", "");
  vanishing.Set("pairingAction", "");
  vanishing.Set("classValue", static_cast<int>(kVanishingDeviceClass));
  vanishing.Set("isTrusted", false);
  vanishing.Set("discoverable", false);
  vanishing.Set("paired", false);
  vanishing.Set("incoming", false);
  predefined_devices.Append(std::move(vanishing));

  base::Value::Dict connect_unpairable;
  connect_unpairable.Set("path", kConnectUnpairablePath);
  connect_unpairable.Set("address", kConnectUnpairableAddress);
  connect_unpairable.Set("name", kConnectUnpairableName);
  connect_unpairable.Set("pairingMethod", "");
  connect_unpairable.Set("pairingAuthToken", "");
  connect_unpairable.Set("pairingAction", "");
  connect_unpairable.Set("alias", kConnectUnpairableName);
  connect_unpairable.Set("classValue",
                         static_cast<int>(kConnectUnpairableClass));
  connect_unpairable.Set("isTrusted", false);
  connect_unpairable.Set("discoverable", false);
  connect_unpairable.Set("paired", false);
  connect_unpairable.Set("incoming", false);
  predefined_devices.Append(std::move(connect_unpairable));

  base::Value::Dict passkey;
  passkey.Set("path", kDisplayPasskeyPath);
  passkey.Set("address", kDisplayPasskeyAddress);
  passkey.Set("name", kDisplayPasskeyName);
  passkey.Set("alias", kDisplayPasskeyName);
  passkey.Set("pairingMethod", kPairingMethodPassKey);
  passkey.Set("pairingAuthToken", kTestPassKey);
  passkey.Set("pairingAction", kPairingActionDisplay);
  passkey.Set("classValue", static_cast<int>(kDisplayPasskeyClass));
  passkey.Set("isTrusted", false);
  passkey.Set("discoverable", false);
  passkey.Set("paired", false);
  passkey.Set("incoming", false);
  predefined_devices.Append(std::move(passkey));

  base::Value::Dict request_pin;
  request_pin.Set("path", kRequestPinCodePath);
  request_pin.Set("address", kRequestPinCodeAddress);
  request_pin.Set("name", kRequestPinCodeName);
  request_pin.Set("alias", kRequestPinCodeName);
  request_pin.Set("pairingMethod", "");
  request_pin.Set("pairingAuthToken", "");
  request_pin.Set("pairingAction", kPairingActionRequest);
  request_pin.Set("classValue", static_cast<int>(kRequestPinCodeClass));
  request_pin.Set("isTrusted", false);
  request_pin.Set("discoverable", false);
  request_pin.Set("paired", false);
  request_pin.Set("incoming", false);
  predefined_devices.Append(std::move(request_pin));

  base::Value::Dict confirm;
  confirm.Set("path", kConfirmPasskeyPath);
  confirm.Set("address", kConfirmPasskeyAddress);
  confirm.Set("name", kConfirmPasskeyName);
  confirm.Set("alias", kConfirmPasskeyName);
  confirm.Set("pairingMethod", "");
  confirm.Set("pairingAuthToken", kTestPassKey);
  confirm.Set("pairingAction", kPairingActionConfirmation);
  confirm.Set("classValue", static_cast<int>(kConfirmPasskeyClass));
  confirm.Set("isTrusted", false);
  confirm.Set("discoverable", false);
  confirm.Set("paired", false);
  confirm.Set("incoming", false);
  predefined_devices.Append(std::move(confirm));

  base::Value::Dict request_passkey;
  request_passkey.Set("path", kRequestPasskeyPath);
  request_passkey.Set("address", kRequestPasskeyAddress);
  request_passkey.Set("name", kRequestPasskeyName);
  request_passkey.Set("alias", kRequestPasskeyName);
  request_passkey.Set("pairingMethod", kPairingMethodPassKey);
  request_passkey.Set("pairingAction", kPairingActionRequest);
  request_passkey.Set("pairingAuthToken", kTestPassKey);
  request_passkey.Set("classValue", static_cast<int>(kRequestPasskeyClass));
  request_passkey.Set("isTrusted", false);
  request_passkey.Set("discoverable", false);
  request_passkey.Set("paired", false);
  request_passkey.Set("incoming", false);
  predefined_devices.Append(std::move(request_passkey));

  base::Value::Dict unconnectable;
  unconnectable.Set("path", kUnconnectableDevicePath);
  unconnectable.Set("address", kUnconnectableDeviceAddress);
  unconnectable.Set("name", kUnconnectableDeviceName);
  unconnectable.Set("alias", kUnconnectableDeviceName);
  unconnectable.Set("pairingMethod", "");
  unconnectable.Set("pairingAuthToken", "");
  unconnectable.Set("pairingAction", "");
  unconnectable.Set("classValue", static_cast<int>(kUnconnectableDeviceClass));
  unconnectable.Set("isTrusted", true);
  unconnectable.Set("discoverable", false);
  unconnectable.Set("paired", false);
  unconnectable.Set("incoming", false);
  predefined_devices.Append(std::move(unconnectable));

  base::Value::Dict unpairable;
  unpairable.Set("path", kUnpairableDevicePath);
  unpairable.Set("address", kUnpairableDeviceAddress);
  unpairable.Set("name", kUnpairableDeviceName);
  unpairable.Set("alias", kUnpairableDeviceName);
  unpairable.Set("pairingMethod", "");
  unpairable.Set("pairingAuthToken", "");
  unpairable.Set("pairingAction", kPairingActionFail);
  unpairable.Set("classValue", static_cast<int>(kUnpairableDeviceClass));
  unpairable.Set("isTrusted", false);
  unpairable.Set("discoverable", false);
  unpairable.Set("paired", false);
  unpairable.Set("incoming", false);
  predefined_devices.Append(std::move(unpairable));

  base::Value::Dict just_works;
  just_works.Set("path", kJustWorksPath);
  just_works.Set("address", kJustWorksAddress);
  just_works.Set("name", kJustWorksName);
  just_works.Set("alias", kJustWorksName);
  just_works.Set("pairingMethod", "");
  just_works.Set("pairingAuthToken", "");
  just_works.Set("pairingAction", "");
  just_works.Set("classValue", static_cast<int>(kJustWorksClass));
  just_works.Set("isTrusted", false);
  just_works.Set("discoverable", false);
  just_works.Set("paired", false);
  just_works.Set("incoming", false);
  predefined_devices.Append(std::move(just_works));

  base::Value::Dict low_energy;
  low_energy.Set("path", kLowEnergyPath);
  low_energy.Set("address", kLowEnergyAddress);
  low_energy.Set("name", kLowEnergyName);
  low_energy.Set("alias", kLowEnergyName);
  low_energy.Set("pairingMethod", "");
  low_energy.Set("pairingAuthToken", "");
  low_energy.Set("pairingAction", "");
  low_energy.Set("classValue", static_cast<int>(kLowEnergyClass));
  low_energy.Set("isTrusted", false);
  low_energy.Set("discoverable", false);
  low_energy.Set("paireed", false);
  low_energy.Set("incoming", false);
  predefined_devices.Append(std::move(low_energy));

  base::Value::Dict paired_unconnectable;
  paired_unconnectable.Set("path", kPairedUnconnectableDevicePath);
  paired_unconnectable.Set("address", kPairedUnconnectableDeviceAddress);
  paired_unconnectable.Set("name", kPairedUnconnectableDeviceName);
  paired_unconnectable.Set("pairingMethod", "");
  paired_unconnectable.Set("pairingAuthToken", "");
  paired_unconnectable.Set("pairingAction", "");
  paired_unconnectable.Set("alias", kPairedUnconnectableDeviceName);
  paired_unconnectable.Set("classValue",
                           static_cast<int>(kPairedUnconnectableDeviceClass));
  paired_unconnectable.Set("isTrusted", false);
  paired_unconnectable.Set("discoverable", true);
  paired_unconnectable.Set("paired", true);
  paired_unconnectable.Set("incoming", false);
  predefined_devices.Append(std::move(paired_unconnectable));

  base::Value::Dict connected_trusted_not_paired;
  connected_trusted_not_paired.Set("path",
                                   kConnectedTrustedNotPairedDevicePath);
  connected_trusted_not_paired.Set("address",
                                   kConnectedTrustedNotPairedDeviceAddress);
  connected_trusted_not_paired.Set("name",
                                   kConnectedTrustedNotPairedDeviceName);
  connected_trusted_not_paired.Set("pairingMethod", "");
  connected_trusted_not_paired.Set("pairingAuthToken", "");
  connected_trusted_not_paired.Set("pairingAction", "");
  connected_trusted_not_paired.Set("alias",
                                   kConnectedTrustedNotPairedDeviceName);
  connected_trusted_not_paired.Set(
      "classValue", static_cast<int>(kConnectedTrustedNotPairedDeviceClass));
  connected_trusted_not_paired.Set("isTrusted", true);
  connected_trusted_not_paired.Set("discoverable", true);
  connected_trusted_not_paired.Set("paired", false);
  connected_trusted_not_paired.Set("incoming", false);
  predefined_devices.Append(std::move(connected_trusted_not_paired));

  return base::Value(std::move(predefined_devices));
}

void FakeBluetoothDeviceClient::RemoveDevice(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  auto listiter = base::ranges::find(device_list_, device_path);
  if (listiter == device_list_.end())
    return;

  PropertiesMap::const_iterator iter = properties_map_.find(device_path);
  Properties* properties = iter->second.get();

  DVLOG(1) << "removing device: " << properties->name.value();
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
  DVLOG(2) << "Fake Bluetooth device property changed: " << object_path.value()
           << ": " << property_name;
  for (auto& observer : observers_)
    observer.DevicePropertyChanged(object_path, property_name);
}

void FakeBluetoothDeviceClient::DiscoverySimulationTimer() {
  if (!discovery_simulation_step_)
    return;

  // Timer fires every .75s, the numbers below are arbitrary to give a feel
  // for a discovery process.
  DVLOG(1) << "discovery simulation, step " << discovery_simulation_step_;
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                     base::Unretained(this)),
      base::Milliseconds(simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::IncomingPairingSimulationTimer() {
  if (!incoming_pairing_simulation_step_)
    return;

  DVLOG(1) << "incoming pairing simulation, step "
           << incoming_pairing_simulation_step_;
  switch (incoming_pairing_simulation_step_) {
    case 1:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kConfirmPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kConfirmPasskeyPath), true,
                      base::DoNothing(), base::BindOnce(&SimpleErrorCallback));
      break;
    case 2:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kJustWorksPath));
      SimulatePairing(dbus::ObjectPath(kJustWorksPath), true, base::DoNothing(),
                      base::BindOnce(&SimpleErrorCallback));
      break;
    case 3:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kDisplayPinCodePath));
      SimulatePairing(dbus::ObjectPath(kDisplayPinCodePath), true,
                      base::DoNothing(), base::BindOnce(&SimpleErrorCallback));
      break;
    case 4:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kDisplayPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kDisplayPasskeyPath), true,
                      base::DoNothing(), base::BindOnce(&SimpleErrorCallback));
      break;
    case 5:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPinCodePath));
      SimulatePairing(dbus::ObjectPath(kRequestPinCodePath), true,
                      base::DoNothing(), base::BindOnce(&SimpleErrorCallback));
      break;
    case 6:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kRequestPasskeyPath), true,
                      base::DoNothing(), base::BindOnce(&SimpleErrorCallback));
      break;
    default:
      return;
  }

  ++incoming_pairing_simulation_step_;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeBluetoothDeviceClient::IncomingPairingSimulationTimer,
                     base::Unretained(this)),
      base::Milliseconds(kIncomingSimulationPairTimeMultiplier *
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
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::FailSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::Milliseconds(simulation_interval_ms_));
    } else if (iter->second->pairing_method == kPairingMethodNone ||
               iter->second->pairing_method.empty()) {
      if (!iter->second->incoming) {
        // Simply pair and connect the device.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)),
            base::Milliseconds(kSimulateNormalPairTimeMultiplier *
                               simulation_interval_ms_));
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

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)),
            base::Milliseconds(kPinCodeDevicePairTimeMultiplier *
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

        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::SimulateKeypress,
                           base::Unretained(this), 1, object_path,
                           std::move(callback), std::move(error_callback)),
            base::Milliseconds(simulation_interval_ms_));
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
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::Milliseconds(kSimulateNormalPairTimeMultiplier *
                             simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kDisplayPinCodePath)) {
      // Display a Pincode, and wait before acting as if the other end accepted
      // it.
      agent_service_provider->DisplayPinCode(object_path, kTestPinCode);

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::Milliseconds(kPinCodeDevicePairTimeMultiplier *
                             simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kVanishingDevicePath)) {
      // The vanishing device simulates being too far away, and thus times out.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::TimeoutSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::Milliseconds(kVanishingDevicePairTimeMultiplier *
                             simulation_interval_ms_));

    } else if (object_path == dbus::ObjectPath(kDisplayPasskeyPath)) {
      // Display a passkey, and each interval act as if another key was entered
      // for it.
      agent_service_provider->DisplayPasskey(object_path, kTestPassKey, 0);

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::SimulateKeypress,
                         base::Unretained(this), 1, object_path,
                         std::move(callback), std::move(error_callback)),
          base::Milliseconds(simulation_interval_ms_));

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
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::FailSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::Milliseconds(simulation_interval_ms_));

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
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                           base::Unretained(this), object_path,
                           std::move(callback), std::move(error_callback)),
            base::Milliseconds(kSimulateNormalPairTimeMultiplier *
                               simulation_interval_ms_));
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
  DVLOG(1) << "CompleteSimulatedPairing: " << object_path.value();
  if (pairing_cancelled_) {
    pairing_cancelled_ = false;

    std::move(error_callback)
        .Run(bluetooth_device::kErrorAuthenticationCanceled, "Cancelled");
  } else {
    Properties* properties = GetProperties(object_path);

    properties->paired.ReplaceValue(true);
#if BUILDFLAG(IS_CHROMEOS)
    properties->bonded.ReplaceValue(true);
#endif
    std::move(callback).Run();

    AddInputDeviceIfNeeded(object_path, properties);
  }
}

void FakeBluetoothDeviceClient::TimeoutSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  DVLOG(1) << "TimeoutSimulatedPairing: " << object_path.value();

  std::move(error_callback)
      .Run(bluetooth_device::kErrorAuthenticationTimeout, "Timed out");
}

void FakeBluetoothDeviceClient::CancelSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  DVLOG(1) << "CancelSimulatedPairing: " << object_path.value();

  std::move(error_callback)
      .Run(bluetooth_device::kErrorAuthenticationCanceled, "Canceled");
}

void FakeBluetoothDeviceClient::RejectSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  DVLOG(1) << "RejectSimulatedPairing: " << object_path.value();

  std::move(error_callback)
      .Run(bluetooth_device::kErrorAuthenticationRejected, "Rejected");
}

void FakeBluetoothDeviceClient::FailSimulatedPairing(
    const dbus::ObjectPath& object_path,
    ErrorCallback error_callback) {
  DVLOG(1) << "FailSimulatedPairing: " << object_path.value();

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
    DVLOG(2) << "Fake device does not exist: " << object_path.value();
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
    DVLOG(2) << "Fake device does not exist: " << object_path.value();
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
    DVLOG(2) << "Fake device does not exist: " << object_path.value();
    return;
  }
  Properties* properties = iter->second.get();
  DCHECK(properties);
  properties->uuids.set_valid(true);
  properties->service_data.set_valid(true);
  properties->manufacturer_data.set_valid(true);

  // BlueZ caches all the previously received advertisements. To mimic BlueZ
  // caching behavior, merge the new data here with the existing data.
  // TODO(crbug.com/41310506): once the BlueZ caching behavior is changed, this
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
    DVLOG(2) << "Fake device does not exist: " << object_path.value();
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
  DVLOG(1) << "PinCodeCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    PairingOptionsMap::const_iterator iter =
        pairing_options_map_.find(object_path);

    bool success = true;

    // If the device has pairing options defined
    if (iter != pairing_options_map_.end()) {
      success = iter->second->pairing_auth_token == pincode;
    }

    if (success) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::Milliseconds(kSimulateNormalPairTimeMultiplier *
                             simulation_interval_ms_));
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::Milliseconds(simulation_interval_ms_));
    }

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::PasskeyCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status,
    uint32_t passkey) {
  DVLOG(1) << "PasskeyCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    PairingOptionsMap::const_iterator iter =
        pairing_options_map_.find(object_path);
    bool success = true;

    if (iter != pairing_options_map_.end()) {
      success = static_cast<uint32_t>(
                    std::stoi(iter->second->pairing_auth_token)) == passkey;
    }

    if (success) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(callback), std::move(error_callback)),
          base::Milliseconds(kSimulateNormalPairTimeMultiplier *
                             simulation_interval_ms_));
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                         base::Unretained(this), object_path,
                         std::move(error_callback)),
          base::Milliseconds(simulation_interval_ms_));
    }

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::ConfirmationCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status) {
  DVLOG(1) << "ConfirmationCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                       base::Unretained(this), object_path, std::move(callback),
                       std::move(error_callback)),
        base::Milliseconds(kSimulateNormalPairTimeMultiplier *
                           simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                       base::Unretained(this), object_path,
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::SimulateKeypress(
    uint16_t entered,
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "SimulateKeypress " << entered << ": " << object_path.value();

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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::SimulateKeypress,
                       base::Unretained(this), entered + 1, object_path,
                       std::move(callback), std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));

  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                       base::Unretained(this), object_path, std::move(callback),
                       std::move(error_callback)),
        base::Milliseconds(simulation_interval_ms_));
  }
}

void FakeBluetoothDeviceClient::ConnectionCallback(
    const dbus::ObjectPath& object_path,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    BluetoothProfileServiceProvider::Delegate::Status status) {
  DVLOG(1) << "ConnectionCallback: " << object_path.value();

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
  DVLOG(1) << "DisconnectionCallback: " << object_path.value();

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
    const std::optional<std::string> name,
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
    id = base::Base64Encode(base::RandBytesAsVector(10));
    base::RemoveChars(id, "+/=", &id);
    device_path = dbus::ObjectPath(adapter_path.value() + "/dev" + id);
  } while (base::Contains(device_list_, device_path));

  std::unique_ptr<Properties> properties(new Properties(
      base::BindRepeating(&FakeBluetoothDeviceClient::OnPropertyChanged,
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

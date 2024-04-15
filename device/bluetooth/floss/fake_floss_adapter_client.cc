// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_adapter_client.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

namespace {

const int kDelayedTaskMs = 100;

FlossDeviceId ConvertAddressToDevice(const std::string& address) {
  static const base::flat_map<std::string, FlossDeviceId> address_to_device{
      {FakeFlossAdapterClient::kClassicAddress,
       FlossDeviceId{FakeFlossAdapterClient::kClassicAddress,
                     FakeFlossAdapterClient::kClassicName}},
      {FakeFlossAdapterClient::kPinCodeDisplayAddress,
       FlossDeviceId{FakeFlossAdapterClient::kPinCodeDisplayAddress,
                     FakeFlossAdapterClient::kPinCodeDisplayName}},
      {FakeFlossAdapterClient::kPasskeyDisplayAddress,
       FlossDeviceId{FakeFlossAdapterClient::kPasskeyDisplayAddress,
                     FakeFlossAdapterClient::kPasskeyDisplayName}},
      {FakeFlossAdapterClient::kPinCodeRequestAddress,
       FlossDeviceId{FakeFlossAdapterClient::kPinCodeRequestAddress,
                     FakeFlossAdapterClient::kPinCodeRequestName}},
      {FakeFlossAdapterClient::kPhoneAddress,
       FlossDeviceId{FakeFlossAdapterClient::kPhoneAddress,
                     FakeFlossAdapterClient::kPhoneName}},
      {FakeFlossAdapterClient::kPasskeyRequestAddress,
       FlossDeviceId{FakeFlossAdapterClient::kPasskeyRequestAddress,
                     FakeFlossAdapterClient::kPasskeyRequestName}},
      {FakeFlossAdapterClient::kJustWorksAddress,
       FlossDeviceId{FakeFlossAdapterClient::kJustWorksAddress,
                     FakeFlossAdapterClient::kJustWorksName}}};

  auto iter = address_to_device.find(address);
  if (iter != address_to_device.end()) {
    return iter->second;
  }
  return FlossDeviceId{address, ""};
}

uint32_t ConvertAddressToClassOfDevice(const std::string& address) {
  static const base::flat_map<std::string, uint32_t> address_to_cod{
      {FakeFlossAdapterClient::kClassicAddress,
       FakeFlossAdapterClient::kClassicClassOfDevice},
      {FakeFlossAdapterClient::kPinCodeDisplayAddress,
       FakeFlossAdapterClient::kPinCodeDisplayClassOfDevice},
      {FakeFlossAdapterClient::kPasskeyDisplayAddress,
       FakeFlossAdapterClient::kPasskeyDisplayClassOfDevice},
      {FakeFlossAdapterClient::kPinCodeRequestAddress,
       FakeFlossAdapterClient::kPinCodeRequestClassOfDevice},
      {FakeFlossAdapterClient::kPhoneAddress,
       FakeFlossAdapterClient::kPhoneClassOfDevice},
      {FakeFlossAdapterClient::kPasskeyRequestAddress,
       FakeFlossAdapterClient::kPasskeyRequestClassOfDevice},
      {FakeFlossAdapterClient::kJustWorksAddress,
       FakeFlossAdapterClient::kJustWorksClassOfDevice}};

  auto iter = address_to_cod.find(address);
  if (iter != address_to_cod.end()) {
    return iter->second;
  }
  return FakeFlossAdapterClient::kDefaultClassOfDevice;
}

}  // namespace

FakeFlossAdapterClient::FakeFlossAdapterClient()
    : bonded_addresses_({kBondedAddress1, kBondedAddress2}),
      connected_addresses_({}) {}

FakeFlossAdapterClient::~FakeFlossAdapterClient() = default;

const char FakeFlossAdapterClient::kBondedAddress1[] = "11:11:11:11:11:01";
const char FakeFlossAdapterClient::kBondedAddress2[] = "11:11:11:11:11:02";
const char FakeFlossAdapterClient::kPairedAddressBrEdr[] = "11:11:11:11:11:03";
const char FakeFlossAdapterClient::kPairedAddressLE[] = "11:11:11:11:11:04";
const uint32_t FakeFlossAdapterClient::kDefaultClassOfDevice = 0x240418;

const char FakeFlossAdapterClient::kClassicName[] = "Bluetooth 2.0 Mouse";
const char FakeFlossAdapterClient::kClassicAddress[] = "11:35:11:35:00:01";
const uint32_t FakeFlossAdapterClient::kClassicClassOfDevice = 0x002580;
const char FakeFlossAdapterClient::kPinCodeDisplayName[] =
    "Bluetooth 2.0 Keyboard";
const char FakeFlossAdapterClient::kPinCodeDisplayAddress[] =
    "11:35:11:35:00:02";
const uint32_t FakeFlossAdapterClient::kPinCodeDisplayClassOfDevice = 0x002540;
const char FakeFlossAdapterClient::kPasskeyDisplayName[] =
    "Bluetooth 2.1+ Keyboard";
const char FakeFlossAdapterClient::kPasskeyDisplayAddress[] =
    "11:35:11:35:00:03";
const uint32_t FakeFlossAdapterClient::kPasskeyDisplayClassOfDevice = 0x002540;
const char FakeFlossAdapterClient::kPinCodeRequestName[] = "PIN Device";
const char FakeFlossAdapterClient::kPinCodeRequestAddress[] =
    "11:35:11:35:00:04";
const uint32_t FakeFlossAdapterClient::kPinCodeRequestClassOfDevice = 0x240408;
const char FakeFlossAdapterClient::kPhoneName[] = "Phone";
const char FakeFlossAdapterClient::kPhoneAddress[] = "11:35:11:35:00:05";
const uint32_t FakeFlossAdapterClient::kPhoneClassOfDevice = 0x7a020c;
const char FakeFlossAdapterClient::kPasskeyRequestName[] = "Passkey Device";
const char FakeFlossAdapterClient::kPasskeyRequestAddress[] =
    "11:35:11:35:00:06";
const uint32_t FakeFlossAdapterClient::kPasskeyRequestClassOfDevice = 0x7a020c;
const char FakeFlossAdapterClient::kJustWorksName[] = "Just-Works Device";
const char FakeFlossAdapterClient::kJustWorksAddress[] = "11:35:11:35:00:07";
const uint32_t FakeFlossAdapterClient::kJustWorksClassOfDevice = 0x240428;

const uint32_t FakeFlossAdapterClient::kPasskey = 123456;
const char FakeFlossAdapterClient::kPinCode[] = "012345";

void FakeFlossAdapterClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const int adapter_index,
                                  base::Version version,
                                  base::OnceClosure on_ready) {
  bus_ = bus;
  adapter_path_ = GenerateAdapterPath(adapter_index);
  service_name_ = service_name;
  version_ = version;
  std::move(on_ready).Run();
}

void FakeFlossAdapterClient::SetName(ResponseCallback<Void> callback,
                                     const std::string& name) {
  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::StartDiscovery(ResponseCallback<Void> callback) {
  // Fail fast if we're meant to fail discovery
  if (fail_discovery_) {
    fail_discovery_ = std::nullopt;

    std::move(callback).Run(base::unexpected(
        Error("org.chromium.bluetooth.Bluetooth.FooError", "Foo error")));
    return;
  }

  // Simulate devices being discovered.

  const auto discoverable_devices = std::vector{
      // Report empty name first to simulate this device sends its name later.
      FlossDeviceId{kClassicAddress, ""},
      FlossDeviceId{kClassicAddress, kClassicName},

      FlossDeviceId{kPinCodeDisplayAddress, kPinCodeDisplayName},
      FlossDeviceId{kPasskeyDisplayAddress, kPasskeyDisplayName},
      FlossDeviceId{kPinCodeRequestAddress, kPinCodeRequestName},
      FlossDeviceId{kPhoneAddress, kPhoneName},
      FlossDeviceId{kPasskeyRequestAddress, kPasskeyRequestName},
      FlossDeviceId{kJustWorksAddress, kJustWorksName}};

  for (const auto& device : discoverable_devices) {
    if (base::Contains(connected_addresses_, device.address)) {
      // Skip connected devices.
      continue;
    }
    for (auto& observer : observers_) {
      observer.AdapterFoundDevice(device);
      observer.AdapterDevicePropertyChanged(
          FlossAdapterClient::BtPropertyType::kBdName, device);
      observer.AdapterDevicePropertyChanged(
          FlossAdapterClient::BtPropertyType::kClassOfDevice, device);
      observer.AdapterDevicePropertyChanged(
          FlossAdapterClient::BtPropertyType::kTypeOfDevice, device);
    }
  }

  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::CancelDiscovery(ResponseCallback<Void> callback) {
  // Will need to stop simulated discovery once the simulation grows.
  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::CreateBond(ResponseCallback<bool> callback,
                                        FlossDeviceId device,
                                        BluetoothTransport transport) {
  if (fail_bonding_) {
    fail_bonding_ = std::nullopt;

    std::move(callback).Run(base::unexpected(
        Error("org.chromium.Error.Failed", "Bonding failed by request")));
    return;
  }

  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(
        device, /*status=*/0,
        FlossAdapterClient::BondState::kBondingInProgress);
  }

  if (device.address == kJustWorksAddress ||
      device.address == kClassicAddress) {
    for (auto& observer : observers_) {
      observer.DeviceBondStateChanged(device, /*status=*/0,
                                      FlossAdapterClient::BondState::kBonded);
    }
    bonded_addresses_.insert(device.address);
    SetConnected(device.address, true);

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else if (device.address == kPasskeyDisplayAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyNotification,
                                 kPasskey);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else if (device.address == kPhoneAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyConfirmation,
                                 kPasskey);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else if (device.address == kPasskeyRequestAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyEntry, 0);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else if (device.address == kPinCodeDisplayAddress) {
    for (auto& observer : observers_) {
      observer.AdapterPinDisplay(device, std::string(kPinCode));
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else if (device.address == kPinCodeRequestAddress) {
    for (auto& observer : observers_) {
      observer.AdapterPinRequest(device, 0, false);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else {
    for (auto& observer : observers_) {
      observer.DeviceBondStateChanged(
          device, static_cast<uint32_t>(BtifStatus::kFail),
          FlossAdapterClient::BondState::kNotBonded);
    }

    PostDelayedTask(base::BindOnce(
        std::move(callback),
        base::unexpected(Error("org.chromium.bluetooth.UnknownDevice", ""))));
  }
}

void FakeFlossAdapterClient::CreateBond(
    ResponseCallback<FlossDBusClient::BtifStatus> callback,
    FlossDeviceId device,
    BluetoothTransport transport) {
  if (fail_bonding_) {
    fail_bonding_ = std::nullopt;

    std::move(callback).Run(base::unexpected(
        Error("org.chromium.Error.Failed", "Bonding failed by request")));
    return;
  }

  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(
        device, /*status=*/0,
        FlossAdapterClient::BondState::kBondingInProgress);
  }

  if (device.address == kJustWorksAddress ||
      device.address == kClassicAddress) {
    for (auto& observer : observers_) {
      observer.DeviceBondStateChanged(device, /*status=*/0,
                                      FlossAdapterClient::BondState::kBonded);
    }
    bonded_addresses_.insert(device.address);
    SetConnected(device.address, true);

    PostDelayedTask(base::BindOnce(std::move(callback),
                                   FlossDBusClient::BtifStatus::kSuccess));
  } else if (device.address == kPasskeyDisplayAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyNotification,
                                 kPasskey);
    }

    PostDelayedTask(base::BindOnce(std::move(callback),
                                   FlossDBusClient::BtifStatus::kSuccess));
  } else if (device.address == kPhoneAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyConfirmation,
                                 kPasskey);
    }

    PostDelayedTask(base::BindOnce(std::move(callback),
                                   FlossDBusClient::BtifStatus::kSuccess));
  } else if (device.address == kPasskeyRequestAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyEntry, 0);
    }

    PostDelayedTask(base::BindOnce(std::move(callback),
                                   FlossDBusClient::BtifStatus::kSuccess));
  } else if (device.address == kPinCodeDisplayAddress) {
    for (auto& observer : observers_) {
      observer.AdapterPinDisplay(device, std::string(kPinCode));
    }

    PostDelayedTask(base::BindOnce(std::move(callback),
                                   FlossDBusClient::BtifStatus::kSuccess));
  } else if (device.address == kPinCodeRequestAddress) {
    for (auto& observer : observers_) {
      observer.AdapterPinRequest(device, 0, false);
    }

    PostDelayedTask(base::BindOnce(std::move(callback),
                                   FlossDBusClient::BtifStatus::kSuccess));
  } else {
    for (auto& observer : observers_) {
      observer.DeviceBondStateChanged(
          device, static_cast<uint32_t>(BtifStatus::kFail),
          FlossAdapterClient::BondState::kNotBonded);
    }

    PostDelayedTask(base::BindOnce(
        std::move(callback),
        base::unexpected(Error("org.chromium.bluetooth.UnknownDevice", ""))));
  }
}

void FakeFlossAdapterClient::CancelBondProcess(ResponseCallback<bool> callback,
                                               FlossDeviceId device) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kNotBonded);
  }
  PostDelayedTask(base::BindOnce(std::move(callback), true));
}

void FakeFlossAdapterClient::RemoveBond(ResponseCallback<bool> callback,
                                        FlossDeviceId device) {
  if (!base::Contains(bonded_addresses_, device.address)) {
    PostDelayedTask(base::BindOnce(std::move(callback), false));
    return;
  }

  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kNotBonded);
  }
  bonded_addresses_.erase(device.address);
  SetConnected(device.address, false);

  PostDelayedTask(base::BindOnce(std::move(callback), true));
}

void FakeFlossAdapterClient::GetRemoteType(
    ResponseCallback<BluetoothDeviceType> callback,
    FlossDeviceId device) {
  auto remote_type = BluetoothDeviceType::kBredr;
  if (device.address == kBondedAddress1 || device.address == kBondedAddress2 ||
      device.address == kPairedAddressLE) {
    remote_type = BluetoothDeviceType::kBle;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), remote_type));
}

void FakeFlossAdapterClient::GetRemoteClass(ResponseCallback<uint32_t> callback,
                                            FlossDeviceId device) {
  uint32_t cod = ConvertAddressToClassOfDevice(device.address);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), cod));
}

void FakeFlossAdapterClient::GetRemoteAppearance(
    ResponseCallback<uint16_t> callback,
    FlossDeviceId device) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakeFlossAdapterClient::GetConnectionState(
    ResponseCallback<uint32_t> callback,
    const FlossDeviceId& device) {
  FlossAdapterClient::ConnectionState conn_state =
      FlossAdapterClient::ConnectionState::kDisconnected;

  if (base::Contains(connected_addresses_, device.address)) {
    if (device.address == kPairedAddressBrEdr) {
      conn_state = FlossAdapterClient::ConnectionState::kPairedBREDROnly;
    } else if (device.address == kPairedAddressLE) {
      conn_state = FlossAdapterClient::ConnectionState::kPairedLEOnly;
    } else {
      conn_state = FlossAdapterClient::ConnectionState::kConnectedOnly;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), static_cast<uint32_t>(conn_state)));
}

void FakeFlossAdapterClient::GetRemoteUuids(
    ResponseCallback<device::BluetoothDevice::UUIDList> callback,
    FlossDeviceId device) {
  device::BluetoothDevice::UUIDList uuid_list;
  PostDelayedTask(base::BindOnce(std::move(callback), std::move(uuid_list)));
}

void FakeFlossAdapterClient::GetRemoteVendorProductInfo(
    ResponseCallback<FlossAdapterClient::VendorProductInfo> callback,
    FlossDeviceId device) {
  FlossAdapterClient::VendorProductInfo info;
  PostDelayedTask(base::BindOnce(std::move(callback), std::move(info)));
}

void FakeFlossAdapterClient::GetRemoteAddressType(
    ResponseCallback<FlossAdapterClient::BtAddressType> callback,
    FlossDeviceId device) {
  PostDelayedTask(base::BindOnce(std::move(callback),
                                 FlossAdapterClient::BtAddressType::kPublic));
}

void FakeFlossAdapterClient::GetBondState(ResponseCallback<uint32_t> callback,
                                          const FlossDeviceId& device) {
  FlossAdapterClient::BondState bond_state =
      base::Contains(bonded_addresses_, device.address)
          ? floss::FlossAdapterClient::BondState::kBonded
          : floss::FlossAdapterClient::BondState::kNotBonded;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), static_cast<uint32_t>(bond_state)));
}

void FakeFlossAdapterClient::ConnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  SetConnected(device.address, true);
  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::ConnectAllEnabledProfiles(
    ResponseCallback<FlossDBusClient::BtifStatus> callback,
    const FlossDeviceId& device) {
  SetConnected(device.address, true);
  PostDelayedTask(base::BindOnce(std::move(callback),
                                 FlossDBusClient::BtifStatus::kSuccess));
}

void FakeFlossAdapterClient::DisconnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  SetConnected(device.address, false);
  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::PostDelayedTask(base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback), base::Milliseconds(kDelayedTaskMs));
}

void FakeFlossAdapterClient::SetConnected(const std::string& address,
                                          bool connected) {
  if (base::Contains(connected_addresses_, address) == connected) {
    return;
  }
  const auto device = ConvertAddressToDevice(address);
  for (auto& observer : observers_) {
    if (connected) {
      connected_addresses_.insert(address);
      observer.AdapterDeviceConnected(device);
    } else {
      connected_addresses_.erase(address);
      observer.AdapterDeviceDisconnected(device);
    }
  }
}

void FakeFlossAdapterClient::NotifyObservers(
    const base::RepeatingCallback<void(Observer*)>& notify) const {
  for (auto& observer : observers_) {
    notify.Run(&observer);
  }
}

void FakeFlossAdapterClient::FailNextDiscovery() {
  fail_discovery_ = std::make_optional(true);
}

void FakeFlossAdapterClient::FailNextBonding() {
  fail_bonding_ = std::make_optional(true);
}

void FakeFlossAdapterClient::SetPairingConfirmation(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device,
    bool accept) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kBonded);
  }
  bonded_addresses_.insert(device.address);
  SetConnected(device.address, true);

  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::SetPin(ResponseCallback<Void> callback,
                                    const FlossDeviceId& device,
                                    bool accept,
                                    const std::vector<uint8_t>& pin) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kBonded);
  }
  bonded_addresses_.insert(device.address);
  SetConnected(device.address, true);

  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::GetBondedDevices() {
  for (const auto& addr : bonded_addresses_) {
    const auto device_id = ConvertAddressToDevice(addr);
    for (auto& observer : observers_) {
      observer.AdapterFoundDevice(device_id);
    }
  }
}

void FakeFlossAdapterClient::GetConnectedDevices() {
  for (const auto& addr : connected_addresses_) {
    const auto device_id = ConvertAddressToDevice(addr);
    for (auto& observer : observers_) {
      observer.AdapterFoundDevice(device_id);
      observer.AdapterDeviceConnected(device_id);
    }
  }
}

}  // namespace floss

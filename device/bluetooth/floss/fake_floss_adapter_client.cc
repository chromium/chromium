// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_adapter_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

namespace {

const int kDelayedTaskMs = 100;

}  // namespace

FakeFlossAdapterClient::FakeFlossAdapterClient() = default;

FakeFlossAdapterClient::~FakeFlossAdapterClient() = default;

const char FakeFlossAdapterClient::kBondedAddress1[] = "11:11:11:11:11:01";
const char FakeFlossAdapterClient::kBondedAddress2[] = "11:11:11:11:11:02";
const char FakeFlossAdapterClient::kPairedAddressBrEdr[] = "11:11:11:11:11:03";
const char FakeFlossAdapterClient::kPairedAddressLE[] = "11:11:11:11:11:04";
const char FakeFlossAdapterClient::kJustWorksAddress[] = "11:22:33:44:55:66";
const char FakeFlossAdapterClient::kKeyboardAddress[] = "aa:aa:aa:aa:aa:aa";
const char FakeFlossAdapterClient::kPhoneAddress[] = "bb:bb:bb:bb:bb:bb";
const char FakeFlossAdapterClient::kOldDeviceAddress[] = "cc:cc:cc:cc:cc:cc";
const char FakeFlossAdapterClient::kClassicAddress[] = "dd:dd:dd:dd:dd:dd";
const char FakeFlossAdapterClient::kClassicName[] = "Classic Device";
const uint32_t FakeFlossAdapterClient::kPasskey = 123456;
const uint32_t FakeFlossAdapterClient::kHeadsetClassOfDevice = 2360344;

void FakeFlossAdapterClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const int adapter_index) {}

void FakeFlossAdapterClient::StartDiscovery(ResponseCallback<Void> callback) {
  // Fail fast if we're meant to fail discovery
  if (fail_discovery_) {
    fail_discovery_ = absl::nullopt;

    std::move(callback).Run(base::unexpected(
        Error("org.chromium.bluetooth.Bluetooth.FooError", "Foo error")));
    return;
  }

  // Simulate devices being discovered.

  for (auto& observer : observers_) {
    observer.AdapterFoundDevice(FlossDeviceId({kJustWorksAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kKeyboardAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kPhoneAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kOldDeviceAddress, ""}));
    // Simulate a device which sends its name later
    observer.AdapterFoundDevice(FlossDeviceId({kClassicAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kClassicAddress, kClassicName}));
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
  // TODO(b/202874707): Simulate pairing failures.

  if (device.address == kJustWorksAddress) {
    for (auto& observer : observers_) {
      observer.DeviceBondStateChanged(device, /*status=*/0,
                                      FlossAdapterClient::BondState::kBonded);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else if (device.address == kKeyboardAddress) {
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
  } else if (device.address == kOldDeviceAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyEntry, 0);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), true));
  } else {
    PostDelayedTask(base::BindOnce(
        std::move(callback),
        base::unexpected(Error("org.chromium.bluetooth.UnknownDevice", ""))));
  }
}

void FakeFlossAdapterClient::RemoveBond(ResponseCallback<bool> callback,
                                        FlossDeviceId device) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kNotBonded);
  }

  PostDelayedTask(base::BindOnce(std::move(callback), true));
}

void FakeFlossAdapterClient::GetRemoteType(
    ResponseCallback<BluetoothDeviceType> callback,
    FlossDeviceId device) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), BluetoothDeviceType::kBle));
}

void FakeFlossAdapterClient::GetRemoteClass(ResponseCallback<uint32_t> callback,
                                            FlossDeviceId device) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), kHeadsetClassOfDevice));
}

void FakeFlossAdapterClient::GetRemoteAppearance(
    ResponseCallback<uint16_t> callback,
    FlossDeviceId device) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 1));
}

void FakeFlossAdapterClient::GetConnectionState(
    ResponseCallback<uint32_t> callback,
    const FlossDeviceId& device) {
  FlossAdapterClient::ConnectionState conn_state =
      FlossAdapterClient::ConnectionState::kDisconnected;

  // One of the bonded devices is already connected at the beginning.
  // The Paired devices will also have paired states at the beginning.
  if (device.address == kBondedAddress1) {
    conn_state = FlossAdapterClient::ConnectionState::kConnectedOnly;
  } else if (device.address == kPairedAddressBrEdr) {
    conn_state = FlossAdapterClient::ConnectionState::kPairedBREDROnly;
  } else if (device.address == kPairedAddressLE) {
    conn_state = FlossAdapterClient::ConnectionState::kPairedLEOnly;
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

void FakeFlossAdapterClient::GetBondState(ResponseCallback<uint32_t> callback,
                                          const FlossDeviceId& device) {
  FlossAdapterClient::BondState bond_state =
      (device.address == kBondedAddress1 || device.address == kBondedAddress2)
          ? floss::FlossAdapterClient::BondState::kBonded
          : floss::FlossAdapterClient::BondState::kNotBonded;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), static_cast<uint32_t>(bond_state)));
}

void FakeFlossAdapterClient::ConnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::DisconnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::PostDelayedTask(base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback), base::Milliseconds(kDelayedTaskMs));
}

void FakeFlossAdapterClient::NotifyObservers(
    const base::RepeatingCallback<void(Observer*)>& notify) const {
  for (auto& observer : observers_) {
    notify.Run(&observer);
  }
}

void FakeFlossAdapterClient::FailNextDiscovery() {
  fail_discovery_ = absl::make_optional(true);
}

void FakeFlossAdapterClient::SetPairingConfirmation(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device,
    bool accept) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kBonded);
  }

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

  PostDelayedTask(base::BindOnce(std::move(callback), Void{}));
}

void FakeFlossAdapterClient::GetBondedDevices() {
  std::vector<FlossDeviceId> known_devices;
  known_devices.push_back(
      FlossDeviceId({.address = kBondedAddress1, .name = ""}));
  known_devices.push_back(
      FlossDeviceId({.address = kBondedAddress2, .name = ""}));
  for (const auto& device_id : known_devices) {
    for (auto& observer : observers_) {
      observer.AdapterFoundDevice(device_id);
    }
  }
}

void FakeFlossAdapterClient::GetConnectedDevices() {
  std::vector<FlossDeviceId> connected_devices;
  connected_devices.push_back(
      FlossDeviceId({.address = kPairedAddressBrEdr, .name = "Paired BREDR"}));
  connected_devices.push_back(
      FlossDeviceId({.address = kPairedAddressLE, .name = "Paired LE"}));

  for (const auto& device_id : connected_devices) {
    for (auto& observer : observers_) {
      observer.AdapterFoundDevice(device_id);
      observer.AdapterDeviceConnected(device_id);
    }
  }
}

}  // namespace floss

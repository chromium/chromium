// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_adapter_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/threading/thread_task_runner_handle.h"

namespace floss {

namespace {

const int kDelayedTaskMs = 100;

}  // namespace

FakeFlossAdapterClient::FakeFlossAdapterClient() = default;

FakeFlossAdapterClient::~FakeFlossAdapterClient() = default;

const char FakeFlossAdapterClient::kBondedAddress1[] = "11:11:11:11:11:01";
const char FakeFlossAdapterClient::kBondedAddress2[] = "11:11:11:11:11:02";
const char FakeFlossAdapterClient::kJustWorksAddress[] = "11:22:33:44:55:66";
const char FakeFlossAdapterClient::kKeyboardAddress[] = "aa:aa:aa:aa:aa:aa";
const char FakeFlossAdapterClient::kPhoneAddress[] = "bb:bb:bb:bb:bb:bb";
const char FakeFlossAdapterClient::kOldDeviceAddress[] = "cc:cc:cc:cc:cc:cc";
const uint32_t FakeFlossAdapterClient::kPasskey = 123456;

void FakeFlossAdapterClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const std::string& adapter_path) {}

void FakeFlossAdapterClient::StartDiscovery(ResponseCallback<Void> callback) {
  // Simulate devices being discovered.

  for (auto& observer : observers_) {
    observer.AdapterFoundDevice(FlossDeviceId({kJustWorksAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kKeyboardAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kPhoneAddress, ""}));
    observer.AdapterFoundDevice(FlossDeviceId({kOldDeviceAddress, ""}));
  }

  PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                 /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::CancelDiscovery(ResponseCallback<Void> callback) {
  // Will need to stop simulated discovery once the simulation grows.
  PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                 /*err=*/absl::nullopt));
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

    PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                   /*err=*/absl::nullopt));
  } else if (device.address == kKeyboardAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyNotification,
                                 kPasskey);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                   /*err=*/absl::nullopt));
  } else if (device.address == kPhoneAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyConfirmation,
                                 kPasskey);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                   /*err=*/absl::nullopt));
  } else if (device.address == kOldDeviceAddress) {
    for (auto& observer : observers_) {
      observer.AdapterSspRequest(device, /*cod=*/0,
                                 BluetoothSspVariant::kPasskeyEntry, 0);
    }

    PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                   /*err=*/absl::nullopt));
  } else {
    PostDelayedTask(base::BindOnce(
        std::move(callback), /*ret=*/absl::nullopt,
        floss::Error("org.chromium.bluetooth.UnknownDevice", /*message=*/"")));
  }
}

void FakeFlossAdapterClient::RemoveBond(ResponseCallback<bool> callback,
                                        FlossDeviceId device) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kNotBonded);
  }

  PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                 /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::GetConnectionState(
    ResponseCallback<uint32_t> callback,
    const FlossDeviceId& device) {
  // One of the bonded devices is already connected at the beginning.
  uint32_t conn_state = (device.address == kBondedAddress1) ? 1 : 0;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), conn_state, /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::GetBondState(ResponseCallback<uint32_t> callback,
                                          const FlossDeviceId& device) {
  FlossAdapterClient::BondState bond_state =
      (device.address == kBondedAddress1 || device.address == kBondedAddress2)
          ? floss::FlossAdapterClient::BondState::kBonded
          : floss::FlossAdapterClient::BondState::kNotBonded;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), static_cast<uint32_t>(bond_state),
                     /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::ConnectAllEnabledProfiles(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device) {
  PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                 /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::PostDelayedTask(base::OnceClosure callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(callback), base::Milliseconds(kDelayedTaskMs));
}

void FakeFlossAdapterClient::NotifyObservers(
    const base::RepeatingCallback<void(Observer*)>& notify) const {
  for (auto& observer : observers_) {
    notify.Run(&observer);
  }
}

void FakeFlossAdapterClient::SetPairingConfirmation(
    ResponseCallback<Void> callback,
    const FlossDeviceId& device,
    bool accept) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kBonded);
  }

  PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                 /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::SetPin(ResponseCallback<Void> callback,
                                    const FlossDeviceId& device,
                                    bool accept,
                                    const std::vector<uint8_t>& pin) {
  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kBonded);
  }

  PostDelayedTask(base::BindOnce(std::move(callback), /*ret=*/absl::nullopt,
                                 /*err=*/absl::nullopt));
}

void FakeFlossAdapterClient::GetBondedDevices(
    ResponseCallback<std::vector<FlossDeviceId>> callback) {
  std::vector<FlossDeviceId> known_devices;
  known_devices.push_back(
      FlossDeviceId({.address = kBondedAddress1, .name = ""}));
  known_devices.push_back(
      FlossDeviceId({.address = kBondedAddress2, .name = ""}));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), known_devices,
                                /*err=*/absl::nullopt));
}

}  // namespace floss

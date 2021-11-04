// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_adapter_client.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace floss {

namespace {

const int kDelayedTaskMs = 100;

}  // namespace

FakeFlossAdapterClient::FakeFlossAdapterClient() = default;

FakeFlossAdapterClient::~FakeFlossAdapterClient() = default;

const char FakeFlossAdapterClient::kJustWorksAddress[] = "11:22:33:44:55:66";

void FakeFlossAdapterClient::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const std::string& adapter_path) {}

void FakeFlossAdapterClient::StartDiscovery(ResponseCallback callback) {
  // Simulate devices being discovered.

  for (auto& observer : observers_) {
    observer.AdapterFoundDevice(FlossDeviceId({kJustWorksAddress, ""}));
  }

  PostDelayedTask(base::BindOnce(std::move(callback), absl::nullopt));
}

void FakeFlossAdapterClient::CancelDiscovery(ResponseCallback callback) {
  // Will need to stop simulated discovery once the simulation grows.
  PostDelayedTask(base::BindOnce(std::move(callback), absl::nullopt));
}

void FakeFlossAdapterClient::CreateBond(ResponseCallback callback,
                                        FlossDeviceId device,
                                        BluetoothTransport transport) {
  // TODO(b/202874707): Simulate pairing failures.

  for (auto& observer : observers_) {
    observer.DeviceBondStateChanged(device, /*status=*/0,
                                    FlossAdapterClient::BondState::kBonded);
  }

  PostDelayedTask(base::BindOnce(std::move(callback), absl::nullopt));
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

}  // namespace floss

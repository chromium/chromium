// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/fake_floss_bluetooth_telephony_client.h"

#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossBluetoothTelephonyClient::FakeFlossBluetoothTelephonyClient() =
    default;
FakeFlossBluetoothTelephonyClient::~FakeFlossBluetoothTelephonyClient() =
    default;

void FakeFlossBluetoothTelephonyClient::Init(dbus::Bus* bus,
                                             const std::string& service_name,
                                             const int adapter_index,
                                             base::Version version,
                                             base::OnceClosure on_ready) {
  version_ = version;
  std::move(on_ready).Run();
}

void FakeFlossBluetoothTelephonyClient::SetPhoneOpsEnabled(
    ResponseCallback<Void> callback,
    bool enabled) {
  std::move(callback).Run(DBusResult<Void>());
}

}  // namespace floss

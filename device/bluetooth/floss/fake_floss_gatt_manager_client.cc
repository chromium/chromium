// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_gatt_manager_client.h"

#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossGattManagerClient::FakeFlossGattManagerClient() = default;
FakeFlossGattManagerClient::~FakeFlossGattManagerClient() = default;

void FakeFlossGattManagerClient::Init(dbus::Bus* bus,
                                      const std::string& service_name,
                                      const int adapter_index,
                                      base::OnceClosure on_ready) {
  std::move(on_ready).Run();
}

void FakeFlossGattManagerClient::Connect(ResponseCallback<Void> callback,
                                         const std::string& remote_device,
                                         const BluetoothTransport& transport) {
  std::move(callback).Run(DBusResult<Void>({}));
}

}  // namespace floss

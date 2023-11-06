// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_gatt_manager_client.h"

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossGattManagerClient::FakeFlossGattManagerClient() = default;
FakeFlossGattManagerClient::~FakeFlossGattManagerClient() = default;

void FakeFlossGattManagerClient::Init(dbus::Bus* bus,
                                      const std::string& service_name,
                                      const int adapter_index,
                                      base::Version version,
                                      base::OnceClosure on_ready) {
  version_ = version;
  std::move(on_ready).Run();
}

void FakeFlossGattManagerClient::Connect(ResponseCallback<Void> callback,
                                         const std::string& remote_device,
                                         const BluetoothTransport& transport,
                                         bool is_direct) {
  std::move(callback).Run(DBusResult<Void>({}));
}

void FakeFlossGattManagerClient::AddService(ResponseCallback<Void> callback,
                                            GattService service) {
  std::move(callback).Run(DBusResult<Void>({}));

  GattService added_service(service);
  added_service.instance_id = static_cast<int32_t>(base::RandUint64());
  for (auto& characteristic : added_service.characteristics) {
    characteristic.instance_id = static_cast<int32_t>(base::RandUint64());
    for (auto& descriptor : characteristic.descriptors) {
      descriptor.instance_id = static_cast<int32_t>(base::RandUint64());
    }
  }

  int32_t instance_id = added_service.instance_id;
  if (base::Contains(services_, instance_id)) {
    GattServerServiceAdded(GattStatus::kError, added_service);
    return;
  }
  DCHECK(!base::Contains(services_, instance_id));
  services_[instance_id] = added_service;

  GattServerServiceAdded(GattStatus::kSuccess, added_service);
}

void FakeFlossGattManagerClient::RemoveService(ResponseCallback<Void> callback,
                                               int32_t handle) {
  std::move(callback).Run(DBusResult<Void>({}));

  if (!base::Contains(services_, handle)) {
    GattServerServiceRemoved(GattStatus::kError, handle);
    return;
  }
  DCHECK(base::Contains(services_, handle));
  services_.erase(handle);

  GattServerServiceRemoved(GattStatus::kSuccess, handle);
}

}  // namespace floss

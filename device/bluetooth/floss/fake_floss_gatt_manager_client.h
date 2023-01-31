// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_GATT_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_GATT_MANAGER_CLIENT_H_

#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossGattManagerClient
    : public FlossGattManagerClient {
 public:
  FakeFlossGattManagerClient();
  ~FakeFlossGattManagerClient() override;

  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index) override;

  void Connect(ResponseCallback<Void> callback,
               const std::string& remote_device,
               const BluetoothTransport& transport) override;

 private:
  base::WeakPtrFactory<FakeFlossGattManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_GATT_MANAGER_CLIENT_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_BLUETOOTH_TELEPHONY_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_BLUETOOTH_TELEPHONY_CLIENT_H_

#include "device/bluetooth/floss/floss_bluetooth_telephony_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossBluetoothTelephonyClient
    : public FlossBluetoothTelephonyClient {
 public:
  FakeFlossBluetoothTelephonyClient();
  ~FakeFlossBluetoothTelephonyClient() override;

  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

  void SetPhoneOpsEnabled(ResponseCallback<Void> callback,
                          bool enabled) override;

 private:
  base::WeakPtrFactory<FakeFlossBluetoothTelephonyClient> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_BLUETOOTH_TELEPHONY_CLIENT_H_

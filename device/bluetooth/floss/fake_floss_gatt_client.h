// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_GATT_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_GATT_CLIENT_H_

#include "device/bluetooth/floss/floss_gatt_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossGattClient : public FlossGattClient {
 public:
  FakeFlossGattClient();
  ~FakeFlossGattClient() override;

  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index) override;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_GATT_CLIENT_H_

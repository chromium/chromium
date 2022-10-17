// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_gatt_client.h"

namespace floss {

FakeFlossGattClient::FakeFlossGattClient() = default;
FakeFlossGattClient::~FakeFlossGattClient() = default;

void FakeFlossGattClient::Init(dbus::Bus* bus,
                               const std::string& service_name,
                               const int adapter_index) {}
}  // namespace floss

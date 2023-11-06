// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_bluetooth_telephony_client.h"

namespace floss {

FlossBluetoothTelephonyClient::FlossBluetoothTelephonyClient() = default;
FlossBluetoothTelephonyClient::~FlossBluetoothTelephonyClient() = default;

std::unique_ptr<FlossBluetoothTelephonyClient>
FlossBluetoothTelephonyClient::Create() {
  return std::make_unique<FlossBluetoothTelephonyClient>();
}

void FlossBluetoothTelephonyClient::SetPhoneOpsEnabled(
    ResponseCallback<Void> callback,
    bool enabled) {
  CallBluetoothTelephonyMethod<Void>(
      std::move(callback), bluetooth_telephony::kSetPhoneOpsEnabled, enabled);
}

void FlossBluetoothTelephonyClient::Init(dbus::Bus* bus,
                                         const std::string& service_name,
                                         const int adapter_index,
                                         base::Version version,
                                         base::OnceClosure on_ready) {
  bus_ = bus;
  service_name_ = service_name;
  bluetooth_telephony_adapter_path_ =
      GenerateBluetoothTelephonyPath(adapter_index);
  version_ = version;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, bluetooth_telephony_adapter_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossBluetoothTelephonyClient couldn't init. Object proxy "
                  "was null.";
    return;
  }

  std::move(on_ready).Run();
}

}  // namespace floss

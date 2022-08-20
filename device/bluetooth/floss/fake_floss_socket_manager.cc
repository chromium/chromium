// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_socket_manager.h"

#include "base/notreached.h"

namespace floss {

FakeFlossSocketManager::FakeFlossSocketManager() = default;
FakeFlossSocketManager::~FakeFlossSocketManager() = default;

void FakeFlossSocketManager::Init(dbus::Bus* bus,
                                  const std::string& service_name,
                                  const std::string& adapter_path) {
  NOTIMPLEMENTED();
}

void FakeFlossSocketManager::ListenUsingL2cap(
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  NOTIMPLEMENTED();
}

void FakeFlossSocketManager::ListenUsingRfcomm(
    const std::string& name,
    const device::BluetoothUUID& uuid,
    const Security security_level,
    ResponseCallback<BtifStatus> callback,
    ConnectionStateChanged ready_cb,
    ConnectionAccepted new_connection_cb) {
  NOTIMPLEMENTED();
}

void FakeFlossSocketManager::ConnectUsingL2cap(
    const FlossDeviceId& remote_device,
    const int psm,
    const Security security_level,
    ConnectionCompleted callback) {
  NOTIMPLEMENTED();
}

void FakeFlossSocketManager::ConnectUsingRfcomm(
    const FlossDeviceId& remote_device,
    const device::BluetoothUUID& uuid,
    const Security security_level,
    ConnectionCompleted callback) {
  NOTIMPLEMENTED();
}

void FakeFlossSocketManager::Accept(const SocketId id,
                                    absl::optional<uint32_t> timeout_ms,
                                    ResponseCallback<BtifStatus> callback) {
  NOTIMPLEMENTED();
}

void FakeFlossSocketManager::Close(const SocketId id,
                                   ResponseCallback<BtifStatus> callback) {
  NOTIMPLEMENTED();
}

}  // namespace floss

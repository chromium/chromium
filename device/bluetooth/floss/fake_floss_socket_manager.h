// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_SOCKET_MANAGER_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_SOCKET_MANAGER_H_

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_socket_manager.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossSocketManager
    : public FlossSocketManager {
 public:
  FakeFlossSocketManager();
  ~FakeFlossSocketManager() override;

  // Fake overrides.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const std::string& adapter_path) override;
  void ListenUsingL2cap(const Security security_level,
                        ResponseCallback<BtifStatus> callback,
                        ConnectionStateChanged ready_cb,
                        ConnectionAccepted new_connection_cb) override;
  void ListenUsingRfcomm(const std::string& name,
                         const device::BluetoothUUID& uuid,
                         const Security security_level,
                         ResponseCallback<BtifStatus> callback,
                         ConnectionStateChanged ready_cb,
                         ConnectionAccepted new_connection_cb) override;
  void ConnectUsingL2cap(const FlossDeviceId& remote_device,
                         const int psm,
                         const Security security_level,
                         ConnectionCompleted callback) override;
  void ConnectUsingRfcomm(const FlossDeviceId& remote_device,
                          const device::BluetoothUUID& uuid,
                          const Security security_level,
                          ConnectionCompleted callback) override;
  void Accept(const SocketId id,
              absl::optional<uint32_t> timeout_ms,
              ResponseCallback<BtifStatus> callback) override;
  void Close(const SocketId id, ResponseCallback<BtifStatus> callback) override;

 private:
  base::WeakPtrFactory<FakeFlossSocketManager> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_SOCKET_MANAGER_H_

// Copyright 2022 The Chromium Authors
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

  // Recognized uuid for testing rfcomm socket.
  static const char kRfcommUuid[];

  // Recognized psm for testing l2cap socket.
  static const int kL2capPsm;

  // Uuid which will fail to be registered.
  static const char kUnregisterableUuid[];

  // Fake overrides.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;
  void ListenUsingL2cap(const Security security_level,
                        ResponseCallback<BtifStatus> callback,
                        ConnectionStateChanged ready_cb,
                        ConnectionAccepted new_connection_cb) override;
  void ListenUsingL2capLe(const Security security_level,
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
  void ConnectUsingL2capLe(const FlossDeviceId& remote_device,
                           const int psm,
                           const Security security_level,
                           ConnectionCompleted callback) override;
  void ConnectUsingRfcomm(const FlossDeviceId& remote_device,
                          const device::BluetoothUUID& uuid,
                          const Security security_level,
                          ConnectionCompleted callback) override;
  void Accept(const SocketId id,
              std::optional<uint32_t> timeout_ms,
              ResponseCallback<BtifStatus> callback) override;
  void Close(const SocketId id, ResponseCallback<BtifStatus> callback) override;

  // Send ready state to a listener socket.
  void SendSocketReady(const SocketId id,
                       const device::BluetoothUUID& uuid,
                       const BtifStatus status);

  // Send closed state to a listener socket.
  void SendSocketClosed(const SocketId id, const BtifStatus status);

  // Send an incoming connection to a listener socket.
  void SendIncomingConnection(const SocketId listener_id,
                              const FlossDeviceId& remote_device,
                              const device::BluetoothUUID& uuid);

  // Gets the next id to be used for sockets.
  SocketId GetNextId() const { return socket_id_ctr_; }

 private:
  SocketId socket_id_ctr_ = 100;

  base::WeakPtrFactory<FakeFlossSocketManager> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_SOCKET_MANAGER_H_

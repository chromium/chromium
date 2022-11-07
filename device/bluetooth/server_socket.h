// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_SERVER_SOCKET_H_
#define DEVICE_BLUETOOTH_SERVER_SOCKET_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"

namespace device {
class BluetoothDevice;
class BluetoothSocket;
}  // namespace device

namespace bluetooth {

// Implementation of Mojo ServerSocket in
// device/bluetooth/public/mojom/adapter.mojom.
// It handles requests to accept incoming connections from remote devices,
// returning a Socket.
// Uses the platform abstraction of //device/bluetooth.
// An instance of this class is constructed by Adapter. When the instance is
// destroyed, the underlying BluetoothSocket is destroyed.
class ServerSocket : public mojom::ServerSocket {
 public:
  explicit ServerSocket(
      scoped_refptr<device::BluetoothSocket> bluetooth_socket);
  ~ServerSocket() override;
  ServerSocket(const ServerSocket&) = delete;
  ServerSocket& operator=(const ServerSocket&) = delete;

  // mojom::ServerSocket:
  void Accept(AcceptCallback callback) override;
  void Disconnect(DisconnectCallback callback) override;

 private:
  void OnAccept(AcceptCallback callback,
                const device::BluetoothDevice* device,
                scoped_refptr<device::BluetoothSocket> socket);
  void OnAcceptError(AcceptCallback callback, const std::string& error_message);

  scoped_refptr<device::BluetoothSocket> server_socket_;

  base::WeakPtrFactory<ServerSocket> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_SERVER_SOCKET_H_

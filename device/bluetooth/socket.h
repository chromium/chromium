// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_SOCKET_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace bluetooth {

// Implementation of Mojo Socket in
// device/bluetooth/public/mojom/adapter.mojom.
// It handles requests to interact with a Socket.
// Uses the platform abstraction of //device/bluetooth.
// An instance of this class is constructed by Adapter and strongly bound to its
// MessagePipe. When the instance is destroyed, the underlying BluetoothSocket
// is destroyed.
class Socket : public mojom::Socket {
 public:
  Socket(scoped_refptr<device::BluetoothSocket> bluetooth_socket,
         mojo::ScopedDataPipeProducerHandle receive_stream,
         mojo::ScopedDataPipeConsumerHandle send_stream);
  ~Socket() override;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  // mojom::Socket:
  void Disconnect(DisconnectCallback callback) override;

 private:
  // "Receiving" in this context means receiving data from |bluetooth_socket_|
  // via BluetoothSocket::Receive() and *writing* it to |receive_stream_|.
  void OnReceiveStreamWritable(MojoResult result);
  void ShutdownReceive();
  void ReceiveMore();
  void OnBluetoothSocketReceive(void* pending_write_buffer,
                                int num_bytes_received,
                                scoped_refptr<net::IOBuffer> io_buffer);
  void OnBluetoothSocketReceiveError(
      device::BluetoothSocket::ErrorReason error_reason,
      const std::string& error_message);

  // "Sending" in this context means *reading* data from |send_stream_| and
  // sending it over the |bluetooth_socket_| via BluetoothSocket::Send().
  void OnSendStreamReadable(MojoResult result);
  void ShutdownSend();
  void SendMore();
  void OnBluetoothSocketSend(int num_bytes_sent);
  void OnBluetoothSocketSendError(const std::string& error_message);

  scoped_refptr<device::BluetoothSocket> bluetooth_socket_;

  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;

  mojo::SimpleWatcher receive_stream_watcher_;
  mojo::SimpleWatcher send_stream_watcher_;

  base::WeakPtrFactory<Socket> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_SOCKET_H_

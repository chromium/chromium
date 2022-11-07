// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_NET_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_NET_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "net/socket/tcp_socket.h"

namespace net {
class IOBuffer;
class IOBufferWithSize;
}  // namespace net

namespace device {

// This class is a base-class for implementations of BluetoothSocket that can
// use net::TCPSocket. All public methods (including the factory method) must
// be called on the UI thread, while underlying socket operations are
// performed on a separate thread.
class BluetoothSocketNet : public BluetoothSocket {
 public:
  BluetoothSocketNet(const BluetoothSocketNet&) = delete;
  BluetoothSocketNet& operator=(const BluetoothSocketNet&) = delete;

  // BluetoothSocket:
  void Disconnect(base::OnceClosure callback) override;
  void Receive(int buffer_size,
               ReceiveCompletionCallback success_callback,
               ReceiveErrorCompletionCallback error_callback) override;
  void Send(scoped_refptr<net::IOBuffer> buffer,
            int buffer_size,
            SendCompletionCallback success_callback,
            ErrorCompletionCallback error_callback) override;

 protected:
  BluetoothSocketNet(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     scoped_refptr<BluetoothSocketThread> socket_thread);
  ~BluetoothSocketNet() override;

  // Resets locally held data after a socket is closed. Default implementation
  // does nothing, subclasses may override.
  virtual void ResetData();

  // Methods for subclasses to obtain the members.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner() const {
    return ui_task_runner_;
  }

  scoped_refptr<BluetoothSocketThread> socket_thread() const {
    return socket_thread_;
  }

  net::TCPSocket* tcp_socket() { return tcp_socket_.get(); }

  void ResetTCPSocket();
  void SetTCPSocket(std::unique_ptr<net::TCPSocket> tcp_socket);

  void PostSuccess(base::OnceClosure callback);
  void PostErrorCompletion(ErrorCompletionCallback callback,
                           const std::string& error);

 private:
  struct WriteRequest {
    WriteRequest();
    ~WriteRequest();

    scoped_refptr<net::IOBuffer> buffer;
    int buffer_size;
    SendCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
  };

  void DoDisconnect(base::OnceClosure callback);
  void DoReceive(int buffer_size,
                 ReceiveCompletionCallback success_callback,
                 ReceiveErrorCompletionCallback error_callback);
  void OnSocketReadComplete(ReceiveCompletionCallback success_callback,
                            ReceiveErrorCompletionCallback error_callback,
                            int read_result);
  void DoSend(scoped_refptr<net::IOBuffer> buffer,
              int buffer_size,
              SendCompletionCallback success_callback,
              ErrorCompletionCallback error_callback);
  void SendFrontWriteRequest();
  void OnSocketWriteComplete(SendCompletionCallback success_callback,
                             ErrorCompletionCallback error_callback,
                             int send_result);

  void PostReceiveCompletion(ReceiveCompletionCallback callback,
                             int io_buffer_size,
                             scoped_refptr<net::IOBuffer> io_buffer);
  void PostReceiveErrorCompletion(ReceiveErrorCompletionCallback callback,
                                  ErrorReason reason,
                                  const std::string& error_message);
  void PostSendCompletion(SendCompletionCallback callback, int bytes_written);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothSocketThread> socket_thread_;

  std::unique_ptr<net::TCPSocket> tcp_socket_;
  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  base::queue<std::unique_ptr<WriteRequest>> write_queue_;
  std::unique_ptr<WriteRequest> pending_write_request_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_NET_H_

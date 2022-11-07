// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

#include <WinSock2.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_socket.h"

namespace device {

class BluetoothAdapter;
class BluetoothDeviceWin;

// The BluetoothSocketWin class implements BluetoothSocket for the Microsoft
// Windows platform.
class BluetoothSocketWin : public BluetoothSocketNet {
 public:
  static scoped_refptr<BluetoothSocketWin> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<BluetoothSocketThread> socket_thread);

  BluetoothSocketWin(const BluetoothSocketWin&) = delete;
  BluetoothSocketWin& operator=(const BluetoothSocketWin&) = delete;

  // Connect to the peer device and calls |success_callback| when the
  // connection has been established successfully. If an error occurs, calls
  // |error_callback| with a system error message.
  void Connect(const BluetoothDeviceWin* device,
               const BluetoothUUID& uuid,
               base::OnceClosure success_callback,
               ErrorCompletionCallback error_callback);

  // Listens using this socket using an RFCOMM service published as UUID |uuid|
  // with Channel |options.channel|, or an automatically allocated Channel if
  // |options.channel| is null. |success_callback| will be called if the service
  // is successfully registered, |error_callback| on failure with a message
  // explaining the cause.
  void Listen(scoped_refptr<BluetoothAdapter> adapter,
              const BluetoothUUID& uuid,
              const BluetoothAdapter::ServiceOptions& options,
              base::OnceClosure success_callback,
              ErrorCompletionCallback error_callback);

  // BluetoothSocketNet:
  void ResetData() override;

  // BluetoothSocket:
  void Accept(AcceptCompletionCallback success_callback,
              ErrorCompletionCallback error_callback) override;

 protected:
  ~BluetoothSocketWin() override;

 private:
  struct ServiceRegData;

  BluetoothSocketWin(scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
                     scoped_refptr<BluetoothSocketThread> socket_thread);

  void DoConnect(base::OnceClosure success_callback,
                 ErrorCompletionCallback error_callback);
  void DoListen(const BluetoothUUID& uuid,
                int rfcomm_channel,
                base::OnceClosure success_callback,
                ErrorCompletionCallback error_callback);
  void DoAccept(AcceptCompletionCallback success_callback,
                ErrorCompletionCallback error_callback);
  void OnAcceptOnSocketThread(AcceptCompletionCallback success_callback,
                              ErrorCompletionCallback error_callback,
                              int accept_result);
  void OnAcceptOnUI(std::unique_ptr<net::TCPSocket> accept_socket,
                    const net::IPEndPoint& peer_address,
                    AcceptCompletionCallback success_callback,
                    ErrorCompletionCallback error_callback);

  std::string device_address_;
  bool supports_rfcomm_;
  uint8_t rfcomm_channel_;
  BTH_ADDR bth_addr_;

  // Data members below are only used when listening.
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<ServiceRegData> service_reg_data_;
  std::unique_ptr<net::TCPSocket> accept_socket_;
  net::IPEndPoint accept_address_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_WIN_H_

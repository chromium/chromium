// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_SOCKET_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_SOCKET_FLOSS_H_

#include <memory>
#include <string>

#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_socket_manager.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothDeviceFloss;

// BluetoothSocketFloss implements BluetoothSocket for platforms that use Floss.
// It must be initialized with a socketpair file descriptor taken via the
// |FlossSocketManager| apis.
//
// This class is not thread-safe, but is only called from the UI thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothSocketFloss
    : public device::BluetoothSocketNet {
 public:
  static scoped_refptr<BluetoothSocketFloss> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);

  BluetoothSocketFloss(const BluetoothSocketFloss&) = delete;
  BluetoothSocketFloss& operator=(const BluetoothSocketFloss&) = delete;

  // Connects this socket to the service on |device| published as UUID |uuid|,
  // the underlying protocol and PSM or Channel is obtained through service
  // discovery. On a successful connection, the socket properties will be
  // updated and |success_callback| called. On failure |error_callback| will be
  // called with a message explaining the cause of the failure.
  //
  // This API only supports RFCOMM at this time.
  virtual void Connect(BluetoothDeviceFloss* device,
                       const FlossSocketManager::Security security,
                       const device::BluetoothUUID& uuid,
                       base::OnceClosure success_callback,
                       ErrorCompletionCallback error_callback);

  // Listens using this socket using a service published on |adapter|. The
  // service is either RFCOMM or L2CAP depending on |socket_type| and published
  // as UUID |uuid| (if RFCOMM). The |service_options| argument is interpreted
  // according to |socket_type|. |success_callback| will be called if the
  // service is successfully registered, |error_callback| on failure with a
  // message explaining the cause.
  //
  // This API supports connect-oriented L2cap (L2capLE) and Rfcomm.
  virtual void Listen(
      scoped_refptr<device::BluetoothAdapter> adapter,
      FlossSocketManager::SocketType socket_type,
      const std::optional<device::BluetoothUUID>& uuid,
      const device::BluetoothAdapter::ServiceOptions& service_options,
      base::OnceClosure success_callback,
      ErrorCompletionCallback error_callback);

  // BluetoothSocket:
  void Disconnect(base::OnceClosure callback) override;
  void Accept(AcceptCompletionCallback success_callback,
              ErrorCompletionCallback error_callback) override;

 protected:
  // Repeating callback that handles when a listening socket becomes ready to
  // accept new connections or when a socket closes.
  void DoConnectionStateChanged(FlossSocketManager::ServerSocketState state,
                                FlossSocketManager::FlossListeningSocket socket,
                                FlossDBusClient::BtifStatus status);

  // Repeating callback that handles when a listening socket accepts a new
  // connection.
  void DoConnectionAccepted(FlossSocketManager::FlossSocket&& socket);

  // Callback that handles completion of a socket listen.
  void CompleteListen(base::OnceClosure success_callback,
                      ErrorCompletionCallback error_callback,
                      DBusResult<FlossDBusClient::BtifStatus> result);

  // Callback that handles completion of a socket connection.
  void CompleteConnect(base::OnceClosure success_callback,
                       ErrorCompletionCallback error_callback,
                       FlossDBusClient::BtifStatus status,
                       std::optional<FlossSocketManager::FlossSocket>&& socket);

  // Callback that handles completion of socket accept.
  void CompleteAccept(DBusResult<FlossDBusClient::BtifStatus> result);

  // Callback that handles completion of socket close.
  void CompleteClose(base::OnceClosure callback,
                     DBusResult<FlossDBusClient::BtifStatus> result);

  // Grabs a connected socket from |connection_request_queue_| and dispatches it
  // via |accept_request_|.
  void CompleteListeningConnect();

  // Complete initializing the socket in the socket thread. Since the underlying
  // implementation is |BluetoothSocketNet|, we need to follow convention and
  // complete the connection in the socket thread. Otherwise, we will get DCHECK
  // failures in |Disconnect|.
  void CompleteConnectionInSocketThread(
      base::OnceClosure success_callback,
      ErrorCompletionCallback error_callback,
      FlossDBusClient::BtifStatus status,
      std::optional<FlossSocketManager::FlossSocket>&& socket);

 private:
  struct AcceptRequest {
    AcceptRequest();
    ~AcceptRequest();

    AcceptCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
  };

  BluetoothSocketFloss(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);

  ~BluetoothSocketFloss() override;

  // Adapter that this socket is created against.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Address of device being connected to. Empty if socket is listening.
  std::string device_address_;

  // Information about a listening socket. Empty if this socket isn't listening.
  std::optional<FlossSocketManager::FlossListeningSocket>
      listening_socket_info_;

  // Is this socket currently accepting? This status reflects what the listening
  // socket is currently doing on the Floss daemon side.
  bool is_accepting_ = false;

  // Hold the listen callbacks and invoke them in DoConnectionStateChanged.
  base::OnceClosure pending_listen_ready_callback_;
  base::OnceClosure pending_listen_close_callback_;

  // Information about a connecting socket. Check |is_valid| before using.
  FlossSocketManager::FlossSocket connecting_socket_info_;

  // Socket is ready to accept the next request using callbacks here.
  std::unique_ptr<AcceptRequest> accept_request_;

  // An accepted socket that is pending connection.
  scoped_refptr<BluetoothSocketFloss> pending_accept_socket_;

  // We need to cancel all socket tasks so that the ui thread both creates and
  // destroys weak pointers. Otherwise, we run into a DCHECK.
  base::CancelableTaskTracker socket_task_tracker_;

  // After a connection is accepted, store the connection until it's ready to be
  // consumed.
  base::queue<FlossSocketManager::FlossSocket> connection_request_queue_;

  base::WeakPtrFactory<BluetoothSocketFloss> weak_ptr_factory_{this};
};
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_SOCKET_FLOSS_H_

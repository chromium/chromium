// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_socket_floss.h"

#include "base/task/sequenced_task_runner.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_socket_manager.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace floss {
namespace {
const char kInvalidSocketType[] = "Invalid Socket Type";
const char kInvalidUUID[] = "Invalid UUID";
const char kSocketNotListening[] = "Socket is not listening";
const char kSocketListenFailed[] = "Socket failed to listen";
const char kSocketAcceptFailed[] = "Socket failed to accept";

const char kSocketFailedToConnect[] = "Socket failed to connect";
const char kSocketInvalidFd[] = "Socket has invalid fd";
const char kSocketAlreadyConnected[] = "Socket already connected";
const char kSocketErrorAdopting[] = "Socket error during adoption";
}  // namespace

// static
scoped_refptr<BluetoothSocketFloss> BluetoothSocketFloss::CreateBluetoothSocket(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread) {
  DCHECK(ui_task_runner->RunsTasksInCurrentSequence());

  return base::WrapRefCounted(
      new BluetoothSocketFloss(ui_task_runner, socket_thread));
}

BluetoothSocketFloss::AcceptRequest::AcceptRequest() = default;
BluetoothSocketFloss::AcceptRequest::~AcceptRequest() = default;

BluetoothSocketFloss::BluetoothSocketFloss(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<device::BluetoothSocketThread> socket_thread)
    : BluetoothSocketNet(ui_task_runner, socket_thread) {}

BluetoothSocketFloss::~BluetoothSocketFloss() = default;

void BluetoothSocketFloss::Connect(BluetoothDeviceFloss* device,
                                   const FlossSocketManager::Security security,
                                   const device::BluetoothUUID& uuid,
                                   base::OnceClosure success_callback,
                                   ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(device);

  if (!uuid.IsValid()) {
    std::move(error_callback).Run(kInvalidUUID);
    return;
  }

  adapter_ = base::WrapRefCounted(device->GetAdapter());
  device_address_ = device->GetAddress();

  FlossDBusManager::Get()->GetSocketManager()->ConnectUsingRfcomm(
      device->AsFlossDeviceId(), uuid, security,
      base::BindOnce(&BluetoothSocketFloss::CompleteConnect,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void BluetoothSocketFloss::Listen(
    scoped_refptr<device::BluetoothAdapter> adapter,
    FlossSocketManager::SocketType socket_type,
    const std::optional<device::BluetoothUUID>& uuid,
    const device::BluetoothAdapter::ServiceOptions& service_options,
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // TODO(abps) - Figure out whether we can accept channel/psm from options.
  FlossSocketManager::Security security =
      (service_options.require_authentication &&
       *service_options.require_authentication)
          ? FlossSocketManager::Security::kSecure
          : FlossSocketManager::Security::kInsecure;

  if (!uuid->IsValid()) {
    std::move(error_callback).Run(kInvalidUUID);
    return;
  }

  if (socket_type != FlossSocketManager::SocketType::kL2cap &&
      socket_type != FlossSocketManager::SocketType::kRfcomm) {
    std::move(error_callback).Run(kInvalidSocketType);
    return;
  }

  adapter_ = adapter;

  ResponseCallback<FlossDBusClient::BtifStatus> callback = base::BindOnce(
      &BluetoothSocketFloss::CompleteListen, weak_ptr_factory_.GetWeakPtr(),
      std::move(success_callback), std::move(error_callback));

  FlossSocketManager::ConnectionStateChanged state_change =
      base::BindRepeating(&BluetoothSocketFloss::DoConnectionStateChanged,
                          weak_ptr_factory_.GetWeakPtr());

  FlossSocketManager::ConnectionAccepted accepted =
      base::BindRepeating(&BluetoothSocketFloss::DoConnectionAccepted,
                          weak_ptr_factory_.GetWeakPtr());

  if (socket_type == FlossSocketManager::SocketType::kL2cap) {
    FlossDBusManager::Get()->GetSocketManager()->ListenUsingL2cap(
        security, std::move(callback), state_change, accepted);
  } else if (socket_type == FlossSocketManager::SocketType::kRfcomm) {
    DCHECK(uuid);
    std::string name;
    if (service_options.name) {
      name = *service_options.name;
    }

    FlossDBusManager::Get()->GetSocketManager()->ListenUsingRfcomm(
        name, *uuid, security, std::move(callback), state_change, accepted);
  }
}

void BluetoothSocketFloss::Disconnect(base::OnceClosure callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Cancel any tasks pending on the socket thread.
  socket_task_tracker_.TryCancelAll();

  // If this is a connecting socket, simply close self.
  if (connecting_socket_info_.is_valid()) {
    BluetoothSocketNet::Disconnect(std::move(callback));

    // Note: Adapter needs to be cleared here or BluetoothSocketNet will be
    // holding a weak pointer and try to destroy a scoped_refptr in a the socket
    // thread.
    adapter_ = nullptr;
    connecting_socket_info_.id = FlossSocketManager::kInvalidSocketId;
  }
  // Close the socket manager instance.
  else if (listening_socket_info_) {
    FlossDBusManager::Get()->GetSocketManager()->Close(
        listening_socket_info_->id,
        base::BindOnce(&BluetoothSocketFloss::CompleteClose,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    listening_socket_info_ = std::nullopt;
    pending_accept_socket_.reset();
  } else {
    if (pending_listen_ready_callback_) {
      LOG(WARNING) << "Disconnecting listening socket before it is ready, "
                   << "which may cause leaking!";
      pending_listen_ready_callback_.Reset();
    } else {
      LOG(WARNING) << "Disconnecting socket (" << this << ") with no info";
    }
  }

  // If there is a pending accept, clear it.
  if (accept_request_) {
    std::move(accept_request_->error_callback)
        .Run(net::ErrorToString(net::ERR_CONNECTION_CLOSED));
    accept_request_.reset(nullptr);
  }

  // Just dropping the queue is sufficient. No need to send anything back
  // because these are already accepted connections. Queue has no |clear|
  // function so just assign a new empty queue.
  connection_request_queue_ = {};
}

void BluetoothSocketFloss::Accept(AcceptCompletionCallback success_callback,
                                  ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (!listening_socket_info_) {
    std::move(error_callback).Run(kSocketNotListening);
    return;
  }

  // Accept is already pending.
  if (accept_request_.get()) {
    std::move(error_callback).Run(net::ErrorToString(net::ERR_IO_PENDING));
    return;
  }

  accept_request_ = std::make_unique<AcceptRequest>();
  accept_request_->success_callback = std::move(success_callback);
  accept_request_->error_callback = std::move(error_callback);

  // If there's any already ready connections, dispatch right away.
  if (connection_request_queue_.size() >= 1) {
    CompleteListeningConnect();
  }

  // If this socket is currently not accepting at the platform level, flip this
  // to accepting.
  if (!is_accepting_) {
    FlossDBusManager::Get()->GetSocketManager()->Accept(
        listening_socket_info_->id, std::nullopt,
        base::BindOnce(&BluetoothSocketFloss::CompleteAccept,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BluetoothSocketFloss::DoConnectionStateChanged(
    FlossSocketManager::ServerSocketState state,
    FlossSocketManager::FlossListeningSocket socket,
    FlossDBusClient::BtifStatus status) {
  // If we don't already have socket info, store it.
  if (!listening_socket_info_) {
    listening_socket_info_ = socket;
  }

  // Since we've received the socket info, we are ready for |Accept| and
  // |Disconnect| now.
  if (pending_listen_ready_callback_) {
    std::move(pending_listen_ready_callback_).Run();
  }

  // Every time we get a socket state update, the socket gets reset to a not
  // accepting state. Mark our internal state to match that.
  is_accepting_ = false;

  // We also always want to be accepting and queue up connections here to be
  // consumed when in the ready state.
  if (state == FlossSocketManager::ServerSocketState::kReady &&
      status == FlossDBusClient::BtifStatus::kSuccess) {
    FlossDBusManager::Get()->GetSocketManager()->Accept(
        listening_socket_info_->id, std::nullopt,
        base::BindOnce(&BluetoothSocketFloss::CompleteAccept,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (state == FlossSocketManager::ServerSocketState::kClosed) {
    if (pending_listen_close_callback_) {
      std::move(pending_listen_close_callback_).Run();
    } else {
      LOG(WARNING) << "Server socket with uuid "
                   << listening_socket_info_->uuid.value()
                   << " closed unexpectedly";
    }
    return;
  }

  if (status != FlossDBusClient::BtifStatus::kSuccess && accept_request_) {
    std::move(accept_request_->error_callback)
        .Run(net::ErrorToString(net::ERR_CONNECTION_FAILED));
    accept_request_.reset(nullptr);
    return;
  }
}

void BluetoothSocketFloss::DoConnectionAccepted(
    FlossSocketManager::FlossSocket&& socket) {
  // Queue dispatch of new connection.
  connection_request_queue_.push(std::move(socket));

  // If there's an accept ready, dispatch immediately.
  if (accept_request_) {
    CompleteListeningConnect();
  }
}

void BluetoothSocketFloss::CompleteListen(
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback,
    DBusResult<FlossDBusClient::BtifStatus> result) {
  if (!result.has_value()) {
    std::move(error_callback).Run(result.error().ToString());
    return;
  }

  if (*result != FlossDBusClient::BtifStatus::kSuccess) {
    std::move(error_callback).Run(kSocketListenFailed);
    return;
  }

  // On success, the callbacks will be invoked in DoConnectionStateChanged.
  pending_listen_ready_callback_ = std::move(success_callback);
}

void BluetoothSocketFloss::CompleteConnect(
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback,
    FlossDBusClient::BtifStatus status,
    std::optional<FlossSocketManager::FlossSocket>&& socket) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (status == FlossDBusClient::BtifStatus::kSuccess && socket) {
    device::BluetoothDevice* device = adapter_->GetDevice(device_address_);
    if (device) {
      // Set discovery completed here because a connected socket implies it.
      // This is necessary for the Mojo Adapter implementation because it always
      // waits for the discovery complete before requesting the next connection.
      device->SetGattServicesDiscoveryComplete(true);
    } else {
      LOG(ERROR) << device_address_
                 << ": Outgoing socket connected to an unknown device";
    }
  }

  socket_task_tracker_.PostTask(
      socket_thread()->task_runner().get(), FROM_HERE,
      base::BindOnce(&BluetoothSocketFloss::CompleteConnectionInSocketThread,
                     this, std::move(success_callback),
                     std::move(error_callback), status, std::move(socket)));
}

void BluetoothSocketFloss::CompleteConnectionInSocketThread(
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback,
    FlossDBusClient::BtifStatus status,
    std::optional<FlossSocketManager::FlossSocket>&& socket) {
  DCHECK(socket_thread()->task_runner()->RunsTasksInCurrentSequence());

  if (status != FlossDBusClient::BtifStatus::kSuccess || !socket) {
    LOG(ERROR) << device_address_ << ": Socket failed connection: "
               << static_cast<uint32_t>(status);
    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback), kSocketFailedToConnect));
    return;
  }

  connecting_socket_info_ = std::move(*socket);

  if (!connecting_socket_info_.fd || !connecting_socket_info_.fd->is_valid()) {
    LOG(WARNING) << device_address_
                 << ": Invalid file descriptor received from Bluetooth daemon";
    ui_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback), kSocketInvalidFd));
    return;
  }

  if (tcp_socket()) {
    LOG(WARNING) << device_address_ << ": Already connected";
    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback), kSocketAlreadyConnected));
    return;
  }

  ResetTCPSocket();

  int net_result = tcp_socket()->AdoptConnectedSocket(
      connecting_socket_info_.fd->release(), net::IPEndPoint());

  if (net_result != net::OK) {
    LOG(WARNING) << device_address_ << ": Error adopting socket: "
                 << std::string(net::ErrorToString(net_result));
    ui_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback), kSocketErrorAdopting));
    return;
  }

  ui_task_runner()->PostTask(FROM_HERE, std::move(success_callback));
}

void BluetoothSocketFloss::CompleteAccept(
    DBusResult<FlossDBusClient::BtifStatus> result) {
  if (!result.has_value() && accept_request_) {
    std::move(accept_request_->error_callback).Run(result.error().ToString());
    accept_request_.reset(nullptr);
    return;
  } else if (!result.has_value()) {
    return;
  }

  if (*result != FlossDBusClient::BtifStatus::kSuccess && accept_request_) {
    LOG(WARNING) << "Failed to listen on socket with uuid "
                 << listening_socket_info_->uuid.value();
    std::move(accept_request_->error_callback).Run(kSocketAcceptFailed);
    accept_request_.reset(nullptr);
    return;
  }

  if (*result == FlossDBusClient::BtifStatus::kSuccess) {
    is_accepting_ = true;
  }
}

void BluetoothSocketFloss::CompleteClose(
    base::OnceClosure callback,
    DBusResult<FlossDBusClient::BtifStatus> result) {
  if (result.has_value()) {
    DVLOG(1) << "Result of closing socket = " << static_cast<uint32_t>(*result);
    if (*result == FlossDBusClient::BtifStatus::kSuccess) {
      // If |Close| succeeded, run the callback in DoConnectionStateChanged.
      pending_listen_close_callback_ = std::move(callback);
      return;
    }
  }

  is_accepting_ = false;
  std::move(callback).Run();
}

void BluetoothSocketFloss::CompleteListeningConnect() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (!listening_socket_info_) {
    LOG(ERROR) << "Tried to complete connect on socket that isn't listening.";
    return;
  }

  if (connection_request_queue_.size() == 0) {
    LOG(ERROR) << "Tried to complete connect with no pending connections.";
    return;
  }

  if (!accept_request_) {
    LOG(ERROR) << "Tried to complete connect with no pending accept.";
    return;
  }

  pending_accept_socket_ = BluetoothSocketFloss::CreateBluetoothSocket(
      ui_task_runner(), socket_thread());

  std::optional<FlossSocketManager::FlossSocket> sock(
      std::move(connection_request_queue_.front()));
  connection_request_queue_.pop();

  device::BluetoothDevice* device =
      adapter_->GetDevice(sock->remote_device.address);

  if (device) {
    // Set discovery completed here because a connected socket implies it.
    // This is necessary for the Mojo Adapter implementation because it always
    // waits for the discovery complete before requesting the next connection.
    device->SetGattServicesDiscoveryComplete(true);
  } else {
    LOG(ERROR) << device_address_
               << ": Incoming socket connected to an unknown device";
  }

  socket_task_tracker_.PostTask(
      socket_thread()->task_runner().get(), FROM_HERE,
      base::BindOnce(
          &BluetoothSocketFloss::CompleteConnectionInSocketThread,
          pending_accept_socket_.get(),
          base::BindOnce(std::move(accept_request_->success_callback), device,
                         pending_accept_socket_),
          std::move(accept_request_->error_callback),
          FlossDBusClient::BtifStatus::kSuccess, std::move(sock)));

  // The last accept request has been consumed by the socket above.
  accept_request_.reset(nullptr);
}

}  // namespace floss

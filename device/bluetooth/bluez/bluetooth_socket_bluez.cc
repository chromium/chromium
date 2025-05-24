// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_socket_bluez.h"

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_adapter_profile_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothSocketThread;
using device::BluetoothUUID;

namespace {

const char kAcceptFailed[] = "Failed to accept connection.";
const char kInvalidUUID[] = "Invalid UUID";
const char kSocketNotListening[] = "Socket is not listening.";

}  // namespace

namespace bluez {

// static
scoped_refptr<BluetoothSocketBlueZ> BluetoothSocketBlueZ::CreateBluetoothSocket(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread) {
  DCHECK(ui_task_runner->RunsTasksInCurrentSequence());

  return base::WrapRefCounted(
      new BluetoothSocketBlueZ(ui_task_runner, socket_thread));
}

BluetoothSocketBlueZ::AcceptRequest::AcceptRequest() = default;

BluetoothSocketBlueZ::AcceptRequest::~AcceptRequest() = default;

BluetoothSocketBlueZ::ConnectionRequest::ConnectionRequest()
    : accepting(false), cancelled(false) {}

BluetoothSocketBlueZ::ConnectionRequest::~ConnectionRequest() = default;

BluetoothSocketBlueZ::BluetoothSocketBlueZ(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThread> socket_thread)
    : BluetoothSocketNet(ui_task_runner, socket_thread), profile_(nullptr) {}

BluetoothSocketBlueZ::~BluetoothSocketBlueZ() {
  DCHECK(!profile_);

  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }
}

void BluetoothSocketBlueZ::Connect(const BluetoothDeviceBlueZ* device,
                                   const BluetoothUUID& uuid,
                                   SecurityLevel security_level,
                                   base::OnceClosure success_callback,
                                   ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!profile_);

  if (!uuid.IsValid()) {
    std::move(error_callback).Run(kInvalidUUID);
    return;
  }

  device_address_ = device->GetAddress();
  device_path_ = device->object_path();
  uuid_ = uuid;
  options_ = std::make_unique<bluez::BluetoothProfileManagerClient::Options>();
  if (security_level == SECURITY_LEVEL_LOW)
    options_->require_authentication = std::make_unique<bool>(false);

  adapter_ = device->adapter();

  RegisterProfile(device->adapter(), std::move(success_callback),
                  std::move(error_callback));
}

void BluetoothSocketBlueZ::Listen(
    scoped_refptr<BluetoothAdapter> adapter,
    SocketType socket_type,
    const BluetoothUUID& uuid,
    const BluetoothAdapter::ServiceOptions& service_options,
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!profile_);

  if (!uuid.IsValid()) {
    std::move(error_callback).Run(kInvalidUUID);
    return;
  }

  adapter_ = adapter;
  adapter_->AddObserver(this);

  uuid_ = uuid;
  options_ = std::make_unique<bluez::BluetoothProfileManagerClient::Options>();
  if (service_options.name)
    options_->name = std::make_unique<std::string>(*service_options.name);

  switch (socket_type) {
    case kRfcomm:
      options_->channel = std::make_unique<uint16_t>(
          service_options.channel ? *service_options.channel : 0);
      break;
    case kL2cap:
      options_->psm = std::make_unique<uint16_t>(
          service_options.psm ? *service_options.psm : 0);
      break;
    default:
      NOTREACHED();
  }

  if (service_options.require_authentication) {
    options_->require_authentication =
        std::make_unique<bool>(*service_options.require_authentication);
  }

  RegisterProfile(static_cast<BluetoothAdapterBlueZ*>(adapter.get()),
                  std::move(success_callback), std::move(error_callback));
}

void BluetoothSocketBlueZ::Disconnect(base::OnceClosure callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (profile_)
    UnregisterProfile();

  // In the case below, where an asynchronous task gets posted on the socket
  // thread in BluetoothSocketNet::Close, a reference will be held to this
  // socket by the callback. This may cause the BluetoothAdapter to outlive
  // BluezDBusManager during shutdown if that callback executes too late.
  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = nullptr;
  }

  if (!device_path_.value().empty()) {
    BluetoothSocketNet::Disconnect(std::move(callback));
    return;
  }

  if (accept_request_) {
    std::move(accept_request_->error_callback)
        .Run(net::ErrorToString(net::ERR_CONNECTION_CLOSED));
    accept_request_.reset(nullptr);
  }

  while (connection_request_queue_.size() > 0) {
    std::move(connection_request_queue_.front()->callback).Run(REJECTED);
    connection_request_queue_.pop();
  }
  std::move(callback).Run();
}

void BluetoothSocketBlueZ::Accept(AcceptCompletionCallback success_callback,
                                  ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  if (!device_path_.value().empty()) {
    std::move(error_callback).Run(kSocketNotListening);
    return;
  }

  // Only one pending accept at a time
  if (accept_request_.get()) {
    std::move(error_callback).Run(net::ErrorToString(net::ERR_IO_PENDING));
    return;
  }

  accept_request_ = std::make_unique<AcceptRequest>();
  accept_request_->success_callback = std::move(success_callback);
  accept_request_->error_callback = std::move(error_callback);

  if (connection_request_queue_.size() >= 1) {
    AcceptConnectionRequest();
  }
}

void BluetoothSocketBlueZ::RegisterProfile(
    BluetoothAdapterBlueZ* adapter,
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!profile_);
  DCHECK(adapter);

  // If the adapter is not present, this is a listening socket and the
  // adapter isn't running yet.  Report success and carry on;
  // the profile will be registered when the daemon becomes available.
  if (!adapter->IsPresent()) {
    DVLOG(1) << uuid_.canonical_value() << " on " << device_path_.value()
             << ": Delaying profile registration.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(success_callback));
    return;
  }

  DVLOG(1) << uuid_.canonical_value() << " on " << device_path_.value()
           << ": Acquiring profile.";

  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));
  adapter->UseProfile(
      uuid_, device_path_, *options_, this,
      base::BindOnce(&BluetoothSocketBlueZ::OnRegisterProfile, this,
                     std::move(success_callback),
                     std::move(split_error_callback.first)),
      base::BindOnce(&BluetoothSocketBlueZ::OnRegisterProfileError, this,
                     std::move(split_error_callback.second)));
}

void BluetoothSocketBlueZ::OnRegisterProfile(
    base::OnceClosure success_callback,
    ErrorCompletionCallback error_callback,
    BluetoothAdapterProfileBlueZ* profile) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!profile_);

  profile_ = profile;

  if (device_path_.value().empty()) {
    DVLOG(1) << uuid_.canonical_value() << ": Profile registered.";
    std::move(success_callback).Run();
    return;
  }

  DVLOG(1) << uuid_.canonical_value() << ": Got profile, connecting to "
           << device_path_.value();

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->ConnectProfile(
      device_path_, uuid_.canonical_value(),
      base::BindOnce(&BluetoothSocketBlueZ::OnConnectProfile, this,
                     std::move(success_callback)),
      base::BindOnce(&BluetoothSocketBlueZ::OnConnectProfileError, this,
                     std::move(error_callback)));
}

void BluetoothSocketBlueZ::OnRegisterProfileError(
    ErrorCompletionCallback error_callback,
    const std::string& error_message) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  LOG(WARNING) << uuid_.canonical_value()
               << ": Failed to register profile: " << error_message;
  std::move(error_callback).Run(error_message);
}

void BluetoothSocketBlueZ::OnConnectProfile(
    base::OnceClosure success_callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(profile_);

  DVLOG(1) << profile_->object_path().value() << ": Profile connected.";
  UnregisterProfile();
  std::move(success_callback).Run();
}

void BluetoothSocketBlueZ::OnConnectProfileError(
    ErrorCompletionCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(profile_);

  const std::string error = base::StrCat({error_name, ": ", error_message});
  LOG(WARNING) << profile_->object_path().value()
               << ": Failed to connect profile: " << error;
  UnregisterProfile();
  std::move(error_callback).Run(error);
}

void BluetoothSocketBlueZ::AdapterPresentChanged(BluetoothAdapter* adapter,
                                                 bool present) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  // Some boards cut power to their Bluetooth chip during suspension, which
  // leads to a AdapterPresentChanged(present=false) event on wake (quickly
  // followed by an AdapterPresentChanged(present=true) event).
  // This 'present=false' event can occur in the middle of
  // BluetoothSocketBlueZ's asynchronous process of acquiring |profile_|. That
  // means the following 2 surprising edge-cases can occur on a few select
  // boards:
  //   1) |profile_| may not be initialized when a 'present=false' event occurs.
  //      We must check |profile_| before calling UnregisterProfile().
  //   2) |profile_| may already be initialized when a 'present=true' event
  //      occurs. We must check |profile_| before attempting to redundantly
  //      initialize it via BluetoothAdapterBlueZ::UseProfile().

  if (!present) {
    // Edge-case (1) described above.
    if (profile_) {
      // Adapter removed, we can't use the profile anymore.
      UnregisterProfile();
    }
    return;
  }

  // Edge-case (2) described above.
  if (profile_) {
    return;
  }

  DVLOG(1) << uuid_.canonical_value() << " on " << device_path_.value()
           << ": Acquiring profile.";

  static_cast<BluetoothAdapterBlueZ*>(adapter)->UseProfile(
      uuid_, device_path_, *options_, this,
      base::BindOnce(&BluetoothSocketBlueZ::OnInternalRegisterProfile, this),
      base::BindOnce(&BluetoothSocketBlueZ::OnInternalRegisterProfileError,
                     this));
}

void BluetoothSocketBlueZ::OnInternalRegisterProfile(
    BluetoothAdapterProfileBlueZ* profile) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!profile_);

  profile_ = profile;

  DVLOG(1) << uuid_.canonical_value() << ": Profile re-registered";
}

void BluetoothSocketBlueZ::OnInternalRegisterProfileError(
    const std::string& error_message) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  LOG(WARNING) << "Failed to re-register profile: " << error_message;
}

void BluetoothSocketBlueZ::Released() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(profile_);

  DVLOG(1) << profile_->object_path().value() << ": Release";
}

void BluetoothSocketBlueZ::NewConnection(
    const dbus::ObjectPath& device_path,
    base::ScopedFD fd,
    const bluez::BluetoothProfileServiceProvider::Delegate::Options& options,
    ConfirmationCallback callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());

  DVLOG(1) << uuid_.canonical_value()
           << ": New connection from device: " << device_path.value();

  if (!device_path_.value().empty()) {
    DCHECK(device_path_ == device_path);

    socket_thread()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BluetoothSocketBlueZ::DoNewConnection, this,
                                  device_path_, std::move(fd), options,
                                  std::move(callback)));
  } else {
    auto request = std::make_unique<ConnectionRequest>();
    request->device_path = device_path;
    request->fd = std::move(fd);
    request->options = options;
    request->callback = std::move(callback);

    connection_request_queue_.push(std::move(request));
    DVLOG(1) << uuid_.canonical_value() << ": Connection is now pending.";
    if (accept_request_) {
      AcceptConnectionRequest();
    }
  }
}

void BluetoothSocketBlueZ::RequestDisconnection(
    const dbus::ObjectPath& device_path,
    ConfirmationCallback callback) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(profile_);

  DVLOG(1) << profile_->object_path().value() << ": Request disconnection";
  std::move(callback).Run(SUCCESS);
}

void BluetoothSocketBlueZ::Cancel() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(profile_);

  DVLOG(1) << profile_->object_path().value() << ": Cancel";

  if (connection_request_queue_.empty())
    return;

  // If the front request is being accepted mark it as cancelled, otherwise
  // just pop it from the queue.
  ConnectionRequest* request = connection_request_queue_.front().get();
  if (!request->accepting) {
    request->cancelled = true;
  } else {
    connection_request_queue_.pop();
  }
}

void BluetoothSocketBlueZ::AcceptConnectionRequest() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(accept_request_.get());
  DCHECK(connection_request_queue_.size() >= 1);
  DCHECK(profile_);

  DVLOG(1) << profile_->object_path().value()
           << ": Accepting pending connection.";

  ConnectionRequest* request = connection_request_queue_.front().get();
  request->accepting = true;

  BluetoothDeviceBlueZ* device =
      static_cast<BluetoothAdapterBlueZ*>(adapter_.get())
          ->GetDeviceWithPath(request->device_path);
  DCHECK(device);

  scoped_refptr<BluetoothSocketBlueZ> client_socket =
      BluetoothSocketBlueZ::CreateBluetoothSocket(ui_task_runner(),
                                                  socket_thread());

  client_socket->device_address_ = device->GetAddress();
  client_socket->device_path_ = request->device_path;
  client_socket->uuid_ = uuid_;

  socket_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BluetoothSocketBlueZ::DoNewConnection, client_socket,
          request->device_path, std::move(request->fd), request->options,
          base::BindOnce(&BluetoothSocketBlueZ::OnNewConnection, this,
                         client_socket, std::move(request->callback))));
}

void BluetoothSocketBlueZ::DoNewConnection(
    const dbus::ObjectPath& device_path,
    base::ScopedFD fd,
    const bluez::BluetoothProfileServiceProvider::Delegate::Options& options,
    ConfirmationCallback callback) {
  DCHECK(socket_thread()->task_runner()->RunsTasksInCurrentSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!fd.is_valid()) {
    LOG(WARNING) << uuid_.canonical_value() << " :" << fd.get()
                 << ": Invalid file descriptor received from Bluetooth Daemon.";
    ui_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), REJECTED));
    return;
  }

  if (tcp_socket()) {
    LOG(WARNING) << uuid_.canonical_value() << ": Already connected";
    ui_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), REJECTED));
    return;
  }

  ResetTCPSocket();

  // Note: We don't have a meaningful |IPEndPoint|, but that is ok since the
  // TCPSocket implementation does not actually require one.
  int net_result =
      tcp_socket()->AdoptConnectedSocket(fd.release(), net::IPEndPoint());
  if (net_result != net::OK) {
    LOG(WARNING) << uuid_.canonical_value() << ": Error adopting socket: "
                 << std::string(net::ErrorToString(net_result));
    ui_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(callback), REJECTED));
    return;
  }
  ui_task_runner()->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback), SUCCESS));
}

void BluetoothSocketBlueZ::OnNewConnection(
    scoped_refptr<BluetoothSocket> socket,
    ConfirmationCallback callback,
    Status status) {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(accept_request_.get());
  DCHECK(connection_request_queue_.size() >= 1);

  std::unique_ptr<ConnectionRequest> request =
      std::move(connection_request_queue_.front());
  if (status == SUCCESS && !request->cancelled) {
    BluetoothDeviceBlueZ* device =
        static_cast<BluetoothAdapterBlueZ*>(adapter_.get())
            ->GetDeviceWithPath(request->device_path);
    DCHECK(device);

    std::move(accept_request_->success_callback).Run(device, socket);
  } else {
    std::move(accept_request_->error_callback).Run(kAcceptFailed);
  }

  accept_request_.reset(nullptr);
  connection_request_queue_.pop();

  std::move(callback).Run(status);
}

void BluetoothSocketBlueZ::UnregisterProfile() {
  DCHECK(ui_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(profile_);

  DVLOG(1) << profile_->object_path().value() << ": Release profile";

  static_cast<BluetoothAdapterBlueZ*>(adapter_.get())
      ->ReleaseProfile(device_path_, profile_);
  profile_ = nullptr;
}

}  // namespace bluez

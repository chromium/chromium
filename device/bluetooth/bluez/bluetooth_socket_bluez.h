// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SOCKET_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SOCKET_BLUEZ_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_net.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

class BluetoothDeviceBlueZ;
class BluetoothAdapterBlueZ;
class BluetoothAdapterProfileBlueZ;

// The BluetoothSocketBlueZ class implements BluetoothSocket for platforms that
// use BlueZ.
//
// This class is not thread-safe, but is only called from the UI thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothSocketBlueZ
    : public device::BluetoothSocketNet,
      public device::BluetoothAdapter::Observer,
      public bluez::BluetoothProfileServiceProvider::Delegate {
 public:
  enum SecurityLevel { SECURITY_LEVEL_LOW, SECURITY_LEVEL_MEDIUM };

  static scoped_refptr<BluetoothSocketBlueZ> CreateBluetoothSocket(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);

  BluetoothSocketBlueZ(const BluetoothSocketBlueZ&) = delete;
  BluetoothSocketBlueZ& operator=(const BluetoothSocketBlueZ&) = delete;

  // Connects this socket to the service on |device| published as UUID |uuid|,
  // the underlying protocol and PSM or Channel is obtained through service
  // discovery. On a successful connection the socket properties will be updated
  // and |success_callback| called. On failure |error_callback| will be called
  // with a message explaining the cause of the failure.
  virtual void Connect(const BluetoothDeviceBlueZ* device,
                       const device::BluetoothUUID& uuid,
                       SecurityLevel security_level,
                       base::OnceClosure success_callback,
                       ErrorCompletionCallback error_callback);

  // Listens using this socket using a service published on |adapter|. The
  // service is either RFCOMM or L2CAP depending on |socket_type| and published
  // as UUID |uuid|. The |service_options| argument is interpreted according to
  // |socket_type|. |success_callback| will be called if the service is
  // successfully registered, |error_callback| on failure with a message
  // explaining the cause.
  enum SocketType { kRfcomm, kL2cap };
  virtual void Listen(
      scoped_refptr<device::BluetoothAdapter> adapter,
      SocketType socket_type,
      const device::BluetoothUUID& uuid,
      const device::BluetoothAdapter::ServiceOptions& service_options,
      base::OnceClosure success_callback,
      ErrorCompletionCallback error_callback);

  // BluetoothSocket:
  void Disconnect(base::OnceClosure callback) override;
  void Accept(AcceptCompletionCallback success_callback,
              ErrorCompletionCallback error_callback) override;

 protected:
  ~BluetoothSocketBlueZ() override;

 private:
  BluetoothSocketBlueZ(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<device::BluetoothSocketThread> socket_thread);

  // Register the underlying profile client object with the Bluetooth Daemon.
  void RegisterProfile(BluetoothAdapterBlueZ* adapter,
                       base::OnceClosure success_callback,
                       ErrorCompletionCallback error_callback);
  void OnRegisterProfile(base::OnceClosure success_callback,
                         ErrorCompletionCallback error_callback,
                         BluetoothAdapterProfileBlueZ* profile);
  void OnRegisterProfileError(ErrorCompletionCallback error_callback,
                              const std::string& error_message);

  // Called by dbus:: on completion of the ConnectProfile() method.
  void OnConnectProfile(base::OnceClosure success_callback);
  void OnConnectProfileError(ErrorCompletionCallback error_callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;

  // Called by dbus:: on completion of the RegisterProfile() method call
  // triggered as a result of the adapter becoming present again.
  void OnInternalRegisterProfile(BluetoothAdapterProfileBlueZ* profile);
  void OnInternalRegisterProfileError(const std::string& error_message);

  // bluez::BluetoothProfileServiceProvider::Delegate:
  void Released() override;
  void NewConnection(
      const dbus::ObjectPath& device_path,
      base::ScopedFD fd,
      const bluez::BluetoothProfileServiceProvider::Delegate::Options& options,
      ConfirmationCallback callback) override;
  void RequestDisconnection(const dbus::ObjectPath& device_path,
                            ConfirmationCallback callback) override;
  void Cancel() override;

  // Method run to accept a single incoming connection.
  void AcceptConnectionRequest();

  // Method run on the socket thread to validate the file descriptor of a new
  // connection and set up the underlying net::TCPSocket() for it.
  void DoNewConnection(
      const dbus::ObjectPath& device_path,
      base::ScopedFD fd,
      const bluez::BluetoothProfileServiceProvider::Delegate::Options& options,
      ConfirmationCallback callback);

  // Method run on the UI thread after a new connection has been accepted and
  // a socket allocated in |socket|. Takes care of calling the Accept()
  // callback and |callback| with the right arguments based on |status|.
  void OnNewConnection(scoped_refptr<BluetoothSocket> socket,
                       ConfirmationCallback callback,
                       Status status);

  // Unregisters this socket's usage of the Bluetooth profile which cleans up
  // the profile if no one is using it.
  void UnregisterProfile();

  // Adapter the profile is registered against
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Address and D-Bus object path of the device being connected to, empty and
  // ignored if the socket is listening.
  std::string device_address_;
  dbus::ObjectPath device_path_;

  // UUID of the profile being connected to, or listening on.
  device::BluetoothUUID uuid_;

  // Copy of the profile options used for registering the profile.
  std::unique_ptr<bluez::BluetoothProfileManagerClient::Options> options_;

  // The profile registered with the adapter for this socket.
  raw_ptr<BluetoothAdapterProfileBlueZ> profile_;

  // Pending request to an Accept() call.
  struct AcceptRequest {
    AcceptRequest();
    ~AcceptRequest();

    AcceptCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
  };
  std::unique_ptr<AcceptRequest> accept_request_;

  // Queue of incoming connection requests.
  struct ConnectionRequest {
    ConnectionRequest();
    ~ConnectionRequest();

    dbus::ObjectPath device_path;
    base::ScopedFD fd;
    bluez::BluetoothProfileServiceProvider::Delegate::Options options;
    ConfirmationCallback callback;
    bool accepting;
    bool cancelled;
  };
  base::queue<std::unique_ptr<ConnectionRequest>> connection_request_queue_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SOCKET_BLUEZ_H_

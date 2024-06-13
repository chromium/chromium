// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_

#import <IOBluetooth/IOBluetooth.h>
#import <IOKit/IOReturn.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

@class BluetoothRfcommConnectionListener;
@class BluetoothL2capConnectionListener;
@class SDPQueryListener;

namespace net {
class IOBuffer;
class IOBufferWithSize;
}

namespace device {

class BluetoothAdapterMac;
class BluetoothChannelMac;

// Implements the BluetoothSocket class for the macOS platform.
class BluetoothSocketMac : public BluetoothSocket {
 public:
  static scoped_refptr<BluetoothSocketMac> CreateSocket();

  BluetoothSocketMac(const BluetoothSocketMac&) = delete;
  BluetoothSocketMac& operator=(const BluetoothSocketMac&) = delete;

  // Connects this socket to the service on |device| published as UUID |uuid|.
  // The underlying protocol and PSM or Channel is obtained through service
  // discovery. On a successful connection, the socket properties will be
  // updated and |success_callback| called. On failure, |error_callback| will be
  // called with a message explaining the cause of failure.
  void Connect(IOBluetoothDevice* device,
               const BluetoothUUID& uuid,
               base::OnceClosure success_callback,
               ErrorCompletionCallback error_callback);

  // Listens for incoming RFCOMM connections using this socket: Publishes an
  // RFCOMM service on the |adapter| as UUID |uuid| with Channel
  // |options.channel|, or an automatically allocated Channel if
  // |options.channel| is left null. The service is published with English name
  // |options.name| if that is non-null. |success_callback| will be called if
  // the service is successfully registered, |error_callback| on failure with a
  // message explaining the cause.
  void ListenUsingRfcomm(scoped_refptr<BluetoothAdapterMac> adapter,
                         const BluetoothUUID& uuid,
                         const BluetoothAdapter::ServiceOptions& options,
                         base::OnceClosure success_callback,
                         ErrorCompletionCallback error_callback);

  // Listens for incoming L2CAP connections using this socket: Publishes an
  // L2CAP service on the |adapter| as UUID |uuid| with PSM |options.psm|, or an
  // automatically allocated PSM if |options.psm| is left null. The service is
  // published with English name |options.name| if that is non-null.
  // |success_callback| will be called if the service is successfully
  // registered, |error_callback| on failure with a message explaining the
  // cause.
  void ListenUsingL2cap(scoped_refptr<BluetoothAdapterMac> adapter,
                        const BluetoothUUID& uuid,
                        const BluetoothAdapter::ServiceOptions& options,
                        base::OnceClosure success_callback,
                        ErrorCompletionCallback error_callback);

  // BluetoothSocket:
  void Disconnect(base::OnceClosure callback) override;
  void Receive(int /* buffer_size */,
               ReceiveCompletionCallback success_callback,
               ReceiveErrorCompletionCallback error_callback) override;
  void Send(scoped_refptr<net::IOBuffer> buffer,
            int buffer_size,
            SendCompletionCallback success_callback,
            ErrorCompletionCallback error_callback) override;
  void Accept(AcceptCompletionCallback success_callback,
              ErrorCompletionCallback error_callback) override;

  // Callback that is invoked when the OS completes an SDP query.
  // |status| is the returned status from the SDP query, |device| is the
  // IOBluetoothDevice for which the query was made. The remaining
  // parameters are those from |Connect()|.
  void OnSDPQueryComplete(IOReturn status,
                          IOBluetoothDevice* device,
                          base::OnceClosure success_callback,
                          ErrorCompletionCallback error_callback);

  // Called by BluetoothRfcommConnectionListener and
  // BluetoothL2capConnectionListener.
  void OnChannelOpened(std::unique_ptr<BluetoothChannelMac> channel);

  // Called by |channel_|.
  // Note: OnChannelOpenComplete might be called before the |channel_| is set.
  void OnChannelOpenComplete(const std::string& device_address,
                             IOReturn status);
  void OnChannelClosed();
  void OnChannelDataReceived(void* data, size_t length);
  void OnChannelWriteComplete(void* refcon, IOReturn status);

  void OnChannelOpeningTimeout();
  void OnSDPQueryTimeout();

 private:
  struct AcceptRequest {
    AcceptRequest();
    ~AcceptRequest();

    AcceptCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
  };

  struct SendRequest {
    SendRequest();
    ~SendRequest();
    int buffer_size;
    SendCompletionCallback success_callback;
    ErrorCompletionCallback error_callback;
    IOReturn status = kIOReturnSuccess;
    int active_async_writes = 0;
    bool error_signaled = false;
  };

  struct ReceiveCallbacks {
    ReceiveCallbacks();
    ~ReceiveCallbacks();
    ReceiveCompletionCallback success_callback;
    ReceiveErrorCompletionCallback error_callback;
  };

  struct ConnectCallbacks {
    ConnectCallbacks();
    ~ConnectCallbacks();
    base::OnceClosure success_callback;
    ErrorCompletionCallback error_callback;
  };

  BluetoothSocketMac();
  ~BluetoothSocketMac() override;

  // Accepts a single incoming connection.
  void AcceptConnectionRequest();

  void ReleaseChannel();
  void ReleaseListener();

  bool is_connecting() const { return !!connect_callbacks_; }

  // Used to verify that all methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Adapter the socket is registered against. This is only present when the
  // socket is listening.
  scoped_refptr<BluetoothAdapterMac> adapter_;

  // UUID of the profile being connected to, or that the socket is listening on.
  device::BluetoothUUID uuid_;

  // Simple helpers that register for OS notifications and forward them to
  // |this| profile.
  BluetoothRfcommConnectionListener* __strong rfcomm_connection_listener_;
  BluetoothL2capConnectionListener* __strong l2cap_connection_listener_;
  SDPQueryListener* __strong sdp_query_listener_;

  // The service record registered in the system SDP server, used to
  // eventually unregister the service.
  IOBluetoothSDPServiceRecord* __strong service_record_;

  // The channel used to issue commands.
  std::unique_ptr<BluetoothChannelMac> channel_;

  // Connection callbacks -- when a pending async connection is active.
  std::unique_ptr<ConnectCallbacks> connect_callbacks_;

  // Packets received while there is no pending "receive" callback.
  base::queue<scoped_refptr<net::IOBufferWithSize>> receive_queue_;

  // Receive callbacks -- when a receive call is active.
  std::unique_ptr<ReceiveCallbacks> receive_callbacks_;

  // Send queue -- one entry per pending send operation.
  base::queue<std::unique_ptr<SendRequest>> send_queue_;

  // The pending request to an Accept() call, or null if there is no pending
  // request.
  std::unique_ptr<AcceptRequest> accept_request_;

  // Queue of incoming connections.
  base::queue<std::unique_ptr<BluetoothChannelMac>> accept_queue_;

  // One shot timer for detecting SDP query or channel opening timeout.
  base::OneShotTimer timer_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_

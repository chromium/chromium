// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_
#define NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_

#include "net/base/datagram_buffer.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket.h"

namespace net {

class IPEndPoint;
class SocketTag;

class NET_EXPORT_PRIVATE DatagramClientSocket : public DatagramSocket,
                                                public Socket {
 public:
  ~DatagramClientSocket() override = default;

  // Initialize this socket as a client socket to server at |address|. This
  // method can only be called once, as it opens a socket and socket reuse is
  // not supported. Returns a network error code.
  // TODO(liza): Remove this method once consumers have been updated.
  virtual int Connect(const IPEndPoint& address) = 0;

  // Binds this socket to |network| and initializes socket as a client socket
  // to server at |address|. All data traffic on the socket will be sent and
  // received via |network|. This call will fail if |network| has disconnected.
  // Communication using this socket will fail if |network| disconnects. Like
  // Connect, this method can only be called once. Returns a net error code.
  // TODO(liza): Remove this method once consumers have been updated.
  virtual int ConnectUsingNetwork(handles::NetworkHandle network,
                                  const IPEndPoint& address) = 0;

  // Same as ConnectUsingNetwork, except that the current default network is
  // used. Like Connect, this method can only be called once. Returns a net
  // error code.
  // TODO(liza): Remove this method once consumers have been updated.
  virtual int ConnectUsingDefaultNetwork(const IPEndPoint& address) = 0;

  // Same as Connect, but it can run asynchronously or synchronously. Returns a
  // network error code.
  // TODO(liza): Rename this to Connect once consumers have been updated.
  virtual int ConnectAsync(const IPEndPoint& address,
                           CompletionOnceCallback callback) = 0;

  // Same as ConnectUsingNetwork, but it can run asynchronously or
  // synchronously. Returns a network error code.
  // TODO(liza): Rename this to ConnectUsingNetwork once consumers have been
  // updated.
  virtual int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                                       const IPEndPoint& address,
                                       CompletionOnceCallback callback) = 0;

  // Same as ConnectUsConnectUsingDefaultNetworkingNetwork, but it can run
  // asynchronously or synchronously. Returns a network error code.
  // TODO(liza): Rename this to ConnectUsingDefaultNetwork once consumers have
  // been updated.
  virtual int ConnectUsingDefaultNetworkAsync(
      const IPEndPoint& address,
      CompletionOnceCallback callback) = 0;

  // Returns the network that either ConnectUsingNetwork() or
  // ConnectUsingDefaultNetwork() bound this socket to. Returns
  // handles::kInvalidNetworkHandle if not explicitly bound via
  // ConnectUsingNetwork() or ConnectUsingDefaultNetwork().
  virtual handles::NetworkHandle GetBoundNetwork() const = 0;

  // Apply |tag| to this socket.
  virtual void ApplySocketTag(const SocketTag& tag) = 0;

  // Enables experimental optimization for receiving data from a socket.
  // By default, this method is no-op.
  virtual void EnableRecvOptimization() {}

  // Set interface to use for data sent to multicast groups. If
  // |interface_index| set to 0, default interface is used.
  // Must be called before Connect(), ConnectUsingNetwork() or
  // ConnectUsingDefaultNetwork().
  // Returns a network error code.
  virtual int SetMulticastInterface(uint32_t interface_index) = 0;

  // Set iOS Network Service Type for socket option SO_NET_SERVICE_TYPE.
  // No-op by default.
  virtual void SetIOSNetworkServiceType(int ios_network_service_type) {}
};

}  // namespace net

#endif  // NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_

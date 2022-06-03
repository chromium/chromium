// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_
#define NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_

#include "net/base/datagram_buffer.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket.h"

namespace net {

class IPEndPoint;
class SocketTag;

class NET_EXPORT_PRIVATE DatagramClientSocket : public DatagramSocket,
                                                public Socket {
 public:
  ~DatagramClientSocket() override {}

  // Initialize this socket as a client socket to server at |address|.
  // Returns a network error code.
  virtual int Connect(const IPEndPoint& address) = 0;

  // Binds this socket to |network| and initializes socket as a client socket
  // to server at |address|. All data traffic on the socket will be sent and
  // received via |network|. This call will fail if |network| has disconnected.
  // Communication using this socket will fail if |network| disconnects.
  // Returns a net error code.
  virtual int ConnectUsingNetwork(NetworkChangeNotifier::NetworkHandle network,
                                  const IPEndPoint& address) = 0;

  // Same as ConnectUsingNetwork, except that the current default network is
  // used. Returns a net error code.
  virtual int ConnectUsingDefaultNetwork(const IPEndPoint& address) = 0;

  // Returns the network that either ConnectUsingNetwork() or
  // ConnectUsingDefaultNetwork() bound this socket to. Returns
  // NetworkChangeNotifier::kInvalidNetworkHandle if not explicitly bound via
  // ConnectUsingNetwork() or ConnectUsingDefaultNetwork().
  virtual NetworkChangeNotifier::NetworkHandle GetBoundNetwork() const = 0;

  // Apply |tag| to this socket.
  virtual void ApplySocketTag(const SocketTag& tag) = 0;

  // Enables experimental optimization for receiving data from a socket.
  // By default, this method is no-op.
  virtual void EnableRecvOptimization() {}

  // As Write, but internally this can delay writes and batch them up
  // for writing in a separate task.  This is to increase throughput
  // in bulk transfer scenarios (in QUIC) where a substantial
  // proportion of CPU time is spend in kernel UDP writes, and total
  // CPU time of the net IO thread saturates single core capacity.
  // The batching is required to allow overlapped computation time
  // between user and kernel packet processing.
  //
  // Returns the number of bytes written or a net error code.  A
  // return value of zero is possible, because with batching enabled,
  // the underlying socket write may be delayed so as to accumulate
  // multiple buffers.  The return value may also be larger than the
  // number of bytes in |buffers| due to completion of previous
  // writes.  [ Writing the batch to the socket typically happens on a
  // different thread/cpu core. ]
  //
  // As with |Write|, a return value of ERR_IO_PENDING indicates the
  // caller should suspend further writes until the callback fires.
  //
  // If a socket write returns an error, it will be surfaced either as
  // the return value from the next call to |WriteAsync|, or via the
  // callback.
  //
  // Not all platforms will implement this, see |write_async_enabled()|
  // below.
  virtual int WriteAsync(
      DatagramBuffers buffers,
      CompletionOnceCallback callback,
      const NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  // |buffer| is copied to an internal |DatagramBuffer|, caller
  // |retains ownership of |buffer|.
  virtual int WriteAsync(
      const char* buffer,
      size_t buf_len,
      CompletionOnceCallback callback,
      const NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  // With WriteAsync, the caller may wish to try unwritten buffers on
  // a new socket, e.g. with QUIC connection migration.
  virtual DatagramBuffers GetUnwrittenBuffers() = 0;

  // Enable |WriteAsync()|.  May be a noop, see |WriteAsyncEnabled()|
  // below.  Must be called right after construction and before other
  // calls. This is intended to support rollout of |WriteAsync| for
  // QUIC via a Finch trial, using the kWRBA client connection option.
  virtual void SetWriteAsyncEnabled(bool enabled) = 0;

  // Needed with |WriteAsync()| enabled, for socket's
  // |DatagramBufferPool|.  Must be called right after construction
  // and before other calls.
  virtual void SetMaxPacketSize(size_t max_packet_size) = 0;

  // This is true if the |SetWriteAsyncEnabled(true)| has been called
  // *and* the platform supports |WriteAsync()|.
  virtual bool WriteAsyncEnabled() = 0;

  // In |WriteAsync()|, allow socket writing to happen on a separate
  // core when advantageous.  This can increase maximum single-stream
  // throughput.  Must be called right after construction and before
  // other calls. This is intended to support QUIC Finch trials, using
  // the kMLTC client connection option.
  virtual void SetWriteMultiCoreEnabled(bool enabled) = 0;

  // In |WriteAsync()|, use |sendmmsg()| on platforms that support it.
  // This can increase maximum single-stream throughput.  Must be
  // called right after construction and before other calls.  This is
  // intended to support QUIC Finch trials, using the kMMSG client
  // connection option.
  virtual void SetSendmmsgEnabled(bool enabled) = 0;

  // This is to (de-)activate batching in |WriteAsync|, e.g. in
  // |QuicChromiumClientSession| based on whether there are large
  // upload stream(s) active.
  virtual void SetWriteBatchingActive(bool active) = 0;

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

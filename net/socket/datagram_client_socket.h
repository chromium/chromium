// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_
#define NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_

#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket.h"

namespace net {

// Minimum buffer size required when calling ReadMultiple(). On platforms
// supporting kernel-coalesced superpackets (e.g. UDP GRO), superpackets can be
// up to 64KB (64 * 1024 bytes). Callers must provide a buffer of at least this
// size to prevent packet truncation.
inline constexpr size_t kMinimumReadMultipleBufferSize = 64 * 1024;

struct NET_EXPORT_PRIVATE DatagramMetadata {
  // The start offset of this datagram's data within the buffer passed to the
  // read API (e.g., ReadMultiple).
  size_t offset;
  // The length of the read datagram in bytes.
  size_t length;
  // The Type of Service (TOS) / Traffic Class byte received with the datagram.
  uint8_t tos;
};

using DatagramsMetadata = std::vector<DatagramMetadata>;

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

  // Reads one or more datagrams from a connected socket. Depending on the
  // ratio of `buf_len` to `max_message_size`, this may read a single
  // datagram, or multiple datagrams via recvmmsg (on supported platforms like
  // Linux, ChromeOS, and Android). On platforms where recvmmsg is not
  // supported (e.g., macOS, iOS, Fuchsia, or Windows), the implementation
  // may fall back to reading a single datagram using recvmsg/recvfrom.
  //
  // Calling Semantics:
  // This method returns either the actual value (`DatagramsMetadata`) or an
  // error code (`Error`).
  //
  // - If the read completes synchronously, it returns the `DatagramsMetadata`
  //   on success, or an error code (other than `ERR_IO_PENDING`) on failure.
  //   The `callback` will NOT be invoked.
  // - If the read proceeds asynchronously, it returns `ERR_IO_PENDING`. The
  //   `callback` will be invoked later with the final result (either
  //   `DatagramsMetadata` or an error code other than `ERR_IO_PENDING`).
  //
  // NOTE: `buf_len` MUST be at least `kMinimumReadMultipleBufferSize` (64 *
  // 1024 bytes). See `UDPSocketPosix::ReadMultiple` for details.
  virtual base::expected<DatagramsMetadata, Error> ReadMultiple(
      IOBuffer* buf,
      size_t buf_len,
      size_t max_message_size,
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
          callback) = 0;

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

  // Register a QUIC UDP payload that can close a QUIC connection and the
  // underlying socket to the Android system server. When the app loses network
  // access, the system server destroys the registered socket and sends the
  // registered UDP payload to the server.
  virtual void RegisterQuicConnectionClosePayload(base::span<uint8_t> payload) {
  }

  // Unregister the underlying socket and its associated UDP payload that were
  // previously registered by RegisterQuicConnectionClosePayload
  virtual void UnregisterQuicConnectionClosePayload() {}
};

}  // namespace net

#endif  // NET_SOCKET_DATAGRAM_CLIENT_SOCKET_H_

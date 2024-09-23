// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_SOCKET_H_
#define NET_SOCKET_STREAM_SOCKET_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket.h"

namespace net {

class IPEndPoint;
class NetLogWithSource;
class SSLCertRequestInfo;
class SSLInfo;
class SocketTag;

class NET_EXPORT StreamSocket : public Socket {
 public:
  using BeforeConnectCallback = base::RepeatingCallback<int()>;

  ~StreamSocket() override = default;

  // Sets a callback to be invoked before establishing a connection. This allows
  // setting options, like receive and send buffer size, when they will take
  // effect. The callback should return net::OK on success, and an error on
  // failure. It must not return net::ERR_IO_PENDING.
  //
  // If multiple connection attempts are made, the callback will be invoked for
  // each one.
  virtual void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback);

  // Called to establish a connection.  Returns OK if the connection could be
  // established synchronously.  Otherwise, ERR_IO_PENDING is returned and the
  // given callback will run asynchronously when the connection is established
  // or when an error occurs.  The result is some other error code if the
  // connection could not be established.
  //
  // The socket's Read and Write methods may not be called until Connect
  // succeeds.
  //
  // It is valid to call Connect on an already connected socket, in which case
  // OK is simply returned.
  //
  // Connect may also be called again after a call to the Disconnect method.
  //
  virtual int Connect(CompletionOnceCallback callback) = 0;

  // Called to confirm the TLS handshake, if any, indicating that replay
  // protection is ready. Returns OK if the handshake could complete
  // synchronously or had already been confirmed. Otherwise, ERR_IO_PENDING is
  // returned and the given callback will run asynchronously when the connection
  // is established or when an error occurs.  The result is some other error
  // code if the connection could not be completed.
  //
  // This operation is only needed if TLS early data is enabled, in which case
  // Connect returns early and Write initially sends early data, which does not
  // have TLS's usual security properties. The caller must call this function
  // and wait for handshake confirmation before sending data that is not
  // replay-safe.
  //
  // ConfirmHandshake may run concurrently with Read or Write, but, as with Read
  // and Write, at most one pending ConfirmHandshake operation may be in
  // progress at a time.
  virtual int ConfirmHandshake(CompletionOnceCallback callback);

  // Called to disconnect a socket.  Does nothing if the socket is already
  // disconnected.  After calling Disconnect it is possible to call Connect
  // again to establish a new connection.
  //
  // If IO (Connect, Read, or Write) is pending when the socket is
  // disconnected, the pending IO is cancelled, and the completion callback
  // will not be called.
  virtual void Disconnect() = 0;

  // Called to test if the connection is still alive.  Returns false if a
  // connection wasn't established or the connection is dead.  True is returned
  // if the connection was terminated, but there is unread data in the incoming
  // buffer.
  virtual bool IsConnected() const = 0;

  // Called to test if the connection is still alive and idle.  Returns false
  // if a connection wasn't established, the connection is dead, or there is
  // unread data in the incoming buffer.
  virtual bool IsConnectedAndIdle() const = 0;

  // Copies the peer address to |address| and returns a network error code.
  // ERR_SOCKET_NOT_CONNECTED will be returned if the socket is not connected.
  virtual int GetPeerAddress(IPEndPoint* address) const = 0;

  // Copies the local address to |address| and returns a network error code.
  // ERR_SOCKET_NOT_CONNECTED will be returned if the socket is not bound.
  virtual int GetLocalAddress(IPEndPoint* address) const = 0;

  // Gets the NetLog for this socket.
  virtual const NetLogWithSource& NetLog() const = 0;

  // Returns true if the socket ever had any reads or writes.  StreamSockets
  // layered on top of transport sockets should return if their own Read() or
  // Write() methods had been called, not the underlying transport's.
  virtual bool WasEverUsed() const = 0;

  // Returns the protocol negotiated via ALPN for this socket, or
  // kProtoUnknown will be returned if ALPN is not applicable.
  virtual NextProto GetNegotiatedProtocol() const = 0;

  // Get data received from peer in ALPS TLS extension.
  // Returns a (possibly empty) value if a TLS version supporting ALPS was used
  // and ALPS was negotiated, nullopt otherwise.
  virtual std::optional<std::string_view> GetPeerApplicationSettings() const;

  // Gets the SSL connection information of the socket.  Returns false if
  // SSL was not used by this socket.
  virtual bool GetSSLInfo(SSLInfo* ssl_info) = 0;

  // Gets the SSL CertificateRequest info of the socket after Connect failed
  // with ERR_SSL_CLIENT_AUTH_CERT_NEEDED.  Must not be called on a socket that
  // does not support SSL.
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) const;

  // Returns the total number of number bytes read by the socket. This only
  // counts the payload bytes. Transport headers are not counted. Returns
  // 0 if the socket does not implement the function. The count is reset when
  // Disconnect() is called.
  virtual int64_t GetTotalReceivedBytes() const = 0;

  // Apply |tag| to this socket. If socket isn't yet connected, tag will be
  // applied when socket is later connected. If Connect() fails or socket
  // is closed, tag is cleared. If this socket is layered upon or wraps an
  // underlying socket, |tag| will be applied to the underlying socket in the
  // same manner as if ApplySocketTag() was called on the underlying socket.
  // The tag can be applied at any time, in other words active sockets can be
  // retagged with a different tag. Sockets wrapping multiplexed sockets
  // (e.g. sockets who proxy through a QUIC or Spdy stream) cannot be tagged as
  // the tag would inadvertently affect other streams; calling ApplySocketTag()
  // in this case will result in CHECK(false).
  virtual void ApplySocketTag(const SocketTag& tag) = 0;
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_H_

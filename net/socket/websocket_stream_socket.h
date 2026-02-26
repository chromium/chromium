// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_STREAM_SOCKET_H_
#define NET_SOCKET_WEBSOCKET_STREAM_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/websocket_endpoint_lock_manager.h"

namespace net {

class IPEndPoint;
class IOBuffer;

// StreamSocket wrapper that wraps a passed in StreamSocket. On Connect(), it
// waits lock the endpoints with a WebSocketEndpointLockManager before calling
// the wrapped socket's connect method. It releases the lock on destruction.
class NET_EXPORT WebSocketStreamSocket final : public StreamSocket {
 public:
  // `endpoint` must be the IPEndPoint that `stream_socket` will connect to when
  // its Connect() method is called. `stream_socket` must not be connected.
  WebSocketStreamSocket(
      WebSocketEndpointLockManager& websocket_endpoint_lock_manager,
      const IPEndPoint& endpoint,
      std::unique_ptr<StreamSocket> stream_socket);

  WebSocketStreamSocket(const WebSocketStreamSocket&) = delete;
  WebSocketStreamSocket& operator=(const WebSocketStreamSocket&) = delete;

  ~WebSocketStreamSocket() override;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  void SetDnsAliases(std::set<std::string> aliases) override;
  const std::set<std::string>& GetDnsAliases() const override;

  // StreamSocket implementation:
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

 private:
  void OnWebSocketEndpointLockObtained(CompletionOnceCallback callback);

  std::unique_ptr<StreamSocket> wrapped_socket_;
  WebSocketEndpointLockManager::EndpointLock endpoint_lock_;
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_STREAM_SOCKET_H_

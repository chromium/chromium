// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class WebSocketStreamRequestAPI;
class SpdySession;
class WebSocketBasicHandshakeStream;
class WebSocketEndpointLockManager;

// Implementation of WebSocketHandshakeStreamBase::CreateHelper. This class is
// used in the implementation of WebSocketStream::CreateAndConnectStream() and
// is not intended to be used by itself.
//
// Holds the information needed to construct a
// WebSocketBasicHandshakeStreamBase.
class NET_EXPORT_PRIVATE WebSocketHandshakeStreamCreateHelper
    : public WebSocketHandshakeStreamBase::CreateHelper {
 public:
  // |*connect_delegate| and |*request| must out-live this object.
  WebSocketHandshakeStreamCreateHelper(
      WebSocketStream::ConnectDelegate* connect_delegate,
      const std::vector<std::string>& requested_subprotocols,
      WebSocketStreamRequestAPI* request);

  ~WebSocketHandshakeStreamCreateHelper() override;

  // WebSocketHandshakeStreamBase::CreateHelper methods

  // Creates a WebSocketBasicHandshakeStream over a TCP/IP or TLS socket.
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
      std::unique_ptr<ClientSocketHandle> connection,
      bool using_proxy,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager) override;

  // Creates a WebSocketHttp2HandshakeStream over an HTTP/2 connection.
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp2Stream(
      base::WeakPtr<SpdySession> session) override;

 private:
  WebSocketStream::ConnectDelegate* const connect_delegate_;
  const std::vector<std::string> requested_subprotocols_;
  WebSocketStreamRequestAPI* const request_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeStreamCreateHelper);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_

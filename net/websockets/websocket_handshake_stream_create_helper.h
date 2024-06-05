// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class WebSocketStreamRequestAPI;
class SpdySession;
class WebSocketBasicHandshakeStream;
class WebSocketEndpointLockManager;
class ClientSocketHandle;

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

  WebSocketHandshakeStreamCreateHelper(
      const WebSocketHandshakeStreamCreateHelper&) = delete;
  WebSocketHandshakeStreamCreateHelper& operator=(
      const WebSocketHandshakeStreamCreateHelper&) = delete;

  ~WebSocketHandshakeStreamCreateHelper() override;

  // WebSocketHandshakeStreamBase::CreateHelper methods

  // Creates a WebSocketBasicHandshakeStream over a TCP/IP or TLS socket.
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
      std::unique_ptr<ClientSocketHandle> connection,
      bool using_proxy,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager) override;

  // Creates a WebSocketHttp2HandshakeStream over an HTTP/2 connection.
  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp2Stream(
      base::WeakPtr<SpdySession> session,
      std::set<std::string> dns_aliases) override;

  std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp3Stream(
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      std::set<std::string> dns_aliases) override;

 private:
  const raw_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;
  const std::vector<std::string> requested_subprotocols_;
  const raw_ptr<WebSocketStreamRequestAPI> request_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_CREATE_HELPER_H_

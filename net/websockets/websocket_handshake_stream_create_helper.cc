// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_stream_create_helper.h"

#include <set>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "net/socket/client_socket_handle.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "net/websockets/websocket_http2_handshake_stream.h"
#include "net/websockets/websocket_http3_handshake_stream.h"

namespace net {

WebSocketHandshakeStreamCreateHelper::WebSocketHandshakeStreamCreateHelper(
    WebSocketStream::ConnectDelegate* connect_delegate,
    const std::vector<std::string>& requested_subprotocols,
    WebSocketStreamRequestAPI* request)
    : connect_delegate_(connect_delegate),
      requested_subprotocols_(requested_subprotocols),
      request_(request) {
  DCHECK(connect_delegate_);
  DCHECK(request_);
}

WebSocketHandshakeStreamCreateHelper::~WebSocketHandshakeStreamCreateHelper() =
    default;

std::unique_ptr<WebSocketHandshakeStreamBase>
WebSocketHandshakeStreamCreateHelper::CreateBasicStream(
    std::unique_ptr<ClientSocketHandle> connection,
    bool using_proxy,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager) {
  // The list of supported extensions and parameters is hard-coded.
  // TODO(ricea): If more extensions are added, consider a more flexible
  // method.
  std::vector<std::string> extensions(
      1, "permessage-deflate; client_max_window_bits");
  auto stream = std::make_unique<WebSocketBasicHandshakeStream>(
      std::move(connection), connect_delegate_, using_proxy,
      requested_subprotocols_, extensions, request_,
      websocket_endpoint_lock_manager);
  request_->OnBasicHandshakeStreamCreated(stream.get());
  return stream;
}

std::unique_ptr<WebSocketHandshakeStreamBase>
WebSocketHandshakeStreamCreateHelper::CreateHttp2Stream(
    base::WeakPtr<SpdySession> session,
    std::set<std::string> dns_aliases) {
  std::vector<std::string> extensions(
      1, "permessage-deflate; client_max_window_bits");
  auto stream = std::make_unique<WebSocketHttp2HandshakeStream>(
      session, connect_delegate_, requested_subprotocols_, extensions, request_,
      std::move(dns_aliases));
  request_->OnHttp2HandshakeStreamCreated(stream.get());
  return stream;
}

std::unique_ptr<WebSocketHandshakeStreamBase>
WebSocketHandshakeStreamCreateHelper::CreateHttp3Stream(
    std::unique_ptr<QuicChromiumClientSession::Handle> session,
    std::set<std::string> dns_aliases) {
  std::vector<std::string> extensions(
      1, "permessage-deflate; client_max_window_bits");

  auto stream = std::make_unique<WebSocketHttp3HandshakeStream>(
      std::move(session), connect_delegate_, requested_subprotocols_,
      extensions, request_, std::move(dns_aliases));
  request_->OnHttp3HandshakeStreamCreated(stream.get());
  return stream;
}

}  // namespace net

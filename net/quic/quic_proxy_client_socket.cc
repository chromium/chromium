// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_client_socket.h"

#include <cstdio>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_log_util.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/quic/quic_http_utils.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

QuicProxyClientSocket::QuicProxyClientSocket(
    std::unique_ptr<QuicChromiumClientStream::Handle> stream,
    std::unique_ptr<QuicChromiumClientSession::Handle> session,
    const ProxyChain& proxy_chain,
    size_t proxy_chain_index,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    const NetLogWithSource& net_log,
    scoped_refptr<HttpAuthController> auth_controller,
    ProxyDelegate* proxy_delegate)
    : stream_(std::move(stream)),
      session_(std::move(session)),
      endpoint_(endpoint),
      auth_(std::move(auth_controller)),
      proxy_chain_(proxy_chain),
      proxy_chain_index_(proxy_chain_index),
      proxy_delegate_(proxy_delegate),
      user_agent_(user_agent),
      net_log_(net_log) {
  DCHECK(stream_->IsOpen());

  request_.method = "CONNECT";
  request_.url = GURL("https://" + endpoint.ToString());

  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                                       net_log_.source());
  net_log_.AddEventReferencingSource(
      NetLogEventType::HTTP2_PROXY_CLIENT_SESSION, stream_->net_log().source());
}

QuicProxyClientSocket::~QuicProxyClientSocket() {
  Disconnect();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

const HttpResponseInfo* QuicProxyClientSocket::GetConnectResponseInfo() const {
  return response_.headers.get() ? &response_ : nullptr;
}

const scoped_refptr<HttpAuthController>&
QuicProxyClientSocket::GetAuthController() const {
  return auth_;
}

int QuicProxyClientSocket::RestartWithAuth(CompletionOnceCallback callback) {
  // A QUIC Stream can only handle a single request, so the underlying
  // stream may not be reused and a new QuicProxyClientSocket must be
  // created (possibly on top of the same QUIC Session).
  next_state_ = STATE_DISCONNECTED;
  return ERR_UNABLE_TO_REUSE_CONNECTION_FOR_PROXY_AUTH;
}

// Ignore priority changes, just use priority of initial request. Since multiple
// requests are pooled on the QuicProxyClientSocket, reprioritization doesn't
// really work.
//
// TODO(mmenke):  Use a single priority value for all QuicProxyClientSockets,
// regardless of what priority they're created with.
void QuicProxyClientSocket::SetStreamPriority(RequestPriority priority) {}

// Sends a HEADERS frame to the proxy with a CONNECT request
// for the specified endpoint.  Waits for the server to send back
// a HEADERS frame.  OK will be returned if the status is 200.
// ERR_TUNNEL_CONNECTION_FAILED will be returned for any other status.
// In any of these cases, Read() may be called to retrieve the HTTP
// response body.  Any other return values should be considered fatal.
int QuicProxyClientSocket::Connect(CompletionOnceCallback callback) {
  DCHECK(connect_callback_.is_null());
  if (!stream_->IsOpen())
    return ERR_CONNECTION_CLOSED;

  DCHECK_EQ(STATE_DISCONNECTED, next_state_);
  next_state_ = STATE_GENERATE_AUTH_TOKEN;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    connect_callback_ = std::move(callback);
  return rv;
}

void QuicProxyClientSocket::Disconnect() {
  connect_callback_.Reset();
  read_callback_.Reset();
  read_buf_ = nullptr;
  write_callback_.Reset();
  write_buf_len_ = 0;

  next_state_ = STATE_DISCONNECTED;

  stream_->Reset(quic::QUIC_STREAM_CANCELLED);
}

bool QuicProxyClientSocket::IsConnected() const {
  return next_state_ == STATE_CONNECT_COMPLETE && stream_->IsOpen();
}

bool QuicProxyClientSocket::IsConnectedAndIdle() const {
  return IsConnected() && !stream_->HasBytesToRead();
}

const NetLogWithSource& QuicProxyClientSocket::NetLog() const {
  return net_log_;
}

bool QuicProxyClientSocket::WasEverUsed() const {
  return session_->WasEverUsed();
}

NextProto QuicProxyClientSocket::GetNegotiatedProtocol() const {
  // Do not delegate to `session_`. While `session_` negotiates ALPN with the
  // proxy, this object represents the tunneled TCP connection to the origin.
  return kProtoUnknown;
}

bool QuicProxyClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  // Do not delegate to `session_`. While `session_` has a secure channel to the
  // proxy, this object represents the tunneled TCP connection to the origin.
  return false;
}

int64_t QuicProxyClientSocket::GetTotalReceivedBytes() const {
  return stream_->NumBytesConsumed();
}

void QuicProxyClientSocket::ApplySocketTag(const SocketTag& tag) {
  // In the case of a connection to the proxy using HTTP/2 or HTTP/3 where the
  // underlying socket may multiplex multiple streams, applying this request's
  // socket tag to the multiplexed session would incorrectly apply the socket
  // tag to all mutliplexed streams. Fortunately socket tagging is only
  // supported on Android without the data reduction proxy, so only simple HTTP
  // proxies are supported, so proxies won't be using HTTP/2 or HTTP/3. Enforce
  // that a specific (non-default) tag isn't being applied.
  CHECK(tag == SocketTag());
}

int QuicProxyClientSocket::Read(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  DCHECK(connect_callback_.is_null());
  DCHECK(read_callback_.is_null());
  DCHECK(!read_buf_);

  if (next_state_ == STATE_DISCONNECTED)
    return ERR_SOCKET_NOT_CONNECTED;

  if (!stream_->IsOpen()) {
    return 0;
  }

  int rv =
      stream_->ReadBody(buf, buf_len,
                        base::BindOnce(&QuicProxyClientSocket::OnReadComplete,
                                       weak_factory_.GetWeakPtr()));

  if (rv == ERR_IO_PENDING) {
    read_callback_ = std::move(callback);
    read_buf_ = buf;
  } else if (rv == 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, 0,
                                  nullptr);
  } else if (rv > 0) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, rv,
                                  buf->data());
  }
  return rv;
}

void QuicProxyClientSocket::OnReadComplete(int rv) {
  if (!stream_->IsOpen())
    rv = 0;

  if (!read_callback_.is_null()) {
    DCHECK(read_buf_);
    if (rv >= 0) {
      net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, rv,
                                    read_buf_->data());
    }
    read_buf_ = nullptr;
    std::move(read_callback_).Run(rv);
  }
}

int QuicProxyClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(connect_callback_.is_null());
  DCHECK(write_callback_.is_null());

  if (next_state_ != STATE_CONNECT_COMPLETE)
    return ERR_SOCKET_NOT_CONNECTED;

  net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT, buf_len,
                                buf->data());

  int rv = stream_->WriteStreamData(
      std::string_view(buf->data(), buf_len), false,
      base::BindOnce(&QuicProxyClientSocket::OnWriteComplete,
                     weak_factory_.GetWeakPtr()));
  if (rv == OK)
    return buf_len;

  if (rv == ERR_IO_PENDING) {
    write_callback_ = std::move(callback);
    write_buf_len_ = buf_len;
  }

  return rv;
}

void QuicProxyClientSocket::OnWriteComplete(int rv) {
  if (!write_callback_.is_null()) {
    if (rv == OK)
      rv = write_buf_len_;
    write_buf_len_ = 0;
    std::move(write_callback_).Run(rv);
  }
}

int QuicProxyClientSocket::SetReceiveBufferSize(int32_t size) {
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyClientSocket::SetSendBufferSize(int32_t size) {
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return IsConnected() ? session_->GetPeerAddress(address)
                       : ERR_SOCKET_NOT_CONNECTED;
}

int QuicProxyClientSocket::GetLocalAddress(IPEndPoint* address) const {
  return IsConnected() ? session_->GetSelfAddress(address)
                       : ERR_SOCKET_NOT_CONNECTED;
}

void QuicProxyClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_DISCONNECTED, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    // Connect() finished (successfully or unsuccessfully).
    DCHECK(!connect_callback_.is_null());
    std::move(connect_callback_).Run(rv);
  }
}

int QuicProxyClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_DISCONNECTED);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_DISCONNECTED;
    switch (state) {
      case STATE_GENERATE_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateAuthToken();
        break;
      case STATE_GENERATE_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateAuthTokenComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST, rv);
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_READ_REPLY:
        rv = DoReadReply();
        break;
      case STATE_READ_REPLY_COMPLETE:
        rv = DoReadReplyComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS, rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_DISCONNECTED &&
           next_state_ != STATE_CONNECT_COMPLETE);
  return rv;
}

int QuicProxyClientSocket::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  return auth_->MaybeGenerateAuthToken(
      &request_,
      base::BindOnce(&QuicProxyClientSocket::OnIOComplete,
                     weak_factory_.GetWeakPtr()),
      net_log_);
}

int QuicProxyClientSocket::DoGenerateAuthTokenComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int QuicProxyClientSocket::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  // Add Proxy-Authentication header if necessary.
  HttpRequestHeaders authorization_headers;
  if (auth_->HaveAuth()) {
    auth_->AddAuthorizationHeader(&authorization_headers);
  }

  if (proxy_delegate_) {
    HttpRequestHeaders proxy_delegate_headers;
    int result = proxy_delegate_->OnBeforeTunnelRequest(
        proxy_chain_, proxy_chain_index_, &proxy_delegate_headers);
    if (result < 0) {
      return result;
    }
    request_.extra_headers.MergeFrom(proxy_delegate_headers);
  }

  std::string request_line;
  BuildTunnelRequest(endpoint_, authorization_headers, user_agent_,
                     &request_line, &request_.extra_headers);

  NetLogRequestHeaders(net_log_,
                       NetLogEventType::HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
                       request_line, &request_.extra_headers);

  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request_, std::nullopt,
                                   request_.extra_headers, &headers);

  return stream_->WriteHeaders(std::move(headers), false, nullptr);
}

int QuicProxyClientSocket::DoSendRequestComplete(int result) {
  if (result >= 0) {
    // Wait for HEADERS frame from the server
    next_state_ = STATE_READ_REPLY;  // STATE_READ_REPLY_COMPLETE;
    result = OK;
  }

  if (result >= 0 || result == ERR_IO_PENDING) {
    // Emit extra event so can use the same events as HttpProxyClientSocket.
    net_log_.BeginEvent(NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS);
  }

  return result;
}

int QuicProxyClientSocket::DoReadReply() {
  next_state_ = STATE_READ_REPLY_COMPLETE;

  int rv = stream_->ReadInitialHeaders(
      &response_header_block_,
      base::BindOnce(&QuicProxyClientSocket::OnReadResponseHeadersComplete,
                     weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING)
    return ERR_IO_PENDING;
  if (rv < 0)
    return rv;

  return ProcessResponseHeaders(response_header_block_);
}

int QuicProxyClientSocket::DoReadReplyComplete(int result) {
  if (result < 0)
    return result;

  // Require the "HTTP/1.x" status line for SSL CONNECT.
  if (response_.headers->GetHttpVersion() < HttpVersion(1, 0))
    return ERR_TUNNEL_CONNECTION_FAILED;

  NetLogResponseHeaders(
      net_log_, NetLogEventType::HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      response_.headers.get());

  if (proxy_delegate_) {
    int rv = proxy_delegate_->OnTunnelHeadersReceived(
        proxy_chain_, proxy_chain_index_, *response_.headers);
    if (rv != OK) {
      DCHECK_NE(ERR_IO_PENDING, rv);
      return rv;
    }
  }

  switch (response_.headers->response_code()) {
    case 200:  // OK
      next_state_ = STATE_CONNECT_COMPLETE;
      return OK;

    case 407:  // Proxy Authentication Required
      next_state_ = STATE_CONNECT_COMPLETE;
      SanitizeProxyAuth(response_);
      return HandleProxyAuthChallenge(auth_.get(), &response_, net_log_);

    default:
      // Ignore response to avoid letting the proxy impersonate the target
      // server.  (See http://crbug.com/137891.)
      return ERR_TUNNEL_CONNECTION_FAILED;
  }
}

void QuicProxyClientSocket::OnReadResponseHeadersComplete(int result) {
  // Convert the now-populated quiche::HttpHeaderBlock to HttpResponseInfo
  if (result > 0)
    result = ProcessResponseHeaders(response_header_block_);

  if (result != ERR_IO_PENDING)
    OnIOComplete(result);
}

int QuicProxyClientSocket::ProcessResponseHeaders(
    const quiche::HttpHeaderBlock& headers) {
  if (SpdyHeadersToHttpResponse(headers, &response_) != OK) {
    DLOG(WARNING) << "Invalid headers";
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  return OK;
}

}  // namespace net

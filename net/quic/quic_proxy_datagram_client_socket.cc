// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_datagram_client_socket.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_log_util.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/spdy/spdy_http_utils.h"

namespace net {

QuicProxyDatagramClientSocket::QuicProxyDatagramClientSocket(
    const GURL& url,
    const ProxyChain& proxy_chain,
    const std::string& user_agent,
    const NetLogWithSource& source_net_log,
    ProxyDelegate* proxy_delegate)
    : url_(url),
      proxy_chain_(proxy_chain),
      proxy_delegate_(proxy_delegate),
      user_agent_(user_agent),
      net_log_(NetLogWithSource::Make(
          source_net_log.net_log(),
          NetLogSourceType::QUIC_PROXY_DATAGRAM_CLIENT_SOCKET)) {
  CHECK_GE(proxy_chain.length(), 1u);
  request_.method = "CONNECT";
  request_.url = url_;

  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                                       source_net_log.source());
}

QuicProxyDatagramClientSocket::~QuicProxyDatagramClientSocket() {
  Close();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

const HttpResponseInfo* QuicProxyDatagramClientSocket::GetConnectResponseInfo()
    const {
  return response_.headers.get() ? &response_ : nullptr;
}

bool QuicProxyDatagramClientSocket::IsConnected() const {
  return next_state_ == STATE_CONNECT_COMPLETE && stream_->IsOpen();
}

int QuicProxyDatagramClientSocket::ConnectViaStream(
    const IPEndPoint& local_address,
    const IPEndPoint& proxy_peer_address,
    std::unique_ptr<QuicChromiumClientStream::Handle> stream,
    CompletionOnceCallback callback) {
  DCHECK(connect_callback_.is_null());

  local_address_ = local_address;
  proxy_peer_address_ = proxy_peer_address;
  stream_ = std::move(stream);

  if (!stream_->IsOpen()) {
    return ERR_CONNECTION_CLOSED;
  }

  DCHECK_EQ(STATE_DISCONNECTED, next_state_);
  next_state_ = STATE_SEND_REQUEST;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    connect_callback_ = std::move(callback);
  }
  return rv;
}

int QuicProxyDatagramClientSocket::Connect(const IPEndPoint& address) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectAsync(
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingDefaultNetworkAsync(
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingNetwork(
    handles::NetworkHandle network,
    const IPEndPoint& address) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingDefaultNetwork(
    const IPEndPoint& address) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingNetworkAsync(
    handles::NetworkHandle network,
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void QuicProxyDatagramClientSocket::Close() {
  connect_callback_.Reset();
  // TODO(crbug.com/1524411): Reset read and write callbacks and buffers once
  // implemented.
  next_state_ = STATE_DISCONNECTED;

  stream_->Reset(quic::QUIC_STREAM_CANCELLED);
}

int QuicProxyDatagramClientSocket::SetReceiveBufferSize(int32_t size) {
  return OK;
}

int QuicProxyDatagramClientSocket::SetSendBufferSize(int32_t size) {
  return OK;
}

// TODO(crbug.com/1524411) Implement method.
handles::NetworkHandle QuicProxyDatagramClientSocket::GetBoundNetwork() const {
  return handles::kInvalidNetworkHandle;
}

// TODO(crbug.com/1524411): Implement method.
void QuicProxyDatagramClientSocket::ApplySocketTag(const SocketTag& tag) {}

int QuicProxyDatagramClientSocket::SetMulticastInterface(
    uint32_t interface_index) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void QuicProxyDatagramClientSocket::SetIOSNetworkServiceType(
    int ios_network_service_type) {}

int QuicProxyDatagramClientSocket::GetPeerAddress(IPEndPoint* address) const {
  *address = proxy_peer_address_;
  return OK;
}

int QuicProxyDatagramClientSocket::GetLocalAddress(IPEndPoint* address) const {
  *address = local_address_;
  return OK;
}

void QuicProxyDatagramClientSocket::UseNonBlockingIO() {
  NOTREACHED();
}

int QuicProxyDatagramClientSocket::SetDoNotFragment() {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::SetRecvTos() {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::SetTos(net::DiffServCodePoint dscp,
                                          net::EcnCodePoint ecn) {
  return OK;
}

void QuicProxyDatagramClientSocket::SetMsgConfirm(bool confirm) {
  NOTREACHED();
}

const NetLogWithSource& QuicProxyDatagramClientSocket::NetLog() const {
  return net_log_;
}

net::DscpAndEcn QuicProxyDatagramClientSocket::GetLastTos() const {
  return {net::DSCP_DEFAULT, net::ECN_DEFAULT};
}

// TODO(crbug.com/1524411): Implement method.
int QuicProxyDatagramClientSocket::Read(IOBuffer* buf,
                                        int buf_len,
                                        CompletionOnceCallback callback) {
  return ERR_NOT_IMPLEMENTED;
}

// TODO(crbug.com/1524411): Implement method.
int QuicProxyDatagramClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return ERR_NOT_IMPLEMENTED;
}

void QuicProxyDatagramClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_DISCONNECTED, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    // Connect() finished (successfully or unsuccessfully).
    DCHECK(!connect_callback_.is_null());
    std::move(connect_callback_).Run(rv);
  }
}

int QuicProxyDatagramClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_DISCONNECTED);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_DISCONNECTED;
    // TODO(crbug.com/326437102): Add support for generate auth token request
    // and complete states.
    switch (state) {
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
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_DISCONNECTED &&
           next_state_ != STATE_CONNECT_COMPLETE);
  return rv;
}

int QuicProxyDatagramClientSocket::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  if (!url_.has_host()) {
    return ERR_ADDRESS_INVALID;
  }
  std::string host = url_.host();
  int port = url_.IntPort();
  std::string host_and_port =
      url_.has_port() ? base::StrCat({host, ":", base::NumberToString(port)})
                      : host;
  request_.extra_headers.SetHeader(HttpRequestHeaders::kHost, host_and_port);

  HttpRequestHeaders authorization_headers;
  // TODO(crbug.com/326437102):  Add Proxy-Authentication headers.
  request_.extra_headers.MergeFrom(authorization_headers);

  if (proxy_delegate_) {
    HttpRequestHeaders proxy_delegate_headers;
    proxy_delegate_->OnBeforeTunnelRequest(proxy_chain(), proxy_chain_index(),
                                           &proxy_delegate_headers);
    request_.extra_headers.MergeFrom(proxy_delegate_headers);
  }

  if (!user_agent_.empty()) {
    request_.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                     user_agent_);
  }

  request_.extra_headers.SetHeader("capsule-protocol", "?1");

  // Generate a fake request line for logging purposes.
  std::string request_line =
      base::StringPrintf("CONNECT-UDP %s HTTP/3\r\n", url_.path().c_str());
  NetLogRequestHeaders(net_log_,
                       NetLogEventType::HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
                       request_line, &request_.extra_headers);

  spdy::Http2HeaderBlock headers;
  CreateSpdyHeadersFromHttpRequestForExtendedConnect(
      request_, /*priority=*/std::nullopt, "connect-udp",
      request_.extra_headers, &headers);

  return stream_->WriteHeaders(std::move(headers), false, nullptr);
}

int QuicProxyDatagramClientSocket::DoSendRequestComplete(int result) {
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

int QuicProxyDatagramClientSocket::DoReadReply() {
  next_state_ = STATE_READ_REPLY_COMPLETE;

  int rv = stream_->ReadInitialHeaders(
      &response_header_block_,
      base::BindOnce(
          &QuicProxyDatagramClientSocket::OnReadResponseHeadersComplete,
          weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    return ERR_IO_PENDING;
  }
  if (rv < 0) {
    return rv;
  }

  return ProcessResponseHeaders(response_header_block_);
}

int QuicProxyDatagramClientSocket::DoReadReplyComplete(int result) {
  if (result < 0) {
    return result;
  }

  NetLogResponseHeaders(
      net_log_, NetLogEventType::HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      response_.headers.get());

  // TODO(crbug.com/326437102): Add case for Proxy Authentication.
  if (proxy_delegate_) {
    int rv = proxy_delegate_->OnTunnelHeadersReceived(
        proxy_chain(), proxy_chain_index(), *response_.headers);
    if (rv != OK) {
      CHECK_NE(ERR_IO_PENDING, rv);
      return rv;
    }
  }

  switch (response_.headers->response_code()) {
    case 200:  // OK
      next_state_ = STATE_CONNECT_COMPLETE;
      return OK;

    default:
      // Ignore response to avoid letting the proxy impersonate the target
      // server.  (See http://crbug.com/137891.)
      return ERR_TUNNEL_CONNECTION_FAILED;
  }
}

void QuicProxyDatagramClientSocket::OnReadResponseHeadersComplete(int result) {
  // Convert the now-populated spdy::Http2HeaderBlock to HttpResponseInfo
  if (result > 0) {
    result = ProcessResponseHeaders(response_header_block_);
  }

  if (result != ERR_IO_PENDING) {
    OnIOComplete(result);
  }
}

int QuicProxyDatagramClientSocket::ProcessResponseHeaders(
    const spdy::Http2HeaderBlock& headers) {
  if (SpdyHeadersToHttpResponse(headers, &response_) != OK) {
    DLOG(WARNING) << "Invalid headers";
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  return OK;
}

}  // namespace net

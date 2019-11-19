// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_log_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_stream_parser.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/stream_socket.h"
#include "url/gurl.h"

namespace net {

const int HttpProxyClientSocket::kDrainBodyBufferSize;

HttpProxyClientSocket::HttpProxyClientSocket(
    std::unique_ptr<StreamSocket> socket,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    const ProxyServer& proxy_server,
    HttpAuthController* http_auth_controller,
    bool tunnel,
    bool using_spdy,
    NextProto negotiated_protocol,
    ProxyDelegate* proxy_delegate,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : io_callback_(base::BindRepeating(&HttpProxyClientSocket::OnIOComplete,
                                       base::Unretained(this))),
      next_state_(STATE_NONE),
      socket_(std::move(socket)),
      is_reused_(false),
      endpoint_(endpoint),
      auth_(http_auth_controller),
      tunnel_(tunnel),
      using_spdy_(using_spdy),
      negotiated_protocol_(negotiated_protocol),
      proxy_server_(proxy_server),
      proxy_delegate_(proxy_delegate),
      traffic_annotation_(traffic_annotation),
      net_log_(socket_->NetLog()) {
  // Synthesize the bits of a request that are actually used.
  request_.url = GURL("https://" + endpoint.ToString());
  request_.method = "CONNECT";
  if (!user_agent.empty())
    request_.extra_headers.SetHeader(HttpRequestHeaders::kUserAgent,
                                     user_agent);
}

HttpProxyClientSocket::~HttpProxyClientSocket() {
  Disconnect();
}

int HttpProxyClientSocket::RestartWithAuth(CompletionOnceCallback callback) {
  DCHECK_EQ(STATE_NONE, next_state_);
  DCHECK(user_callback_.is_null());

  int rv = PrepareForAuthRestart();
  if (rv != OK)
    return rv;

  rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING) {
    if (!callback.is_null())
      user_callback_ = std::move(callback);
  }

  return rv;
}

const scoped_refptr<HttpAuthController>&
HttpProxyClientSocket::GetAuthController() const {
  return auth_;
}

bool HttpProxyClientSocket::IsUsingSpdy() const {
  return using_spdy_;
}

NextProto HttpProxyClientSocket::GetProxyNegotiatedProtocol() const {
  return negotiated_protocol_;
}

const HttpResponseInfo* HttpProxyClientSocket::GetConnectResponseInfo() const {
  return response_.headers.get() ? &response_ : nullptr;
}

int HttpProxyClientSocket::Connect(CompletionOnceCallback callback) {
  DCHECK(socket_);
  DCHECK(user_callback_.is_null());

  // TODO(rch): figure out the right way to set up a tunnel with SPDY.
  // This approach sends the complete HTTPS request to the proxy
  // which allows the proxy to see "private" data.  Instead, we should
  // create an SSL tunnel to the origin server using the CONNECT method
  // inside a single SPDY stream.
  if (using_spdy_ || !tunnel_)
    next_state_ = STATE_DONE;
  if (next_state_ == STATE_DONE)
    return OK;

  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_GENERATE_AUTH_TOKEN;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = std::move(callback);
  return rv;
}

void HttpProxyClientSocket::Disconnect() {
  if (socket_)
    socket_->Disconnect();

  // Reset other states to make sure they aren't mistakenly used later.
  // These are the states initialized by Connect().
  next_state_ = STATE_NONE;
  user_callback_.Reset();
}

bool HttpProxyClientSocket::IsConnected() const {
  return next_state_ == STATE_DONE && socket_->IsConnected();
}

bool HttpProxyClientSocket::IsConnectedAndIdle() const {
  return next_state_ == STATE_DONE && socket_->IsConnectedAndIdle();
}

const NetLogWithSource& HttpProxyClientSocket::NetLog() const {
  return net_log_;
}

bool HttpProxyClientSocket::WasEverUsed() const {
  if (socket_)
    return socket_->WasEverUsed();
  NOTREACHED();
  return false;
}

bool HttpProxyClientSocket::WasAlpnNegotiated() const {
  if (socket_)
    return socket_->WasAlpnNegotiated();
  NOTREACHED();
  return false;
}

NextProto HttpProxyClientSocket::GetNegotiatedProtocol() const {
  if (socket_)
    return socket_->GetNegotiatedProtocol();
  NOTREACHED();
  return kProtoUnknown;
}

bool HttpProxyClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  if (socket_)
    return socket_->GetSSLInfo(ssl_info);
  NOTREACHED();
  return false;
}

void HttpProxyClientSocket::GetConnectionAttempts(
    ConnectionAttempts* out) const {
  out->clear();
}

int64_t HttpProxyClientSocket::GetTotalReceivedBytes() const {
  return socket_->GetTotalReceivedBytes();
}

void HttpProxyClientSocket::ApplySocketTag(const SocketTag& tag) {
  return socket_->ApplySocketTag(tag);
}

int HttpProxyClientSocket::Read(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  DCHECK(user_callback_.is_null());
  if (!CheckDone())
    return ERR_TUNNEL_CONNECTION_FAILED;

  return socket_->Read(buf, buf_len, std::move(callback));
}

int HttpProxyClientSocket::ReadIfReady(IOBuffer* buf,
                                       int buf_len,
                                       CompletionOnceCallback callback) {
  DCHECK(user_callback_.is_null());
  if (!CheckDone())
    return ERR_TUNNEL_CONNECTION_FAILED;

  return socket_->ReadIfReady(buf, buf_len, std::move(callback));
}

int HttpProxyClientSocket::CancelReadIfReady() {
  return socket_->CancelReadIfReady();
}

int HttpProxyClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_EQ(STATE_DONE, next_state_);
  DCHECK(user_callback_.is_null());

  return socket_->Write(buf, buf_len, std::move(callback), traffic_annotation);
}

int HttpProxyClientSocket::SetReceiveBufferSize(int32_t size) {
  return socket_->SetReceiveBufferSize(size);
}

int HttpProxyClientSocket::SetSendBufferSize(int32_t size) {
  return socket_->SetSendBufferSize(size);
}

int HttpProxyClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_->GetPeerAddress(address);
}

int HttpProxyClientSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_->GetLocalAddress(address);
}

int HttpProxyClientSocket::PrepareForAuthRestart() {
  if (!response_.headers.get())
    return ERR_CONNECTION_RESET;

  // If the connection can't be reused, return
  // ERR_UNABLE_TO_REUSE_CONNECTION_FOR_PROXY_AUTH.  The request will be retried
  // at a higher layer.
  if (!response_.headers->IsKeepAlive() ||
      !http_stream_parser_->CanFindEndOfResponse() || !socket_->IsConnected()) {
    socket_->Disconnect();
    return ERR_UNABLE_TO_REUSE_CONNECTION_FOR_PROXY_AUTH;
  }

  // If the auth request had a body, need to drain it before reusing the socket.
  if (!http_stream_parser_->IsResponseBodyComplete()) {
    next_state_ = STATE_DRAIN_BODY;
    drain_buf_ = base::MakeRefCounted<IOBuffer>(kDrainBodyBufferSize);
    return OK;
  }

  return DidDrainBodyForAuthRestart();
}

int HttpProxyClientSocket::DidDrainBodyForAuthRestart() {
  // Can't reuse the socket if there's still unread data on it.
  if (!socket_->IsConnectedAndIdle())
    return ERR_UNABLE_TO_REUSE_CONNECTION_FOR_PROXY_AUTH;

  next_state_ = STATE_GENERATE_AUTH_TOKEN;
  is_reused_ = true;

  // Reset the other member variables.
  drain_buf_ = nullptr;
  parser_buf_ = nullptr;
  http_stream_parser_.reset();
  request_line_.clear();
  request_headers_.Clear();
  response_ = HttpResponseInfo();
  return OK;
}

void HttpProxyClientSocket::DoCallback(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  DCHECK(!user_callback_.is_null());

  // Since Run() may result in Read being called,
  // clear user_callback_ up front.
  std::move(user_callback_).Run(result);
}

void HttpProxyClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_NONE, next_state_);
  DCHECK_NE(STATE_DONE, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int HttpProxyClientSocket::DoLoop(int last_io_result) {
  DCHECK_NE(next_state_, STATE_NONE);
  DCHECK_NE(next_state_, STATE_DONE);
  int rv = last_io_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
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
        rv = DoSendRequestComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST, rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS, rv);
        break;
      case STATE_DRAIN_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoDrainBody();
        break;
      case STATE_DRAIN_BODY_COMPLETE:
        rv = DoDrainBodyComplete(rv);
        break;
      case STATE_DONE:
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE &&
           next_state_ != STATE_DONE);
  return rv;
}

int HttpProxyClientSocket::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  return auth_->MaybeGenerateAuthToken(&request_, io_callback_, net_log_);
}

int HttpProxyClientSocket::DoGenerateAuthTokenComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int HttpProxyClientSocket::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_line_.empty()) {
    DCHECK(request_headers_.IsEmpty());

    HttpRequestHeaders extra_headers;
    if (auth_->HaveAuth())
      auth_->AddAuthorizationHeader(&extra_headers);
    // AddAuthorizationHeader() might not have added the header even if
    // HaveAuth().
    response_.did_use_http_auth =
        extra_headers.HasHeader(HttpRequestHeaders::kProxyAuthorization);

    if (proxy_delegate_) {
      HttpRequestHeaders proxy_delegate_headers;
      proxy_delegate_->OnBeforeHttp1TunnelRequest(proxy_server_,
                                                  &proxy_delegate_headers);
      extra_headers.MergeFrom(proxy_delegate_headers);
    }

    std::string user_agent;
    if (!request_.extra_headers.GetHeader(HttpRequestHeaders::kUserAgent,
                                          &user_agent)) {
      user_agent.clear();
    }
    BuildTunnelRequest(endpoint_, extra_headers, user_agent, &request_line_,
                       &request_headers_);

    NetLogRequestHeaders(net_log_,
                         NetLogEventType::HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
                         request_line_, &request_headers_);
  }

  parser_buf_ = base::MakeRefCounted<GrowableIOBuffer>();
  http_stream_parser_.reset(new HttpStreamParser(
      socket_.get(), is_reused_, &request_, parser_buf_.get(), net_log_));
  return http_stream_parser_->SendRequest(request_line_, request_headers_,
                                          traffic_annotation_, &response_,
                                          io_callback_);
}

int HttpProxyClientSocket::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int HttpProxyClientSocket::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return http_stream_parser_->ReadResponseHeaders(io_callback_);
}

int HttpProxyClientSocket::DoReadHeadersComplete(int result) {
  if (result < 0)
    return result;

  // Require the "HTTP/1.x" status line for SSL CONNECT.
  if (response_.headers->GetHttpVersion() < HttpVersion(1, 0))
    return ERR_TUNNEL_CONNECTION_FAILED;

  NetLogResponseHeaders(
      net_log_, NetLogEventType::HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      response_.headers.get());

  if (proxy_delegate_) {
    int rv = proxy_delegate_->OnHttp1TunnelHeadersReceived(proxy_server_,
                                                           *response_.headers);
    if (rv != OK) {
      DCHECK_NE(ERR_IO_PENDING, rv);
      return rv;
    }
  }

  switch (response_.headers->response_code()) {
    case 200:  // OK
      if (http_stream_parser_->IsMoreDataBuffered())
        // The proxy sent extraneous data after the headers.
        return ERR_TUNNEL_CONNECTION_FAILED;

      next_state_ = STATE_DONE;
      return OK;

      // We aren't able to CONNECT to the remote host through the proxy.  We
      // need to be very suspicious about the response because an active network
      // attacker can force us into this state by masquerading as the proxy.
      // The only safe thing to do here is to fail the connection because our
      // client is expecting an SSL protected response.
      // See http://crbug.com/7338.

    case 407:  // Proxy Authentication Required
      // We need this status code to allow proxy authentication.  Our
      // authentication code is smart enough to avoid being tricked by an
      // active network attacker.
      // The next state is intentionally not set as it should be STATE_NONE;
      if (!SanitizeProxyAuth(&response_))
        return ERR_TUNNEL_CONNECTION_FAILED;
      return HandleProxyAuthChallenge(auth_.get(), &response_, net_log_);

    default:
      // Ignore response to avoid letting the proxy impersonate the target
      // server.  (See http://crbug.com/137891.)
      // We lose something by doing this.  We have seen proxy 403, 404, and
      // 501 response bodies that contain a useful error message.  For
      // example, Squid uses a 404 response to report the DNS error: "The
      // domain name does not exist."
      return ERR_TUNNEL_CONNECTION_FAILED;
  }
}

int HttpProxyClientSocket::DoDrainBody() {
  DCHECK(drain_buf_.get());
  next_state_ = STATE_DRAIN_BODY_COMPLETE;
  return http_stream_parser_->ReadResponseBody(
      drain_buf_.get(), kDrainBodyBufferSize, io_callback_);
}

int HttpProxyClientSocket::DoDrainBodyComplete(int result) {
  if (result < 0)
    return ERR_UNABLE_TO_REUSE_CONNECTION_FOR_PROXY_AUTH;

  if (!http_stream_parser_->IsResponseBodyComplete()) {
    // Keep draining.
    next_state_ = STATE_DRAIN_BODY;
    return OK;
  }

  return DidDrainBodyForAuthRestart();
}

bool HttpProxyClientSocket::CheckDone() {
  if (next_state_ != STATE_DONE) {
    // We're trying to read the body of the response but we're still trying
    // to establish an SSL tunnel through the proxy.  We can't read these
    // bytes when establishing a tunnel because they might be controlled by
    // an active network attacker.  We don't worry about this for HTTP
    // because an active network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.
    // See http://crbug.com/8473.
    DCHECK_EQ(407, response_.headers->response_code());

    return false;
  }
  return true;
}

//----------------------------------------------------------------

}  // namespace net

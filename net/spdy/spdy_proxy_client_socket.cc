// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_proxy_client_socket.h"

#include <algorithm>  // min
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_log_util.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {

SpdyProxyClientSocket::SpdyProxyClientSocket(
    const base::WeakPtr<SpdyStream>& spdy_stream,
    const ProxyChain& proxy_chain,
    size_t proxy_chain_index,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    const NetLogWithSource& source_net_log,
    scoped_refptr<HttpAuthController> auth_controller,
    ProxyDelegate* proxy_delegate)
    : spdy_stream_(spdy_stream),
      endpoint_(endpoint),
      auth_(std::move(auth_controller)),
      proxy_chain_(proxy_chain),
      proxy_chain_index_(proxy_chain_index),
      proxy_delegate_(proxy_delegate),
      user_agent_(user_agent),
      net_log_(NetLogWithSource::Make(spdy_stream->net_log().net_log(),
                                      NetLogSourceType::PROXY_CLIENT_SOCKET)),
      source_dependency_(source_net_log.source()) {
  request_.method = "CONNECT";
  request_.url = GURL("https://" + endpoint.ToString());
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                                       source_net_log.source());
  net_log_.AddEventReferencingSource(
      NetLogEventType::HTTP2_PROXY_CLIENT_SESSION,
      spdy_stream->net_log().source());

  spdy_stream_->SetDelegate(this);
  was_ever_used_ = spdy_stream_->WasEverUsed();
}

SpdyProxyClientSocket::~SpdyProxyClientSocket() {
  Disconnect();
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

const HttpResponseInfo* SpdyProxyClientSocket::GetConnectResponseInfo() const {
  return response_.headers.get() ? &response_ : nullptr;
}

const scoped_refptr<HttpAuthController>&
SpdyProxyClientSocket::GetAuthController() const {
  return auth_;
}

int SpdyProxyClientSocket::RestartWithAuth(CompletionOnceCallback callback) {
  // A SPDY Stream can only handle a single request, so the underlying
  // stream may not be reused and a new SpdyProxyClientSocket must be
  // created (possibly on top of the same SPDY Session).
  next_state_ = STATE_DISCONNECTED;
  return ERR_UNABLE_TO_REUSE_CONNECTION_FOR_PROXY_AUTH;
}

// Ignore priority changes, just use priority of initial request. Since multiple
// requests are pooled on the SpdyProxyClientSocket, reprioritization doesn't
// really work.
//
// TODO(mmenke):  Use a single priority value for all SpdyProxyClientSockets,
// regardless of what priority they're created with.
void SpdyProxyClientSocket::SetStreamPriority(RequestPriority priority) {}

// Sends a HEADERS frame to the proxy with a CONNECT request
// for the specified endpoint.  Waits for the server to send back
// a HEADERS frame.  OK will be returned if the status is 200.
// ERR_TUNNEL_CONNECTION_FAILED will be returned for any other status.
// In any of these cases, Read() may be called to retrieve the HTTP
// response body.  Any other return values should be considered fatal.
// TODO(rch): handle 407 proxy auth requested correctly, perhaps
// by creating a new stream for the subsequent request.
// TODO(rch): create a more appropriate error code to disambiguate
// the HTTPS Proxy tunnel failure from an HTTP Proxy tunnel failure.
int SpdyProxyClientSocket::Connect(CompletionOnceCallback callback) {
  DCHECK(read_callback_.is_null());
  if (next_state_ == STATE_OPEN)
    return OK;

  DCHECK_EQ(STATE_DISCONNECTED, next_state_);
  next_state_ = STATE_GENERATE_AUTH_TOKEN;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    read_callback_ = std::move(callback);
  return rv;
}

void SpdyProxyClientSocket::Disconnect() {
  read_buffer_queue_.Clear();
  user_buffer_ = nullptr;
  user_buffer_len_ = 0;
  read_callback_.Reset();

  write_buffer_len_ = 0;
  write_callback_.Reset();

  next_state_ = STATE_DISCONNECTED;

  if (spdy_stream_.get()) {
    // This will cause OnClose to be invoked, which takes care of
    // cleaning up all the internal state.
    spdy_stream_->Cancel(ERR_ABORTED);
    DCHECK(!spdy_stream_.get());
  }
}

bool SpdyProxyClientSocket::IsConnected() const {
  return next_state_ == STATE_OPEN;
}

bool SpdyProxyClientSocket::IsConnectedAndIdle() const {
  return IsConnected() && read_buffer_queue_.IsEmpty() &&
      spdy_stream_->IsOpen();
}

const NetLogWithSource& SpdyProxyClientSocket::NetLog() const {
  return net_log_;
}

bool SpdyProxyClientSocket::WasEverUsed() const {
  return was_ever_used_ || (spdy_stream_.get() && spdy_stream_->WasEverUsed());
}

NextProto SpdyProxyClientSocket::GetNegotiatedProtocol() const {
  // Do not delegate to `spdy_stream_`. While `spdy_stream_` negotiated ALPN
  // with the proxy, this object represents the tunneled TCP connection to the
  // origin.
  return kProtoUnknown;
}

bool SpdyProxyClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  // Do not delegate to `spdy_stream_`. While `spdy_stream_` connected to the
  // proxy with TLS, this object represents the tunneled TCP connection to the
  // origin.
  return false;
}

int64_t SpdyProxyClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void SpdyProxyClientSocket::ApplySocketTag(const SocketTag& tag) {
  // In the case of a connection to the proxy using HTTP/2 or HTTP/3 where the
  // underlying socket may multiplex multiple streams, applying this request's
  // socket tag to the multiplexed session would incorrectly apply the socket
  // tag to all mutliplexed streams. Fortunately socket tagging is only
  // supported on Android without the data reduction proxy, so only simple HTTP
  // proxies are supported, so proxies won't be using HTTP/2 or HTTP/3. Enforce
  // that a specific (non-default) tag isn't being applied.
  CHECK(tag == SocketTag());
}

int SpdyProxyClientSocket::Read(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  int rv = ReadIfReady(buf, buf_len, std::move(callback));
  if (rv == ERR_IO_PENDING) {
    user_buffer_ = buf;
    user_buffer_len_ = static_cast<size_t>(buf_len);
  }
  return rv;
}

int SpdyProxyClientSocket::ReadIfReady(IOBuffer* buf,
                                       int buf_len,
                                       CompletionOnceCallback callback) {
  DCHECK(!read_callback_);
  DCHECK(!user_buffer_);

  if (next_state_ == STATE_DISCONNECTED)
    return ERR_SOCKET_NOT_CONNECTED;

  if (next_state_ == STATE_CLOSED && read_buffer_queue_.IsEmpty()) {
    return 0;
  }

  DCHECK(next_state_ == STATE_OPEN || next_state_ == STATE_CLOSED);
  DCHECK(buf);
  size_t result = PopulateUserReadBuffer(buf->data(), buf_len);
  if (result == 0) {
    read_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }
  return result;
}

int SpdyProxyClientSocket::CancelReadIfReady() {
  // Only a pending ReadIfReady() can be canceled.
  DCHECK(!user_buffer_) << "Pending Read() cannot be canceled";
  read_callback_.Reset();
  return OK;
}

size_t SpdyProxyClientSocket::PopulateUserReadBuffer(char* data, size_t len) {
  return read_buffer_queue_.Dequeue(data, len);
}

int SpdyProxyClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(write_callback_.is_null());
  if (next_state_ != STATE_OPEN)
    return ERR_SOCKET_NOT_CONNECTED;
  if (end_stream_state_ == EndStreamState::kEndStreamSent)
    return ERR_CONNECTION_CLOSED;

  DCHECK(spdy_stream_.get());
  spdy_stream_->SendData(buf, buf_len, MORE_DATA_TO_SEND);
  net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_SENT, buf_len,
                                buf->data());
  write_callback_ = std::move(callback);
  write_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

int SpdyProxyClientSocket::SetReceiveBufferSize(int32_t size) {
  // Since this StreamSocket sits on top of a shared SpdySession, it
  // is not safe for callers to change this underlying socket.
  return ERR_NOT_IMPLEMENTED;
}

int SpdyProxyClientSocket::SetSendBufferSize(int32_t size) {
  // Since this StreamSocket sits on top of a shared SpdySession, it
  // is not safe for callers to change this underlying socket.
  return ERR_NOT_IMPLEMENTED;
}

int SpdyProxyClientSocket::GetPeerAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  return spdy_stream_->GetPeerAddress(address);
}

int SpdyProxyClientSocket::GetLocalAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  return spdy_stream_->GetLocalAddress(address);
}

void SpdyProxyClientSocket::RunWriteCallback(int result) {
  base::WeakPtr<SpdyProxyClientSocket> weak_ptr = weak_factory_.GetWeakPtr();
  // `write_callback_` might be consumed by OnClose().
  if (write_callback_) {
    std::move(write_callback_).Run(result);
  }
  if (!weak_ptr) {
    // `this` was already destroyed while running `write_callback_`. Must
    // return immediately without touching any field member.
    return;
  }

  if (end_stream_state_ == EndStreamState::kEndStreamReceived) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SpdyProxyClientSocket::MaybeSendEndStream,
                                  weak_factory_.GetMutableWeakPtr()));
  }
}

void SpdyProxyClientSocket::OnIOComplete(int result) {
  DCHECK_NE(STATE_DISCONNECTED, next_state_);
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    std::move(read_callback_).Run(rv);
  }
}

int SpdyProxyClientSocket::DoLoop(int last_io_result) {
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
        if (rv >= 0 || rv == ERR_IO_PENDING) {
          // Emit extra event so can use the same events as
          // HttpProxyClientSocket.
          net_log_.BeginEvent(
              NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS);
        }
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
           next_state_ != STATE_OPEN);
  return rv;
}

int SpdyProxyClientSocket::DoGenerateAuthToken() {
  next_state_ = STATE_GENERATE_AUTH_TOKEN_COMPLETE;
  return auth_->MaybeGenerateAuthToken(
      &request_,
      base::BindOnce(&SpdyProxyClientSocket::OnIOComplete,
                     weak_factory_.GetWeakPtr()),
      net_log_);
}

int SpdyProxyClientSocket::DoGenerateAuthTokenComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int SpdyProxyClientSocket::DoSendRequest() {
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

  return spdy_stream_->SendRequestHeaders(std::move(headers),
                                          MORE_DATA_TO_SEND);
}

int SpdyProxyClientSocket::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  // Wait for HEADERS frame from the server
  next_state_ = STATE_READ_REPLY_COMPLETE;
  return ERR_IO_PENDING;
}

int SpdyProxyClientSocket::DoReadReplyComplete(int result) {
  // We enter this method directly from DoSendRequestComplete, since
  // we are notified by a callback when the HEADERS frame arrives.

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
      next_state_ = STATE_OPEN;
      return OK;

    case 407:  // Proxy Authentication Required
      next_state_ = STATE_OPEN;
      SanitizeProxyAuth(response_);
      return HandleProxyAuthChallenge(auth_.get(), &response_, net_log_);

    default:
      // Ignore response to avoid letting the proxy impersonate the target
      // server.  (See http://crbug.com/137891.)
      return ERR_TUNNEL_CONNECTION_FAILED;
  }
}

// SpdyStream::Delegate methods:
// Called when SYN frame has been sent.
// Returns true if no more data to be sent after SYN frame.
void SpdyProxyClientSocket::OnHeadersSent() {
  DCHECK_EQ(next_state_, STATE_SEND_REQUEST_COMPLETE);

  OnIOComplete(OK);
}

void SpdyProxyClientSocket::OnEarlyHintsReceived(
    const quiche::HttpHeaderBlock& headers) {}

void SpdyProxyClientSocket::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  // If we've already received the reply, existing headers are too late.
  // TODO(mbelshe): figure out a way to make HEADERS frames useful after the
  //                initial response.
  if (next_state_ != STATE_READ_REPLY_COMPLETE)
    return;

  // Save the response
  const int rv = SpdyHeadersToHttpResponse(response_headers, &response_);
  DCHECK_NE(rv, ERR_INCOMPLETE_HTTP2_HEADERS);

  OnIOComplete(OK);
}

// Called when data is received or on EOF (if `buffer is nullptr).
void SpdyProxyClientSocket::OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) {
  if (buffer) {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED,
                                  buffer->GetRemainingSize(),
                                  buffer->GetRemainingData());
    read_buffer_queue_.Enqueue(std::move(buffer));
  } else {
    net_log_.AddByteTransferEvent(NetLogEventType::SOCKET_BYTES_RECEIVED, 0,
                                  nullptr);

    if (end_stream_state_ == EndStreamState::kNone) {
      // The peer sent END_STREAM. Schedule a DATA frame with END_STREAM.
      end_stream_state_ = EndStreamState::kEndStreamReceived;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&SpdyProxyClientSocket::MaybeSendEndStream,
                                    weak_factory_.GetWeakPtr()));
    }
  }

  if (read_callback_) {
    if (user_buffer_) {
      int rv = PopulateUserReadBuffer(user_buffer_->data(), user_buffer_len_);
      user_buffer_ = nullptr;
      user_buffer_len_ = 0;
      std::move(read_callback_).Run(rv);
    } else {
      // If ReadIfReady() is used instead of Read(), tell the caller that data
      // is available for reading.
      std::move(read_callback_).Run(OK);
    }
  }
}

void SpdyProxyClientSocket::OnDataSent() {
  if (end_stream_state_ == EndStreamState::kEndStreamSent) {
    CHECK(write_callback_.is_null());
    return;
  }

  DCHECK(!write_callback_.is_null());

  int rv = write_buffer_len_;
  write_buffer_len_ = 0;

  // Proxy write callbacks result in deep callback chains. Post to allow the
  // stream's write callback chain to unwind (see crbug.com/355511).
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SpdyProxyClientSocket::RunWriteCallback,
                                weak_factory_.GetWeakPtr(), rv));
}

void SpdyProxyClientSocket::OnTrailers(
    const quiche::HttpHeaderBlock& trailers) {
  // |spdy_stream_| is of type SPDY_BIDIRECTIONAL_STREAM, so trailers are
  // combined with response headers and this method will not be calld.
  NOTREACHED_IN_MIGRATION();
}

void SpdyProxyClientSocket::OnClose(int status)  {
  was_ever_used_ = spdy_stream_->WasEverUsed();
  spdy_stream_.reset();

  bool connecting = next_state_ != STATE_DISCONNECTED &&
      next_state_ < STATE_OPEN;
  if (next_state_ == STATE_OPEN)
    next_state_ = STATE_CLOSED;
  else
    next_state_ = STATE_DISCONNECTED;

  base::WeakPtr<SpdyProxyClientSocket> weak_ptr = weak_factory_.GetWeakPtr();
  CompletionOnceCallback write_callback = std::move(write_callback_);
  write_buffer_len_ = 0;

  // If we're in the middle of connecting, we need to make sure
  // we invoke the connect callback.
  if (connecting) {
    DCHECK(!read_callback_.is_null());
    std::move(read_callback_).Run(status);
  } else if (!read_callback_.is_null()) {
    // If we have a read_callback_, the we need to make sure we call it back.
    OnDataReceived(std::unique_ptr<SpdyBuffer>());
  }
  // This may have been deleted by read_callback_, so check first.
  if (weak_ptr.get() && !write_callback.is_null())
    std::move(write_callback).Run(ERR_CONNECTION_CLOSED);
}

bool SpdyProxyClientSocket::CanGreaseFrameType() const {
  return false;
}

NetLogSource SpdyProxyClientSocket::source_dependency() const {
  return source_dependency_;
}

void SpdyProxyClientSocket::MaybeSendEndStream() {
  DCHECK_NE(end_stream_state_, EndStreamState::kNone);
  if (end_stream_state_ == EndStreamState::kEndStreamSent)
    return;

  if (!spdy_stream_)
    return;

  // When there is a pending write, wait until the write completes.
  if (write_callback_)
    return;

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(/*buffer_size=*/0);
  spdy_stream_->SendData(buffer.get(), /*length=*/0, NO_MORE_DATA_TO_SEND);
  end_stream_state_ = EndStreamState::kEndStreamSent;
}

}  // namespace net

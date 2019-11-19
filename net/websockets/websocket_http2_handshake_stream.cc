// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_http2_handshake_stream.h"

#include <cstddef>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_deflate_predictor_impl.h"
#include "net/websockets/websocket_deflate_stream.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_request_info.h"

namespace net {

namespace {

bool ValidateStatus(const HttpResponseHeaders* headers) {
  return headers->GetStatusLine() == "HTTP/1.1 200";
}

}  // namespace

WebSocketHttp2HandshakeStream::WebSocketHttp2HandshakeStream(
    base::WeakPtr<SpdySession> session,
    WebSocketStream::ConnectDelegate* connect_delegate,
    std::vector<std::string> requested_sub_protocols,
    std::vector<std::string> requested_extensions,
    WebSocketStreamRequestAPI* request)
    : result_(HandshakeResult::HTTP2_INCOMPLETE),
      session_(session),
      connect_delegate_(connect_delegate),
      http_response_info_(nullptr),
      requested_sub_protocols_(requested_sub_protocols),
      requested_extensions_(requested_extensions),
      stream_request_(request),
      request_info_(nullptr),
      stream_closed_(false),
      stream_error_(OK),
      response_headers_complete_(false) {
  DCHECK(connect_delegate);
  DCHECK(request);
}

WebSocketHttp2HandshakeStream::~WebSocketHttp2HandshakeStream() {
  spdy_stream_request_.reset();
  RecordHandshakeResult(result_);
}

int WebSocketHttp2HandshakeStream::InitializeStream(
    const HttpRequestInfo* request_info,
    bool can_send_early,
    RequestPriority priority,
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback) {
  DCHECK(request_info->traffic_annotation.is_valid());
  request_info_ = request_info;
  priority_ = priority;
  net_log_ = net_log;
  return OK;
}

int WebSocketHttp2HandshakeStream::SendRequest(
    const HttpRequestHeaders& headers,
    HttpResponseInfo* response,
    CompletionOnceCallback callback) {
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketKey));
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketProtocol));
  DCHECK(!headers.HasHeader(websockets::kSecWebSocketExtensions));
  DCHECK(headers.HasHeader(HttpRequestHeaders::kOrigin));
  DCHECK(headers.HasHeader(websockets::kUpgrade));
  DCHECK(headers.HasHeader(HttpRequestHeaders::kConnection));
  DCHECK(headers.HasHeader(websockets::kSecWebSocketVersion));

  if (!session_) {
    OnFailure("Connection closed before sending request.");
    return ERR_CONNECTION_CLOSED;
  }

  http_response_info_ = response;

  IPEndPoint address;
  int result = session_->GetPeerAddress(&address);
  if (result != OK) {
    OnFailure("Error getting IP address.");
    return result;
  }
  http_response_info_->remote_endpoint = address;

  auto request = std::make_unique<WebSocketHandshakeRequestInfo>(
      request_info_->url, base::Time::Now());
  request->headers.CopyFrom(headers);

  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketExtensions,
                            requested_extensions_, &request->headers);
  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketProtocol,
                            requested_sub_protocols_, &request->headers);

  CreateSpdyHeadersFromHttpRequestForWebSocket(
      request_info_->url, request->headers, &http2_request_headers_);

  connect_delegate_->OnStartOpeningHandshake(std::move(request));

  callback_ = std::move(callback);
  spdy_stream_request_ = std::make_unique<SpdyStreamRequest>();
  // The initial request for the WebSocket is a CONNECT, so there is no need to
  // call ConfirmHandshake().
  int rv = spdy_stream_request_->StartRequest(
      SPDY_BIDIRECTIONAL_STREAM, session_, request_info_->url, true, priority_,
      request_info_->socket_tag, net_log_,
      base::BindOnce(&WebSocketHttp2HandshakeStream::StartRequestCallback,
                     base::Unretained(this)),
      NetworkTrafficAnnotationTag(request_info_->traffic_annotation));
  if (rv == OK) {
    StartRequestCallback(rv);
    return ERR_IO_PENDING;
  }
  return rv;
}

int WebSocketHttp2HandshakeStream::ReadResponseHeaders(
    CompletionOnceCallback callback) {
  if (stream_closed_)
    return stream_error_;

  if (response_headers_complete_)
    return ValidateResponse();

  callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int WebSocketHttp2HandshakeStream::ReadResponseBody(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback) {
  // Callers should instead call Upgrade() to get a WebSocketStream
  // and call ReadFrames() on that.
  NOTREACHED();
  return OK;
}

void WebSocketHttp2HandshakeStream::Close(bool not_reusable) {
  spdy_stream_request_.reset();
  if (stream_) {
    stream_ = nullptr;
    stream_closed_ = true;
    stream_error_ = ERR_CONNECTION_CLOSED;
  }
  stream_adapter_.reset();
}

bool WebSocketHttp2HandshakeStream::IsResponseBodyComplete() const {
  return false;
}

bool WebSocketHttp2HandshakeStream::IsConnectionReused() const {
  return true;
}

void WebSocketHttp2HandshakeStream::SetConnectionReused() {}

bool WebSocketHttp2HandshakeStream::CanReuseConnection() const {
  return false;
}

int64_t WebSocketHttp2HandshakeStream::GetTotalReceivedBytes() const {
  return stream_ ? stream_->raw_received_bytes() : 0;
}

int64_t WebSocketHttp2HandshakeStream::GetTotalSentBytes() const {
  return stream_ ? stream_->raw_sent_bytes() : 0;
}

bool WebSocketHttp2HandshakeStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  return false;
}

bool WebSocketHttp2HandshakeStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return stream_ && stream_->GetLoadTimingInfo(load_timing_info);
}

void WebSocketHttp2HandshakeStream::GetSSLInfo(SSLInfo* ssl_info) {
  if (stream_)
    stream_->GetSSLInfo(ssl_info);
}

void WebSocketHttp2HandshakeStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  // A multiplexed stream cannot request client certificates. Client
  // authentication may only occur during the initial SSL handshake.
  NOTREACHED();
}

bool WebSocketHttp2HandshakeStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return session_ && session_->GetRemoteEndpoint(endpoint);
}

void WebSocketHttp2HandshakeStream::PopulateNetErrorDetails(
    NetErrorDetails* /*details*/) {
  return;
}

void WebSocketHttp2HandshakeStream::Drain(HttpNetworkSession* session) {
  Close(true /* not_reusable */);
}

void WebSocketHttp2HandshakeStream::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (stream_)
    stream_->SetPriority(priority_);
}

HttpStream* WebSocketHttp2HandshakeStream::RenewStreamForAuth() {
  // Renewing the stream is not supported.
  return nullptr;
}

std::unique_ptr<WebSocketStream> WebSocketHttp2HandshakeStream::Upgrade() {
  DCHECK(extension_params_.get());

  stream_adapter_->DetachDelegate();
  std::unique_ptr<WebSocketStream> basic_stream =
      std::make_unique<WebSocketBasicStream>(
          std::move(stream_adapter_), nullptr, sub_protocol_, extensions_);

  if (!extension_params_->deflate_enabled)
    return basic_stream;

  return std::make_unique<WebSocketDeflateStream>(
      std::move(basic_stream), extension_params_->deflate_parameters,
      std::make_unique<WebSocketDeflatePredictorImpl>());
}

base::WeakPtr<WebSocketHandshakeStreamBase>
WebSocketHttp2HandshakeStream::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebSocketHttp2HandshakeStream::OnHeadersSent() {
  std::move(callback_).Run(OK);
}

void WebSocketHttp2HandshakeStream::OnHeadersReceived(
    const spdy::SpdyHeaderBlock& response_headers) {
  DCHECK(!response_headers_complete_);
  DCHECK(http_response_info_);

  response_headers_complete_ = true;

  const bool headers_valid =
      SpdyHeadersToHttpResponse(response_headers, http_response_info_);
  DCHECK(headers_valid);

  http_response_info_->response_time = stream_->response_time();
  // Do not store SSLInfo in the response here, HttpNetworkTransaction will take
  // care of that part.
  http_response_info_->was_alpn_negotiated = true;
  http_response_info_->request_time = stream_->GetRequestTime();
  http_response_info_->connection_info =
      HttpResponseInfo::CONNECTION_INFO_HTTP2;
  http_response_info_->alpn_negotiated_protocol =
      HttpResponseInfo::ConnectionInfoToString(
          http_response_info_->connection_info);
  http_response_info_->vary_data.Init(*request_info_,
                                      *http_response_info_->headers.get());

  if (callback_)
    std::move(callback_).Run(ValidateResponse());
}

void WebSocketHttp2HandshakeStream::OnClose(int status) {
  DCHECK(stream_adapter_);
  DCHECK_GT(ERR_IO_PENDING, status);

  stream_closed_ = true;
  stream_error_ = status;
  stream_ = nullptr;

  stream_adapter_.reset();

  // If response headers have already been received,
  // then ValidateResponse() sets |result_|.
  if (!response_headers_complete_)
    result_ = HandshakeResult::HTTP2_FAILED;

  OnFailure(std::string("Stream closed with error: ") + ErrorToString(status));

  if (callback_)
    std::move(callback_).Run(status);
}

void WebSocketHttp2HandshakeStream::StartRequestCallback(int rv) {
  DCHECK(callback_);
  if (rv != OK) {
    spdy_stream_request_.reset();
    std::move(callback_).Run(rv);
    return;
  }
  stream_ = spdy_stream_request_->ReleaseStream();
  spdy_stream_request_.reset();
  stream_adapter_ =
      std::make_unique<WebSocketSpdyStreamAdapter>(stream_, this, net_log_);
  rv = stream_->SendRequestHeaders(std::move(http2_request_headers_),
                                   MORE_DATA_TO_SEND);
  // SendRequestHeaders() always returns asynchronously,
  // and instead of taking a callback, it calls OnHeadersSent().
  DCHECK_EQ(ERR_IO_PENDING, rv);
}

int WebSocketHttp2HandshakeStream::ValidateResponse() {
  DCHECK(http_response_info_);
  const HttpResponseHeaders* headers = http_response_info_->headers.get();
  const int response_code = headers->response_code();
  switch (response_code) {
    case HTTP_OK:
      OnFinishOpeningHandshake();
      return ValidateUpgradeResponse(headers);

    // We need to pass these through for authentication to work.
    case HTTP_UNAUTHORIZED:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return OK;

    // Other status codes are potentially risky (see the warnings in the
    // WHATWG WebSocket API spec) and so are dropped by default.
    default:
      OnFailure(base::StringPrintf(
          "Error during WebSocket handshake: Unexpected response code: %d",
          headers->response_code()));
      OnFinishOpeningHandshake();
      result_ = HandshakeResult::HTTP2_INVALID_STATUS;
      return ERR_INVALID_RESPONSE;
  }
}

int WebSocketHttp2HandshakeStream::ValidateUpgradeResponse(
    const HttpResponseHeaders* headers) {
  extension_params_ = std::make_unique<WebSocketExtensionParams>();
  std::string failure_message;
  if (!ValidateStatus(headers)) {
    result_ = HandshakeResult::HTTP2_INVALID_STATUS;
  } else if (!ValidateSubProtocol(headers, requested_sub_protocols_,
                                  &sub_protocol_, &failure_message)) {
    result_ = HandshakeResult::HTTP2_FAILED_SUBPROTO;
  } else if (!ValidateExtensions(headers, &extensions_, &failure_message,
                                 extension_params_.get())) {
    result_ = HandshakeResult::HTTP2_FAILED_EXTENSIONS;
  } else {
    result_ = HandshakeResult::HTTP2_CONNECTED;
    return OK;
  }
  OnFailure("Error during WebSocket handshake: " + failure_message);
  return ERR_INVALID_RESPONSE;
}

void WebSocketHttp2HandshakeStream::OnFinishOpeningHandshake() {
  DCHECK(http_response_info_);
  WebSocketDispatchOnFinishOpeningHandshake(
      connect_delegate_, request_info_->url, http_response_info_->headers,
      http_response_info_->remote_endpoint, http_response_info_->response_time);
}

void WebSocketHttp2HandshakeStream::OnFailure(const std::string& message) {
  stream_request_->OnFailure(message);
}

}  // namespace net

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_handshake_stream.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "crypto/random.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_client_socket_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_basic_stream_adapters.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_deflate_predictor.h"
#include "net/websockets/websocket_deflate_predictor_impl.h"
#include "net/websockets/websocket_deflate_stream.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_handshake_challenge.h"
#include "net/websockets/websocket_handshake_constants.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_stream.h"

namespace net {

namespace {

const char kConnectionErrorStatusLine[] = "HTTP/1.1 503 Connection Error";

}  // namespace

namespace {

enum GetHeaderResult {
  GET_HEADER_OK,
  GET_HEADER_MISSING,
  GET_HEADER_MULTIPLE,
};

std::string MissingHeaderMessage(const std::string& header_name) {
  return std::string("'") + header_name + "' header is missing";
}

std::string GenerateHandshakeChallenge() {
  std::string raw_challenge(websockets::kRawChallengeLength, '\0');
  crypto::RandBytes(base::data(raw_challenge), raw_challenge.length());
  std::string encoded_challenge;
  base::Base64Encode(raw_challenge, &encoded_challenge);
  return encoded_challenge;
}

GetHeaderResult GetSingleHeaderValue(const HttpResponseHeaders* headers,
                                     const base::StringPiece& name,
                                     std::string* value) {
  size_t iter = 0;
  size_t num_values = 0;
  std::string temp_value;
  while (headers->EnumerateHeader(&iter, name, &temp_value)) {
    if (++num_values > 1)
      return GET_HEADER_MULTIPLE;
    *value = temp_value;
  }
  return num_values > 0 ? GET_HEADER_OK : GET_HEADER_MISSING;
}

bool ValidateHeaderHasSingleValue(GetHeaderResult result,
                                  const std::string& header_name,
                                  std::string* failure_message) {
  if (result == GET_HEADER_MISSING) {
    *failure_message = MissingHeaderMessage(header_name);
    return false;
  }
  if (result == GET_HEADER_MULTIPLE) {
    *failure_message =
        WebSocketHandshakeStreamBase::MultipleHeaderValuesMessage(header_name);
    return false;
  }
  DCHECK_EQ(result, GET_HEADER_OK);
  return true;
}

bool ValidateUpgrade(const HttpResponseHeaders* headers,
                     std::string* failure_message) {
  std::string value;
  GetHeaderResult result =
      GetSingleHeaderValue(headers, websockets::kUpgrade, &value);
  if (!ValidateHeaderHasSingleValue(result,
                                    websockets::kUpgrade,
                                    failure_message)) {
    return false;
  }

  if (!base::LowerCaseEqualsASCII(value, websockets::kWebSocketLowercase)) {
    *failure_message =
        "'Upgrade' header value is not 'WebSocket': " + value;
    return false;
  }
  return true;
}

bool ValidateSecWebSocketAccept(const HttpResponseHeaders* headers,
                                const std::string& expected,
                                std::string* failure_message) {
  std::string actual;
  GetHeaderResult result =
      GetSingleHeaderValue(headers, websockets::kSecWebSocketAccept, &actual);
  if (!ValidateHeaderHasSingleValue(result,
                                    websockets::kSecWebSocketAccept,
                                    failure_message)) {
    return false;
  }

  if (expected != actual) {
    *failure_message = "Incorrect 'Sec-WebSocket-Accept' header value";
    return false;
  }
  return true;
}

bool ValidateConnection(const HttpResponseHeaders* headers,
                        std::string* failure_message) {
  // Connection header is permitted to contain other tokens.
  if (!headers->HasHeader(HttpRequestHeaders::kConnection)) {
    *failure_message = MissingHeaderMessage(HttpRequestHeaders::kConnection);
    return false;
  }
  if (!headers->HasHeaderValue(HttpRequestHeaders::kConnection,
                               websockets::kUpgrade)) {
    *failure_message = "'Connection' header value must contain 'Upgrade'";
    return false;
  }
  return true;
}

}  // namespace

WebSocketBasicHandshakeStream::WebSocketBasicHandshakeStream(
    std::unique_ptr<ClientSocketHandle> connection,
    WebSocketStream::ConnectDelegate* connect_delegate,
    bool using_proxy,
    std::vector<std::string> requested_sub_protocols,
    std::vector<std::string> requested_extensions,
    WebSocketStreamRequestAPI* request,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager)
    : result_(HandshakeResult::INCOMPLETE),
      state_(std::move(connection), using_proxy),
      connect_delegate_(connect_delegate),
      http_response_info_(nullptr),
      requested_sub_protocols_(std::move(requested_sub_protocols)),
      requested_extensions_(std::move(requested_extensions)),
      stream_request_(request),
      websocket_endpoint_lock_manager_(websocket_endpoint_lock_manager) {
  DCHECK(connect_delegate);
  DCHECK(request);
}

WebSocketBasicHandshakeStream::~WebSocketBasicHandshakeStream() {
  // Some members are "stolen" by RenewStreamForAuth() and should not be touched
  // here. Particularly |connect_delegate_|, |stream_request_|, and
  // |websocket_endpoint_lock_manager_|.

  // TODO(ricea): What's the right thing to do here if we renewed the stream for
  // auth? Currently we record it as INCOMPLETE.
  RecordHandshakeResult(result_);
}

int WebSocketBasicHandshakeStream::InitializeStream(
    const HttpRequestInfo* request_info,
    bool can_send_early,
    RequestPriority priority,
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback) {
  DCHECK(request_info->traffic_annotation.is_valid());
  url_ = request_info->url;
  // The WebSocket may receive a socket in the early data state from
  // HttpNetworkTransaction, which means it must call ConfirmHandshake() for
  // requests that need replay protection. However, the first request on any
  // WebSocket stream is a GET with an idempotent request
  // (https://tools.ietf.org/html/rfc6455#section-1.3), so there is no need to
  // call ConfirmHandshake().
  //
  // Data after the WebSockets handshake may not be replayable, but the
  // handshake is guaranteed to be confirmed once the HTTP response is received.
  DCHECK(can_send_early);
  state_.Initialize(request_info, priority, net_log);
  return OK;
}

int WebSocketBasicHandshakeStream::SendRequest(
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
  DCHECK(parser());

  http_response_info_ = response;

  // Create a copy of the headers object, so that we can add the
  // Sec-WebSockey-Key header.
  HttpRequestHeaders enriched_headers;
  enriched_headers.CopyFrom(headers);
  std::string handshake_challenge;
  if (handshake_challenge_for_testing_.has_value()) {
    handshake_challenge = handshake_challenge_for_testing_.value();
    handshake_challenge_for_testing_.reset();
  } else {
    handshake_challenge = GenerateHandshakeChallenge();
  }
  enriched_headers.SetHeader(websockets::kSecWebSocketKey, handshake_challenge);

  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketExtensions,
                            requested_extensions_,
                            &enriched_headers);
  AddVectorHeaderIfNonEmpty(websockets::kSecWebSocketProtocol,
                            requested_sub_protocols_,
                            &enriched_headers);

  handshake_challenge_response_ =
      ComputeSecWebSocketAccept(handshake_challenge);

  DCHECK(connect_delegate_);
  auto request =
      std::make_unique<WebSocketHandshakeRequestInfo>(url_, base::Time::Now());
  request->headers.CopyFrom(enriched_headers);
  connect_delegate_->OnStartOpeningHandshake(std::move(request));

  return parser()->SendRequest(
      state_.GenerateRequestLine(), enriched_headers,
      NetworkTrafficAnnotationTag(state_.traffic_annotation()), response,
      std::move(callback));
}

int WebSocketBasicHandshakeStream::ReadResponseHeaders(
    CompletionOnceCallback callback) {
  // HttpStreamParser uses a weak pointer when reading from the
  // socket, so it won't be called back after being destroyed. The
  // HttpStreamParser is owned by HttpBasicState which is owned by this object,
  // so this use of base::Unretained() is safe.
  int rv = parser()->ReadResponseHeaders(base::BindOnce(
      &WebSocketBasicHandshakeStream::ReadResponseHeadersCallback,
      base::Unretained(this), std::move(callback)));
  if (rv == ERR_IO_PENDING)
    return rv;
  return ValidateResponse(rv);
}

int WebSocketBasicHandshakeStream::ReadResponseBody(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback) {
  return parser()->ReadResponseBody(buf, buf_len, std::move(callback));
}

void WebSocketBasicHandshakeStream::Close(bool not_reusable) {
  // This class ignores the value of |not_reusable| and never lets the socket be
  // re-used.
  if (!parser())
    return;
  StreamSocket* socket = state_.connection()->socket();
  if (socket)
    socket->Disconnect();
  state_.connection()->Reset();
}

bool WebSocketBasicHandshakeStream::IsResponseBodyComplete() const {
  return parser()->IsResponseBodyComplete();
}

bool WebSocketBasicHandshakeStream::IsConnectionReused() const {
  return state_.IsConnectionReused();
}

void WebSocketBasicHandshakeStream::SetConnectionReused() {
  state_.connection()->set_reuse_type(ClientSocketHandle::REUSED_IDLE);
}

bool WebSocketBasicHandshakeStream::CanReuseConnection() const {
  return parser() && state_.connection()->socket() &&
         parser()->CanReuseConnection();
}

int64_t WebSocketBasicHandshakeStream::GetTotalReceivedBytes() const {
  return 0;
}

int64_t WebSocketBasicHandshakeStream::GetTotalSentBytes() const {
  return 0;
}

bool WebSocketBasicHandshakeStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  return false;
}

bool WebSocketBasicHandshakeStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return state_.connection()->GetLoadTimingInfo(IsConnectionReused(),
                                                load_timing_info);
}

void WebSocketBasicHandshakeStream::GetSSLInfo(SSLInfo* ssl_info) {
  if (!state_.connection()->socket()) {
    ssl_info->Reset();
    return;
  }
  parser()->GetSSLInfo(ssl_info);
}

void WebSocketBasicHandshakeStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  if (!state_.connection()->socket()) {
    cert_request_info->Reset();
    return;
  }
  parser()->GetSSLCertRequestInfo(cert_request_info);
}

bool WebSocketBasicHandshakeStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  if (!state_.connection() || !state_.connection()->socket())
    return false;

  return state_.connection()->socket()->GetPeerAddress(endpoint) == OK;
}

void WebSocketBasicHandshakeStream::PopulateNetErrorDetails(
    NetErrorDetails* /*details*/) {
  return;
}

void WebSocketBasicHandshakeStream::Drain(HttpNetworkSession* session) {
  HttpResponseBodyDrainer* drainer = new HttpResponseBodyDrainer(this);
  drainer->Start(session);
  // |drainer| will delete itself.
}

void WebSocketBasicHandshakeStream::SetPriority(RequestPriority priority) {
  // TODO(ricea): See TODO comment in HttpBasicStream::SetPriority(). If it is
  // gone, then copy whatever has happened there over here.
}

HttpStream* WebSocketBasicHandshakeStream::RenewStreamForAuth() {
  DCHECK(IsResponseBodyComplete());
  DCHECK(!parser()->IsMoreDataBuffered());
  // The HttpStreamParser object still has a pointer to the connection. Just to
  // be extra-sure it doesn't touch the connection again, delete it here rather
  // than leaving it until the destructor is called.
  state_.DeleteParser();

  auto handshake_stream = std::make_unique<WebSocketBasicHandshakeStream>(
      state_.ReleaseConnection(), connect_delegate_, state_.using_proxy(),
      std::move(requested_sub_protocols_), std::move(requested_extensions_),
      stream_request_, websocket_endpoint_lock_manager_);

  stream_request_->OnBasicHandshakeStreamCreated(handshake_stream.get());

  return handshake_stream.release();
}

std::unique_ptr<WebSocketStream> WebSocketBasicHandshakeStream::Upgrade() {
  // The HttpStreamParser object has a pointer to our ClientSocketHandle. Make
  // sure it does not touch it again before it is destroyed.
  state_.DeleteParser();
  WebSocketTransportClientSocketPool::UnlockEndpoint(
      state_.connection(), websocket_endpoint_lock_manager_);
  std::unique_ptr<WebSocketStream> basic_stream =
      std::make_unique<WebSocketBasicStream>(
          std::make_unique<WebSocketClientSocketHandleAdapter>(
              state_.ReleaseConnection()),
          state_.read_buf(), sub_protocol_, extensions_);
  DCHECK(extension_params_.get());
  if (extension_params_->deflate_enabled) {
    return std::make_unique<WebSocketDeflateStream>(
        std::move(basic_stream), extension_params_->deflate_parameters,
        std::make_unique<WebSocketDeflatePredictorImpl>());
  }

  return basic_stream;
}

base::WeakPtr<WebSocketHandshakeStreamBase>
WebSocketBasicHandshakeStream::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebSocketBasicHandshakeStream::SetWebSocketKeyForTesting(
    const std::string& key) {
  handshake_challenge_for_testing_ = key;
}

void WebSocketBasicHandshakeStream::ReadResponseHeadersCallback(
    CompletionOnceCallback callback,
    int result) {
  std::move(callback).Run(ValidateResponse(result));
}

void WebSocketBasicHandshakeStream::OnFinishOpeningHandshake() {
  DCHECK(http_response_info_);
  WebSocketDispatchOnFinishOpeningHandshake(
      connect_delegate_, url_, http_response_info_->headers,
      http_response_info_->remote_endpoint, http_response_info_->response_time);
}

int WebSocketBasicHandshakeStream::ValidateResponse(int rv) {
  DCHECK(http_response_info_);
  // Most net errors happen during connection, so they are not seen by this
  // method. The histogram for error codes is created in
  // Delegate::OnResponseStarted in websocket_stream.cc instead.
  if (rv >= 0) {
    const HttpResponseHeaders* headers = http_response_info_->headers.get();
    const int response_code = headers->response_code();
    base::UmaHistogramSparse("Net.WebSocket.ResponseCode", response_code);
    switch (response_code) {
      case HTTP_SWITCHING_PROTOCOLS:
        OnFinishOpeningHandshake();
        return ValidateUpgradeResponse(headers);

      // We need to pass these through for authentication to work.
      case HTTP_UNAUTHORIZED:
      case HTTP_PROXY_AUTHENTICATION_REQUIRED:
        return OK;

      // Other status codes are potentially risky (see the warnings in the
      // WHATWG WebSocket API spec) and so are dropped by default.
      default:
        // A WebSocket server cannot be using HTTP/0.9, so if we see version
        // 0.9, it means the response was garbage.
        // Reporting "Unexpected response code: 200" in this case is not
        // helpful, so use a different error message.
        if (headers->GetHttpVersion() == HttpVersion(0, 9)) {
          OnFailure("Error during WebSocket handshake: Invalid status line");
        } else {
          OnFailure(base::StringPrintf(
              "Error during WebSocket handshake: Unexpected response code: %d",
              headers->response_code()));
        }
        OnFinishOpeningHandshake();
        result_ = HandshakeResult::INVALID_STATUS;
        return ERR_INVALID_RESPONSE;
    }
  } else {
    if (rv == ERR_EMPTY_RESPONSE) {
      OnFailure("Connection closed before receiving a handshake response");
      result_ = HandshakeResult::EMPTY_RESPONSE;
      return rv;
    }
    OnFailure(std::string("Error during WebSocket handshake: ") +
              ErrorToString(rv));
    OnFinishOpeningHandshake();
    // Some error codes (for example ERR_CONNECTION_CLOSED) get changed to OK at
    // higher levels. To prevent an unvalidated connection getting erroneously
    // upgraded, don't pass through the status code unchanged if it is
    // HTTP_SWITCHING_PROTOCOLS.
    if (http_response_info_->headers &&
        http_response_info_->headers->response_code() ==
            HTTP_SWITCHING_PROTOCOLS) {
      http_response_info_->headers->ReplaceStatusLine(
          kConnectionErrorStatusLine);
      result_ = HandshakeResult::FAILED_SWITCHING_PROTOCOLS;
      return rv;
    }
    result_ = HandshakeResult::FAILED;
    return rv;
  }
}

int WebSocketBasicHandshakeStream::ValidateUpgradeResponse(
    const HttpResponseHeaders* headers) {
  extension_params_ = std::make_unique<WebSocketExtensionParams>();
  std::string failure_message;
  if (!ValidateUpgrade(headers, &failure_message)) {
    result_ = HandshakeResult::FAILED_UPGRADE;
  } else if (!ValidateSecWebSocketAccept(headers, handshake_challenge_response_,
                                         &failure_message)) {
    result_ = HandshakeResult::FAILED_ACCEPT;
  } else if (!ValidateConnection(headers, &failure_message)) {
    result_ = HandshakeResult::FAILED_CONNECTION;
  } else if (!ValidateSubProtocol(headers, requested_sub_protocols_,
                                  &sub_protocol_, &failure_message)) {
    result_ = HandshakeResult::FAILED_SUBPROTO;
  } else if (!ValidateExtensions(headers, &extensions_, &failure_message,
                                 extension_params_.get())) {
    result_ = HandshakeResult::FAILED_EXTENSIONS;
  } else {
    result_ = HandshakeResult::CONNECTED;
    return OK;
  }
  OnFailure("Error during WebSocket handshake: " + failure_message);
  return ERR_INVALID_RESPONSE;
}

void WebSocketBasicHandshakeStream::OnFailure(const std::string& message) {
  // Avoid connection reuse if auth did not happen.
  state_.connection()->socket()->Disconnect();
  stream_request_->OnFailure(message);
}

}  // namespace net

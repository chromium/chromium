// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_transaction.h"

#include <set>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/upload_data_stream.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/filter/filter_source_stream.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_proxy_client_socket_pool.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_status_code.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/url_security_manager.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/socks_client_socket_pool.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_private_key.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// Max number of |retry_attempts| (excluding the initial request) after which
// we give up and show an error page.
const size_t kMaxRetryAttempts = 2;

// Max number of calls to RestartWith* allowed for a single connection. A single
// HttpNetworkTransaction should not signal very many restartable errors, but it
// may occur due to a bug (e.g. https://crbug.com/823387 or
// https://crbug.com/488043) or simply if the server or proxy requests
// authentication repeatedly. Although these calls are often associated with a
// user prompt, in other scenarios (remembered preferences, extensions,
// multi-leg authentication), they may be triggered automatically. To avoid
// looping forever, bound the number of restarts.
const size_t kMaxRestarts = 32;

}  // namespace

namespace net {

const int HttpNetworkTransaction::kDrainBodyBufferSize;

HttpNetworkTransaction::HttpNetworkTransaction(RequestPriority priority,
                                               HttpNetworkSession* session)
    : pending_auth_target_(HttpAuth::AUTH_NONE),
      io_callback_(base::BindRepeating(&HttpNetworkTransaction::OnIOComplete,
                                       base::Unretained(this))),
      session_(session),
      request_(NULL),
      priority_(priority),
      headers_valid_(false),
      can_send_early_data_(false),
      server_ssl_client_cert_was_cached_(false),
      request_headers_(),
      read_buf_len_(0),
      total_received_bytes_(0),
      total_sent_bytes_(0),
      next_state_(STATE_NONE),
      establishing_tunnel_(false),
      enable_ip_based_pooling_(true),
      enable_alternative_services_(true),
      websocket_handshake_stream_base_create_helper_(NULL),
      net_error_details_(),
      retry_attempts_(0),
      num_restarts_(0),
      ssl_version_interference_error_(OK) {}

HttpNetworkTransaction::~HttpNetworkTransaction() {
  if (stream_.get()) {
    // TODO(mbelshe): The stream_ should be able to compute whether or not the
    //                stream should be kept alive.  No reason to compute here
    //                and pass it in.
    if (!stream_->CanReuseConnection() || next_state_ != STATE_NONE) {
      stream_->Close(true /* not reusable */);
    } else if (stream_->IsResponseBodyComplete()) {
      // If the response body is complete, we can just reuse the socket.
      stream_->Close(false /* reusable */);
    } else {
      // Otherwise, we try to drain the response body.
      HttpStream* stream = stream_.release();
      stream->Drain(session_);
    }
  }
  if (request_ && request_->upload_data_stream)
    request_->upload_data_stream->Reset();  // Invalidate pending callbacks.
}

int HttpNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionOnceCallback callback,
                                  const NetLogWithSource& net_log) {
  DCHECK(request_info->traffic_annotation.is_valid());
  net_log_ = net_log;
  request_ = request_info;
  url_ = request_->url;

  // Now that we have an HttpRequestInfo object, update server_ssl_config_.
  session_->GetSSLConfig(*request_, &server_ssl_config_, &proxy_ssl_config_);

  if (request_->load_flags & LOAD_DISABLE_CERT_NETWORK_FETCHES) {
    server_ssl_config_.disable_cert_verification_network_fetches = true;
    proxy_ssl_config_.disable_cert_verification_network_fetches = true;
  }

  if (HttpUtil::IsMethodSafe(request_info->method)) {
    can_send_early_data_ = true;
  }

  if (request_->load_flags & LOAD_PREFETCH)
    response_.unused_since_prefetch = true;

  next_state_ = STATE_NOTIFY_BEFORE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

int HttpNetworkTransaction::RestartIgnoringLastError(
    CompletionOnceCallback callback) {
  DCHECK(!stream_.get());
  DCHECK(!stream_request_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  if (!CheckMaxRestarts())
    return ERR_TOO_MANY_RETRIES;

  next_state_ = STATE_CREATE_STREAM;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

int HttpNetworkTransaction::RestartWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key,
    CompletionOnceCallback callback) {
  // In HandleCertificateRequest(), we always tear down existing stream
  // requests to force a new connection.  So we shouldn't have one here.
  DCHECK(!stream_request_.get());
  DCHECK(!stream_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  if (!CheckMaxRestarts())
    return ERR_TOO_MANY_RETRIES;

  SSLConfig* ssl_config = response_.cert_request_info->is_proxy ?
      &proxy_ssl_config_ : &server_ssl_config_;
  ssl_config->send_client_cert = true;
  ssl_config->client_cert = client_cert;
  ssl_config->client_private_key = client_private_key;
  session_->ssl_client_auth_cache()->Add(
      response_.cert_request_info->host_and_port, std::move(client_cert),
      std::move(client_private_key));
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  next_state_ = STATE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

int HttpNetworkTransaction::RestartWithAuth(const AuthCredentials& credentials,
                                            CompletionOnceCallback callback) {
  if (!CheckMaxRestarts())
    return ERR_TOO_MANY_RETRIES;

  HttpAuth::Target target = pending_auth_target_;
  if (target == HttpAuth::AUTH_NONE) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  pending_auth_target_ = HttpAuth::AUTH_NONE;

  auth_controllers_[target]->ResetAuth(credentials);

  DCHECK(callback_.is_null());

  int rv = OK;
  if (target == HttpAuth::AUTH_PROXY && establishing_tunnel_) {
    // In this case, we've gathered credentials for use with proxy
    // authentication of a tunnel.
    DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
    DCHECK(stream_request_ != NULL);
    auth_controllers_[target] = NULL;
    ResetStateForRestart();
    rv = stream_request_->RestartTunnelWithProxyAuth();
  } else {
    // In this case, we've gathered credentials for the server or the proxy
    // but it is not during the tunneling phase.
    DCHECK(stream_request_ == NULL);
    PrepareForAuthRestart(target);
    rv = DoLoop(OK);
  }

  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

void HttpNetworkTransaction::PrepareForAuthRestart(HttpAuth::Target target) {
  DCHECK(HaveAuth(target));
  DCHECK(!stream_request_.get());

  // Authorization schemes incompatible with HTTP/2 are unsupported for proxies.
  if (target == HttpAuth::AUTH_SERVER &&
      auth_controllers_[target]->NeedsHTTP11()) {
    session_->http_server_properties()->SetHTTP11Required(
        HostPortPair::FromURL(request_->url));
  }

  bool keep_alive = false;
  // Even if the server says the connection is keep-alive, we have to be
  // able to find the end of each response in order to reuse the connection.
  if (stream_->CanReuseConnection()) {
    // If the response body hasn't been completely read, we need to drain
    // it first.
    if (!stream_->IsResponseBodyComplete()) {
      next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
      read_buf_ = base::MakeRefCounted<IOBuffer>(
          kDrainBodyBufferSize);  // A bit bucket.
      read_buf_len_ = kDrainBodyBufferSize;
      return;
    }
    keep_alive = true;
  }

  // We don't need to drain the response body, so we act as if we had drained
  // the response body.
  DidDrainBodyForAuthRestart(keep_alive);
}

void HttpNetworkTransaction::DidDrainBodyForAuthRestart(bool keep_alive) {
  DCHECK(!stream_request_.get());

  if (stream_.get()) {
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
    HttpStream* new_stream = NULL;
    if (keep_alive && stream_->CanReuseConnection()) {
      // We should call connection_->set_idle_time(), but this doesn't occur
      // often enough to be worth the trouble.
      stream_->SetConnectionReused();
      new_stream = stream_->RenewStreamForAuth();
    }

    if (!new_stream) {
      // Close the stream and mark it as not_reusable.  Even in the
      // keep_alive case, we've determined that the stream_ is not
      // reusable if new_stream is NULL.
      stream_->Close(true);
      next_state_ = STATE_CREATE_STREAM;
    } else {
      // Renewed streams shouldn't carry over sent or received bytes.
      DCHECK_EQ(0, new_stream->GetTotalReceivedBytes());
      DCHECK_EQ(0, new_stream->GetTotalSentBytes());
      next_state_ = STATE_INIT_STREAM;
    }
    stream_.reset(new_stream);
  }

  // Reset the other member variables.
  ResetStateForAuthRestart();
}

bool HttpNetworkTransaction::IsReadyToRestartForAuth() {
  return pending_auth_target_ != HttpAuth::AUTH_NONE &&
      HaveAuth(pending_auth_target_);
}

int HttpNetworkTransaction::Read(IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  DCHECK(buf);
  DCHECK_LT(0, buf_len);

  scoped_refptr<HttpResponseHeaders> headers(GetResponseHeaders());
  if (headers_valid_ && headers.get() && stream_request_.get()) {
    // We're trying to read the body of the response but we're still trying
    // to establish an SSL tunnel through an HTTP proxy.  We can't read these
    // bytes when establishing a tunnel because they might be controlled by
    // an active network attacker.  We don't worry about this for HTTP
    // because an active network attacker can already control HTTP sessions.
    // We reach this case when the user cancels a 407 proxy auth prompt.  We
    // also don't worry about this for an HTTPS Proxy, because the
    // communication with the proxy is secure.
    // See http://crbug.com/8473.
    DCHECK(proxy_info_.is_http() || proxy_info_.is_https() ||
           proxy_info_.is_quic());
    DCHECK_EQ(headers->response_code(), HTTP_PROXY_AUTHENTICATION_REQUIRED);
    LOG(WARNING) << "Blocked proxy response with status "
                 << headers->response_code() << " to CONNECT request for "
                 << GetHostAndPort(url_) << ".";
    return ERR_TUNNEL_CONNECTION_FAILED;
  }

  // Are we using SPDY or HTTP?
  next_state_ = STATE_READ_BODY;

  read_buf_ = buf;
  read_buf_len_ = buf_len;

  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  return rv;
}

void HttpNetworkTransaction::StopCaching() {}

bool HttpNetworkTransaction::GetFullRequestHeaders(
    HttpRequestHeaders* headers) const {
  // TODO(juliatuttle): Make sure we've populated request_headers_.
  *headers = request_headers_;
  return true;
}

int64_t HttpNetworkTransaction::GetTotalReceivedBytes() const {
  int64_t total_received_bytes = total_received_bytes_;
  if (stream_)
    total_received_bytes += stream_->GetTotalReceivedBytes();
  return total_received_bytes;
}

int64_t HttpNetworkTransaction::GetTotalSentBytes() const {
  int64_t total_sent_bytes = total_sent_bytes_;
  if (stream_)
    total_sent_bytes += stream_->GetTotalSentBytes();
  return total_sent_bytes;
}

void HttpNetworkTransaction::DoneReading() {}

const HttpResponseInfo* HttpNetworkTransaction::GetResponseInfo() const {
  return &response_;
}

LoadState HttpNetworkTransaction::GetLoadState() const {
  // TODO(wtc): Define a new LoadState value for the
  // STATE_INIT_CONNECTION_COMPLETE state, which delays the HTTP request.
  switch (next_state_) {
    case STATE_CREATE_STREAM:
      return LOAD_STATE_WAITING_FOR_DELEGATE;
    case STATE_CREATE_STREAM_COMPLETE:
      return stream_request_->GetLoadState();
    case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
    case STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE:
    case STATE_SEND_REQUEST_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_READ_HEADERS_COMPLETE:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_READ_BODY_COMPLETE:
      return LOAD_STATE_READING_RESPONSE;
    default:
      return LOAD_STATE_IDLE;
  }
}

void HttpNetworkTransaction::SetQuicServerInfo(
    QuicServerInfo* quic_server_info) {}

bool HttpNetworkTransaction::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  if (!stream_ || !stream_->GetLoadTimingInfo(load_timing_info))
    return false;

  load_timing_info->proxy_resolve_start =
      proxy_info_.proxy_resolve_start_time();
  load_timing_info->proxy_resolve_end = proxy_info_.proxy_resolve_end_time();
  load_timing_info->send_start = send_start_time_;
  load_timing_info->send_end = send_end_time_;
  return true;
}

bool HttpNetworkTransaction::GetRemoteEndpoint(IPEndPoint* endpoint) const {
  if (remote_endpoint_.address().empty())
    return false;

  *endpoint = remote_endpoint_;
  return true;
}

void HttpNetworkTransaction::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  *details = net_error_details_;
  if (stream_)
    stream_->PopulateNetErrorDetails(details);
}

void HttpNetworkTransaction::SetPriority(RequestPriority priority) {
  priority_ = priority;

  if (stream_request_)
    stream_request_->SetPriority(priority);
  if (stream_)
    stream_->SetPriority(priority);

  // The above call may have resulted in deleting |*this|.
}

void HttpNetworkTransaction::SetWebSocketHandshakeStreamCreateHelper(
    WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  websocket_handshake_stream_base_create_helper_ = create_helper;
}

void HttpNetworkTransaction::SetBeforeNetworkStartCallback(
    const BeforeNetworkStartCallback& callback) {
  before_network_start_callback_ = callback;
}

void HttpNetworkTransaction::SetBeforeHeadersSentCallback(
    const BeforeHeadersSentCallback& callback) {
  before_headers_sent_callback_ = callback;
}

void HttpNetworkTransaction::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  DCHECK(!stream_);
  request_headers_callback_ = std::move(callback);
}

void HttpNetworkTransaction::SetResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!stream_);
  response_headers_callback_ = std::move(callback);
}

int HttpNetworkTransaction::ResumeNetworkStart() {
  DCHECK_EQ(next_state_, STATE_CREATE_STREAM);
  return DoLoop(OK);
}

void HttpNetworkTransaction::OnStreamReady(const SSLConfig& used_ssl_config,
                                           const ProxyInfo& used_proxy_info,
                                           std::unique_ptr<HttpStream> stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK(stream_request_.get());

  if (stream_) {
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
  }
  stream_ = std::move(stream);
  stream_->SetRequestHeadersCallback(request_headers_callback_);
  server_ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;
  response_.was_alpn_negotiated = stream_request_->was_alpn_negotiated();
  response_.alpn_negotiated_protocol =
      NextProtoToString(stream_request_->negotiated_protocol());
  response_.was_fetched_via_spdy = stream_request_->using_spdy();
  response_.was_fetched_via_proxy = !proxy_info_.is_direct();
  if (response_.was_fetched_via_proxy && !proxy_info_.is_empty())
    response_.proxy_server = proxy_info_.proxy_server();
  else if (!response_.was_fetched_via_proxy && proxy_info_.is_direct())
    response_.proxy_server = ProxyServer::Direct();
  else
    response_.proxy_server = ProxyServer();
  OnIOComplete(OK);
}

void HttpNetworkTransaction::OnBidirectionalStreamImplReady(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<BidirectionalStreamImpl> stream) {
  NOTREACHED();
}

void HttpNetworkTransaction::OnWebSocketHandshakeStreamReady(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<WebSocketHandshakeStreamBase> stream) {
  OnStreamReady(used_ssl_config, used_proxy_info, std::move(stream));
}

void HttpNetworkTransaction::OnStreamFailed(
    int result,
    const NetErrorDetails& net_error_details,
    const SSLConfig& used_ssl_config) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());
  server_ssl_config_ = used_ssl_config;
  net_error_details_ = net_error_details;

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnCertificateError(
    int result,
    const SSLConfig& used_ssl_config,
    const SSLInfo& ssl_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());

  response_.ssl_info = ssl_info;
  server_ssl_config_ = used_ssl_config;

  // TODO(mbelshe):  For now, we're going to pass the error through, and that
  // will close the stream_request in all cases.  This means that we're always
  // going to restart an entire STATE_CREATE_STREAM, even if the connection is
  // good and the user chooses to ignore the error.  This is not ideal, but not
  // the end of the world either.

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  DCHECK(stream_request_.get());
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  establishing_tunnel_ = true;
  response_.headers = proxy_response.headers;
  response_.auth_challenge = proxy_response.auth_challenge;

  if (response_.headers.get() && !ContentEncodingsValid()) {
    DoCallback(ERR_CONTENT_DECODING_FAILED);
    return;
  }

  headers_valid_ = true;
  server_ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;

  auth_controllers_[HttpAuth::AUTH_PROXY] = auth_controller;
  pending_auth_target_ = HttpAuth::AUTH_PROXY;

  DoCallback(OK);
}

void HttpNetworkTransaction::OnNeedsClientAuth(
    const SSLConfig& used_ssl_config,
    SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  server_ssl_config_ = used_ssl_config;
  response_.cert_request_info = cert_info;
  OnIOComplete(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void HttpNetworkTransaction::OnHttpsProxyTunnelResponse(
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<HttpStream> stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  CopyConnectionAttemptsFromStreamRequest();

  headers_valid_ = true;
  response_ = response_info;
  server_ssl_config_ = used_ssl_config;
  proxy_info_ = used_proxy_info;
  if (stream_) {
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
  }
  stream_ = std::move(stream);
  stream_->SetRequestHeadersCallback(request_headers_callback_);
  stream_request_.reset();  // we're done with the stream request
  OnIOComplete(ERR_HTTPS_PROXY_TUNNEL_RESPONSE);
}

void HttpNetworkTransaction::OnQuicBroken() {
  net_error_details_.quic_broken = true;
}

void HttpNetworkTransaction::GetConnectionAttempts(
    ConnectionAttempts* out) const {
  *out = connection_attempts_;
}

bool HttpNetworkTransaction::IsSecureRequest() const {
  return request_->url.SchemeIsCryptographic();
}

bool HttpNetworkTransaction::UsingHttpProxyWithoutTunnel() const {
  return (proxy_info_.is_http() || proxy_info_.is_https() ||
          proxy_info_.is_quic()) &&
         !(request_->url.SchemeIs("https") || request_->url.SchemeIsWSOrWSS());
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!callback_.is_null());

  // Since Run may result in Read being called, clear user_callback_ up front.
  base::ResetAndReturn(&callback_).Run(rv);
}

void HttpNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int HttpNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_NOTIFY_BEFORE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoNotifyBeforeCreateStream();
        break;
      case STATE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case STATE_CREATE_STREAM_COMPLETE:
        // TODO(zhongyi): remove liveness checks when crbug.com/652868 is
        // solved.
        net_log_.CrashIfInvalid();
        rv = DoCreateStreamComplete(rv);
        net_log_.CrashIfInvalid();
        break;
      case STATE_INIT_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoInitStream();
        break;
      case STATE_INIT_STREAM_COMPLETE:
        rv = DoInitStreamComplete(rv);
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateProxyAuthToken();
        break;
      case STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateProxyAuthTokenComplete(rv);
        break;
      case STATE_GENERATE_SERVER_AUTH_TOKEN:
        DCHECK_EQ(OK, rv);
        rv = DoGenerateServerAuthToken();
        break;
      case STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE:
        rv = DoGenerateServerAuthTokenComplete(rv);
        break;
      case STATE_INIT_REQUEST_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoInitRequestBody();
        break;
      case STATE_INIT_REQUEST_BODY_COMPLETE:
        rv = DoInitRequestBodyComplete(rv);
        break;
      case STATE_BUILD_REQUEST:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST);
        rv = DoBuildRequest();
        break;
      case STATE_BUILD_REQUEST_COMPLETE:
        rv = DoBuildRequestComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST, rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::HTTP_TRANSACTION_READ_HEADERS);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        rv = DoReadHeadersComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_READ_HEADERS, rv);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLogEventType::HTTP_TRANSACTION_READ_BODY);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        rv = DoReadBodyComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_READ_BODY, rv);
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(
            NetLogEventType::HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART);
        rv = DoDrainBodyForAuthRestart();
        break;
      case STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE:
        rv = DoDrainBodyForAuthRestartComplete(rv);
        net_log_.EndEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART, rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpNetworkTransaction::DoNotifyBeforeCreateStream() {
  next_state_ = STATE_CREATE_STREAM;
  bool defer = false;
  if (!before_network_start_callback_.is_null())
    before_network_start_callback_.Run(&defer);
  if (!defer)
    return OK;
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStream() {
  response_.network_accessed = true;

  next_state_ = STATE_CREATE_STREAM_COMPLETE;
  // IP based pooling is only enabled on a retry after 421 Misdirected Request
  // is received. Alternative Services are also disabled in this case (though
  // they can also be disabled when retrying after a QUIC error).
  if (!enable_ip_based_pooling_)
    DCHECK(!enable_alternative_services_);
  if (ForWebSocketHandshake()) {
    stream_request_ =
        session_->http_stream_factory()->RequestWebSocketHandshakeStream(
            *request_, priority_, server_ssl_config_, proxy_ssl_config_, this,
            websocket_handshake_stream_base_create_helper_,
            enable_ip_based_pooling_, enable_alternative_services_, net_log_);
  } else {
    stream_request_ = session_->http_stream_factory()->RequestStream(
        *request_, priority_, server_ssl_config_, proxy_ssl_config_, this,
        enable_ip_based_pooling_, enable_alternative_services_, net_log_);
  }
  DCHECK(stream_request_.get());
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStreamComplete(int result) {
  // Version interference probes should not result in success.
  DCHECK(!server_ssl_config_.version_interference_probe || result != OK);

  // If |result| is ERR_HTTPS_PROXY_TUNNEL_RESPONSE, then
  // DoCreateStreamComplete is being called from OnHttpsProxyTunnelResponse,
  // which resets the stream request first. Therefore, we have to grab the
  // connection attempts in *that* function instead of here in that case.
  if (result != ERR_HTTPS_PROXY_TUNNEL_RESPONSE)
    CopyConnectionAttemptsFromStreamRequest();

  if (result == OK) {
    next_state_ = STATE_INIT_STREAM;
    DCHECK(stream_.get());
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    result = HandleCertificateRequest(result);
  } else if (result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    // Return OK and let the caller read the proxy's error page
    next_state_ = STATE_NONE;
    return OK;
  } else if (result == ERR_HTTP_1_1_REQUIRED ||
             result == ERR_PROXY_HTTP_1_1_REQUIRED) {
    return HandleHttp11Required(result);
  }

  // Perform a TLS 1.3 version interference probe on various connection
  // errors. The retry will never produce a successful connection but may map
  // errors to ERR_SSL_VERSION_INTERFERENCE, which signals a probable
  // version-interfering middlebox.
  if (IsSecureRequest() && !HasExceededMaxRetries() &&
      server_ssl_config_.version_max == SSL_PROTOCOL_VERSION_TLS1_3 &&
      !server_ssl_config_.version_interference_probe) {
    if (result == ERR_CONNECTION_CLOSED || result == ERR_SSL_PROTOCOL_ERROR ||
        result == ERR_SSL_VERSION_OR_CIPHER_MISMATCH ||
        result == ERR_CONNECTION_RESET ||
        result == ERR_SSL_BAD_RECORD_MAC_ALERT) {
      // Report the error code for each time a version interference probe is
      // triggered.
      base::UmaHistogramSparse("Net.SSLVersionInterferenceProbeTrigger",
                               std::abs(result));
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::SSL_VERSION_INTERFERENCE_PROBE, result);

      retry_attempts_++;
      server_ssl_config_.version_interference_probe = true;
      server_ssl_config_.version_max = SSL_PROTOCOL_VERSION_TLS1_2;
      ssl_version_interference_error_ = result;
      ResetConnectionAndRequestForResend();
      return OK;
    }
  }

  if (result == ERR_SSL_VERSION_INTERFERENCE) {
    // Record the error code version interference was detected at.
    DCHECK(server_ssl_config_.version_interference_probe);
    DCHECK_NE(OK, ssl_version_interference_error_);
    base::UmaHistogramSparse("Net.SSLVersionInterferenceError",
                             std::abs(ssl_version_interference_error_));
  }

  // Handle possible client certificate errors that may have occurred if the
  // stream used SSL for one or more of the layers.
  result = HandleSSLClientAuthError(result);

  // At this point we are done with the stream_request_.
  stream_request_.reset();
  return result;
}

int HttpNetworkTransaction::DoInitStream() {
  DCHECK(stream_.get());
  next_state_ = STATE_INIT_STREAM_COMPLETE;

  stream_->GetRemoteEndpoint(&remote_endpoint_);

  return stream_->InitializeStream(request_, can_send_early_data_, priority_,
                                   net_log_, io_callback_);
}

int HttpNetworkTransaction::DoInitStreamComplete(int result) {
  if (result == OK) {
    next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  } else {
    if (result < 0)
      result = HandleIOError(result);

    // The stream initialization failed, so this stream will never be useful.
    if (stream_) {
      total_received_bytes_ += stream_->GetTotalReceivedBytes();
      total_sent_bytes_ += stream_->GetTotalSentBytes();
    }
    CacheNetErrorDetailsAndResetStream();
  }

  return result;
}

int HttpNetworkTransaction::DoGenerateProxyAuthToken() {
  next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE;
  if (!ShouldApplyProxyAuth())
    return OK;
  HttpAuth::Target target = HttpAuth::AUTH_PROXY;
  if (!auth_controllers_[target].get())
    auth_controllers_[target] =
        new HttpAuthController(target,
                               AuthURL(target),
                               session_->http_auth_cache(),
                               session_->http_auth_handler_factory());
  return auth_controllers_[target]->MaybeGenerateAuthToken(request_,
                                                           io_callback_,
                                                           net_log_);
}

int HttpNetworkTransaction::DoGenerateProxyAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv == OK)
    next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN;
  return rv;
}

int HttpNetworkTransaction::DoGenerateServerAuthToken() {
  next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE;
  HttpAuth::Target target = HttpAuth::AUTH_SERVER;
  if (!auth_controllers_[target].get()) {
    auth_controllers_[target] =
        new HttpAuthController(target,
                               AuthURL(target),
                               session_->http_auth_cache(),
                               session_->http_auth_handler_factory());
    if (request_->load_flags & LOAD_DO_NOT_USE_EMBEDDED_IDENTITY)
      auth_controllers_[target]->DisableEmbeddedIdentity();
  }
  if (!ShouldApplyServerAuth())
    return OK;
  return auth_controllers_[target]->MaybeGenerateAuthToken(request_,
                                                           io_callback_,
                                                           net_log_);
}

int HttpNetworkTransaction::DoGenerateServerAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv == OK)
    next_state_ = STATE_INIT_REQUEST_BODY;
  return rv;
}

int HttpNetworkTransaction::BuildRequestHeaders(
    bool using_http_proxy_without_tunnel) {
  request_headers_.SetHeader(HttpRequestHeaders::kHost,
                             GetHostAndOptionalPort(request_->url));

  // For compat with HTTP/1.0 servers and proxies:
  if (using_http_proxy_without_tunnel) {
    request_headers_.SetHeader(HttpRequestHeaders::kProxyConnection,
                               "keep-alive");
  } else {
    request_headers_.SetHeader(HttpRequestHeaders::kConnection, "keep-alive");
  }

  // Add a content length header?
  if (request_->upload_data_stream) {
    if (request_->upload_data_stream->is_chunked()) {
      request_headers_.SetHeader(
          HttpRequestHeaders::kTransferEncoding, "chunked");
    } else {
      request_headers_.SetHeader(
          HttpRequestHeaders::kContentLength,
          base::NumberToString(request_->upload_data_stream->size()));
    }
  } else if (request_->method == "POST" || request_->method == "PUT") {
    // An empty POST/PUT request still needs a content length.  As for HEAD,
    // IE and Safari also add a content length header.  Presumably it is to
    // support sending a HEAD request to an URL that only expects to be sent a
    // POST or some other method that normally would have a message body.
    // Firefox (40.0) does not send the header, and RFC 7230 & 7231
    // specify that it should not be sent due to undefined behavior.
    request_headers_.SetHeader(HttpRequestHeaders::kContentLength, "0");
  }

  // Honor load flags that impact proxy caches.
  if (request_->load_flags & LOAD_BYPASS_CACHE) {
    request_headers_.SetHeader(HttpRequestHeaders::kPragma, "no-cache");
    request_headers_.SetHeader(HttpRequestHeaders::kCacheControl, "no-cache");
  } else if (request_->load_flags & LOAD_VALIDATE_CACHE) {
    request_headers_.SetHeader(HttpRequestHeaders::kCacheControl, "max-age=0");
  }

  if (ShouldApplyProxyAuth() && HaveAuth(HttpAuth::AUTH_PROXY))
    auth_controllers_[HttpAuth::AUTH_PROXY]->AddAuthorizationHeader(
        &request_headers_);
  if (ShouldApplyServerAuth() && HaveAuth(HttpAuth::AUTH_SERVER))
    auth_controllers_[HttpAuth::AUTH_SERVER]->AddAuthorizationHeader(
        &request_headers_);

  request_headers_.MergeFrom(request_->extra_headers);

  if (!before_headers_sent_callback_.is_null())
    before_headers_sent_callback_.Run(proxy_info_, &request_headers_);

  response_.did_use_http_auth =
      request_headers_.HasHeader(HttpRequestHeaders::kAuthorization) ||
      request_headers_.HasHeader(HttpRequestHeaders::kProxyAuthorization);
  return OK;
}

int HttpNetworkTransaction::DoInitRequestBody() {
  next_state_ = STATE_INIT_REQUEST_BODY_COMPLETE;
  int rv = OK;
  if (request_->upload_data_stream)
    rv = request_->upload_data_stream->Init(
        base::BindOnce(&HttpNetworkTransaction::OnIOComplete,
                       base::Unretained(this)),
        net_log_);
  return rv;
}

int HttpNetworkTransaction::DoInitRequestBodyComplete(int result) {
  if (result == OK)
    next_state_ = STATE_BUILD_REQUEST;
  return result;
}

int HttpNetworkTransaction::DoBuildRequest() {
  next_state_ = STATE_BUILD_REQUEST_COMPLETE;
  headers_valid_ = false;

  // This is constructed lazily (instead of within our Start method), so that
  // we have proxy info available.
  if (request_headers_.IsEmpty()) {
    bool using_http_proxy_without_tunnel = UsingHttpProxyWithoutTunnel();
    return BuildRequestHeaders(using_http_proxy_without_tunnel);
  }

  return OK;
}

int HttpNetworkTransaction::DoBuildRequestComplete(int result) {
  if (result == OK)
    next_state_ = STATE_SEND_REQUEST;
  return result;
}

int HttpNetworkTransaction::DoSendRequest() {
  send_start_time_ = base::TimeTicks::Now();
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  return stream_->SendRequest(request_headers_, &response_, io_callback_);
}

int HttpNetworkTransaction::DoSendRequestComplete(int result) {
  send_end_time_ = base::TimeTicks::Now();

  if (result == ERR_HTTP_1_1_REQUIRED ||
      result == ERR_PROXY_HTTP_1_1_REQUIRED) {
    return HandleHttp11Required(result);
  }

  if (result < 0)
    return HandleIOError(result);
  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int HttpNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return stream_->ReadResponseHeaders(io_callback_);
}

int HttpNetworkTransaction::DoReadHeadersComplete(int result) {
  // We can get a certificate error or ERR_SSL_CLIENT_AUTH_CERT_NEEDED here
  // due to SSL renegotiation.
  if (IsCertificateError(result)) {
    // We don't handle a certificate error during SSL renegotiation, so we
    // have to return an error that's not in the certificate error range
    // (-2xx).
    LOG(ERROR) << "Got a server certificate with error " << result
               << " during SSL renegotiation";
    result = ERR_CERT_ERROR_IN_SSL_RENEGOTIATION;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    // TODO(wtc): Need a test case for this code path!
    DCHECK(stream_.get());
    DCHECK(IsSecureRequest());
    response_.cert_request_info = new SSLCertRequestInfo;
    stream_->GetSSLCertRequestInfo(response_.cert_request_info.get());
    result = HandleCertificateRequest(result);
    if (result == OK)
      return result;
  }

  if (result == ERR_HTTP_1_1_REQUIRED ||
      result == ERR_PROXY_HTTP_1_1_REQUIRED) {
    return HandleHttp11Required(result);
  }

  // ERR_CONNECTION_CLOSED is treated differently at this point; if partial
  // response headers were received, we do the best we can to make sense of it
  // and send it back up the stack.
  //
  // TODO(davidben): Consider moving this to HttpBasicStream, It's a little
  // bizarre for SPDY. Assuming this logic is useful at all.
  // TODO(davidben): Bubble the error code up so we do not cache?
  if (result == ERR_CONNECTION_CLOSED && response_.headers.get())
    result = OK;

  if (result < 0)
    return HandleIOError(result);

  DCHECK(response_.headers.get());

  if (response_.headers.get() && !ContentEncodingsValid())
    return ERR_CONTENT_DECODING_FAILED;

  // On a 408 response from the server ("Request Timeout") on a stale socket,
  // retry the request.
  // Headers can be NULL because of http://crbug.com/384554.
  if (response_.headers.get() &&
      response_.headers->response_code() == HTTP_REQUEST_TIMEOUT &&
      stream_->IsConnectionReused()) {
    net_log_.AddEventWithNetErrorCode(
        NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR,
        response_.headers->response_code());
    // This will close the socket - it would be weird to try and reuse it, even
    // if the server doesn't actually close it.
    ResetConnectionAndRequestForResend();
    return OK;
  }

  // Like Net.HttpResponseCode, but only for MAIN_FRAME loads.
  if (request_->load_flags & LOAD_MAIN_FRAME_DEPRECATED) {
    const int response_code = response_.headers->response_code();
    UMA_HISTOGRAM_ENUMERATION(
        "Net.HttpResponseCode_Nxx_MainFrame", response_code/100, 10);
  }

  net_log_.AddEvent(
      NetLogEventType::HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
      base::Bind(&HttpResponseHeaders::NetLogCallback, response_.headers));
  if (response_headers_callback_)
    response_headers_callback_.Run(response_.headers);

  if (response_.headers->GetHttpVersion() < HttpVersion(1, 0)) {
    // HTTP/0.9 doesn't support the PUT method, so lack of response headers
    // indicates a buggy server.  See:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=193921
    if (request_->method == "PUT")
      return ERR_METHOD_NOT_SUPPORTED;
  }

  if (can_send_early_data_ && response_.headers.get() &&
      response_.headers->response_code() == HTTP_TOO_EARLY) {
    return HandleIOError(ERR_EARLY_DATA_REJECTED);
  }

  // Check for an intermediate 100 Continue response.  An origin server is
  // allowed to send this response even if we didn't ask for it, so we just
  // need to skip over it.
  // We treat any other 1xx in this same way (although in practice getting
  // a 1xx that isn't a 100 is rare).
  // Unless this is a WebSocket request, in which case we pass it on up.
  if (response_.headers->response_code() / 100 == 1 &&
      !ForWebSocketHandshake()) {
    response_.headers = new HttpResponseHeaders(std::string());
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  if (response_.headers->response_code() == 421 &&
      (enable_ip_based_pooling_ || enable_alternative_services_)) {
    // Retry the request with both IP based pooling and Alternative Services
    // disabled.
    enable_ip_based_pooling_ = false;
    enable_alternative_services_ = false;
    net_log_.AddEvent(
        NetLogEventType::HTTP_TRANSACTION_RESTART_MISDIRECTED_REQUEST);
    ResetConnectionAndRequestForResend();
    return OK;
  }

  if (IsSecureRequest()) {
    stream_->GetSSLInfo(&response_.ssl_info);
    if (response_.ssl_info.is_valid() &&
        !IsCertStatusError(response_.ssl_info.cert_status)) {
      session_->http_stream_factory()->ProcessAlternativeServices(
          session_, response_.headers.get(),
          url::SchemeHostPort(request_->url));
    }
  }

  int rv = HandleAuthChallenge();
  if (rv != OK)
    return rv;

  headers_valid_ = true;

  // We have reached the end of Start state machine, set the RequestInfo to
  // null.
  // RequestInfo is a member of the HttpTransaction's consumer and is useful
  // only until the final response headers are received. Clearing it will ensure
  // that HttpRequestInfo is only used up until final response headers are
  // received. Clearing is allowed so that the transaction can be disassociated
  // from its creating consumer in cases where it is shared for writing to the
  // cache. It is also safe to set it to null at this point since
  // upload_data_stream is also not used in the Read state machine.
  if (pending_auth_target_ == HttpAuth::AUTH_NONE)
    request_ = nullptr;

  return OK;
}

int HttpNetworkTransaction::DoReadBody() {
  DCHECK(read_buf_.get());
  DCHECK_GT(read_buf_len_, 0);
  DCHECK(stream_ != NULL);

  next_state_ = STATE_READ_BODY_COMPLETE;
  return stream_->ReadResponseBody(
      read_buf_.get(), read_buf_len_, io_callback_);
}

int HttpNetworkTransaction::DoReadBodyComplete(int result) {
  // We are done with the Read call.
  bool done = false;
  if (result <= 0) {
    DCHECK_NE(ERR_IO_PENDING, result);
    done = true;
  }

  // Clean up connection if we are done.
  if (done) {
    // Note: Just because IsResponseBodyComplete is true, we're not
    // necessarily "done".  We're only "done" when it is the last
    // read on this HttpNetworkTransaction, which will be signified
    // by a zero-length read.
    // TODO(mbelshe): The keep-alive property is really a property of
    //    the stream.  No need to compute it here just to pass back
    //    to the stream's Close function.
    bool keep_alive =
        stream_->IsResponseBodyComplete() && stream_->CanReuseConnection();

    stream_->Close(!keep_alive);
    // Note: we don't reset the stream here.  We've closed it, but we still
    // need it around so that callers can call methods such as
    // GetUploadProgress() and have them be meaningful.
    // TODO(mbelshe): This means we closed the stream here, and we close it
    // again in ~HttpNetworkTransaction.  Clean that up.

    // The next Read call will return 0 (EOF).

    // This transaction was successful. If it had been retried because of an
    // error with an alternative service, mark that alternative service broken.
    if (!enable_alternative_services_ &&
        retried_alternative_service_.protocol != kProtoUnknown) {
      session_->http_server_properties()->MarkAlternativeServiceBroken(
          retried_alternative_service_);
    }
  }

  // Clear these to avoid leaving around old state.
  read_buf_ = NULL;
  read_buf_len_ = 0;

  return result;
}

int HttpNetworkTransaction::DoDrainBodyForAuthRestart() {
  // This method differs from DoReadBody only in the next_state_.  So we just
  // call DoReadBody and override the next_state_.  Perhaps there is a more
  // elegant way for these two methods to share code.
  int rv = DoReadBody();
  DCHECK(next_state_ == STATE_READ_BODY_COMPLETE);
  next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE;
  return rv;
}

// TODO(wtc): This method and the DoReadBodyComplete method are almost
// the same.  Figure out a good way for these two methods to share code.
int HttpNetworkTransaction::DoDrainBodyForAuthRestartComplete(int result) {
  // keep_alive defaults to true because the very reason we're draining the
  // response body is to reuse the connection for auth restart.
  bool done = false, keep_alive = true;
  if (result < 0) {
    // Error or closed connection while reading the socket.
    done = true;
    keep_alive = false;
  } else if (stream_->IsResponseBodyComplete()) {
    done = true;
  }

  if (done) {
    DidDrainBodyForAuthRestart(keep_alive);
  } else {
    // Keep draining.
    next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
  }

  return OK;
}

int HttpNetworkTransaction::HandleCertificateRequest(int error) {
  // There are two paths through which the server can request a certificate
  // from us.  The first is during the initial handshake, the second is
  // during SSL renegotiation.
  //
  // In both cases, we want to close the connection before proceeding.
  // We do this for two reasons:
  //   First, we don't want to keep the connection to the server hung for a
  //   long time while the user selects a certificate.
  //   Second, even if we did keep the connection open, NSS has a bug where
  //   restarting the handshake for ClientAuth is currently broken.
  DCHECK_EQ(error, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);

  if (stream_.get()) {
    // Since we already have a stream, we're being called as part of SSL
    // renegotiation.
    DCHECK(!stream_request_.get());
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
    stream_->Close(true);
    CacheNetErrorDetailsAndResetStream();
  }

  // The server is asking for a client certificate during the initial
  // handshake.
  stream_request_.reset();

  // If the user selected one of the certificates in client_certs or declined
  // to provide one for this server before, use the past decision
  // automatically.
  scoped_refptr<X509Certificate> client_cert;
  scoped_refptr<SSLPrivateKey> client_private_key;
  bool found_cached_cert = session_->ssl_client_auth_cache()->Lookup(
      response_.cert_request_info->host_and_port, &client_cert,
      &client_private_key);
  if (!found_cached_cert)
    return error;

  // Check that the certificate selected is still a certificate the server
  // is likely to accept, based on the criteria supplied in the
  // CertificateRequest message.
  if (client_cert.get()) {
    const std::vector<std::string>& cert_authorities =
        response_.cert_request_info->cert_authorities;

    bool cert_still_valid = cert_authorities.empty() ||
        client_cert->IsIssuedByEncoded(cert_authorities);
    if (!cert_still_valid)
      return error;
  }

  if (!response_.cert_request_info->is_proxy) {
    server_ssl_client_cert_was_cached_ = true;
  }

  // TODO(davidben): Add a unit test which covers this path; we need to be
  // able to send a legitimate certificate and also bypass/clear the
  // SSL session cache.
  SSLConfig* ssl_config = response_.cert_request_info->is_proxy ?
      &proxy_ssl_config_ : &server_ssl_config_;
  ssl_config->send_client_cert = true;
  ssl_config->client_cert = client_cert;
  ssl_config->client_private_key = client_private_key;
  next_state_ = STATE_CREATE_STREAM;
  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  return OK;
}

int HttpNetworkTransaction::HandleHttp11Required(int error) {
  DCHECK(error == ERR_HTTP_1_1_REQUIRED ||
         error == ERR_PROXY_HTTP_1_1_REQUIRED);

  if (error == ERR_HTTP_1_1_REQUIRED) {
    HttpServerProperties::ForceHTTP11(&server_ssl_config_);
  } else {
    HttpServerProperties::ForceHTTP11(&proxy_ssl_config_);
  }
  ResetConnectionAndRequestForResend();
  return OK;
}

int HttpNetworkTransaction::HandleSSLClientAuthError(int error) {
  // TODO(davidben): This does handle client certificate errors from the
  // proxy. https://crbug.com/814911.
  if (server_ssl_config_.send_client_cert &&
      (error == ERR_SSL_PROTOCOL_ERROR || IsClientCertificateError(error))) {
    session_->ssl_client_auth_cache()->Remove(
        HostPortPair::FromURL(request_->url));

    // The private key handle may have gone stale due to, e.g., the user
    // unplugging their smartcard. Operating systems do not provide reliable
    // notifications for this, so if the signature failed and the private key
    // came from SSLClientAuthCache, retry to ask the user for a new one.
    if (error == ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED &&
        server_ssl_client_cert_was_cached_ && !HasExceededMaxRetries()) {
      server_ssl_client_cert_was_cached_ = false;
      server_ssl_config_.send_client_cert = false;
      server_ssl_config_.client_cert = nullptr;
      server_ssl_config_.client_private_key = nullptr;
      retry_attempts_++;
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      ResetConnectionAndRequestForResend();
      return OK;
    }
  }
  return error;
}

// This method determines whether it is safe to resend the request after an
// IO error.  It can only be called in response to request header or body
// write errors or response header read errors.  It should not be used in
// other cases, such as a Connect error.
int HttpNetworkTransaction::HandleIOError(int error) {
  // Because the peer may request renegotiation with client authentication at
  // any time, check and handle client authentication errors.
  error = HandleSSLClientAuthError(error);

  switch (error) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_ABORTED:
    // There can be a race between the socket pool checking checking whether a
    // socket is still connected, receiving the FIN, and sending/reading data
    // on a reused socket.  If we receive the FIN between the connectedness
    // check and writing/reading from the socket, we may first learn the socket
    // is disconnected when we get a ERR_SOCKET_NOT_CONNECTED.  This will most
    // likely happen when trying to retrieve its IP address.
    // See http://crbug.com/105824 for more details.
    case ERR_SOCKET_NOT_CONNECTED:
    // If a socket is closed on its initial request, HttpStreamParser returns
    // ERR_EMPTY_RESPONSE. This may still be close/reuse race if the socket was
    // preconnected but failed to be used before the server timed it out.
    case ERR_EMPTY_RESPONSE:
      if (ShouldResendRequest()) {
        net_log_.AddEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
    case ERR_EARLY_DATA_REJECTED:
    case ERR_WRONG_VERSION_ON_EARLY_DATA:
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      // Disable early data on the SSLConfig on a reset.
      can_send_early_data_ = false;
      ResetConnectionAndRequestForResend();
      error = OK;
      break;
    case ERR_SPDY_PING_FAILED:
    case ERR_SPDY_SERVER_REFUSED_STREAM:
    case ERR_SPDY_PUSHED_STREAM_NOT_AVAILABLE:
    case ERR_SPDY_CLAIMED_PUSHED_STREAM_RESET_BY_SERVER:
    case ERR_SPDY_PUSHED_RESPONSE_DOES_NOT_MATCH:
    case ERR_QUIC_HANDSHAKE_FAILED:
      if (HasExceededMaxRetries())
        break;
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      retry_attempts_++;
      ResetConnectionAndRequestForResend();
      error = OK;
      break;
    case ERR_QUIC_PROTOCOL_ERROR:
      if (GetResponseHeaders() != nullptr ||
          !stream_->GetAlternativeService(&retried_alternative_service_)) {
        // If the response headers have already been recieved and passed up
        // then the request can not be retried. Also, if there was no
        // alternative service used for this request, then there is no
        // alternative service to be disabled.
        break;
      }
      if (HasExceededMaxRetries())
        break;
      if (session_->http_server_properties()->IsAlternativeServiceBroken(
              retried_alternative_service_)) {
        // If the alternative service was marked as broken while the request
        // was in flight, retry the request which will not use the broken
        // alternative service.
        net_log_.AddEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        retry_attempts_++;
        ResetConnectionAndRequestForResend();
        error = OK;
      } else if (session_->params().retry_without_alt_svc_on_quic_errors) {
        // Disable alternative services for this request and retry it. If the
        // retry succeeds, then the alternative service will be marked as
        // broken then.
        enable_alternative_services_ = false;
        net_log_.AddEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        retry_attempts_++;
        ResetConnectionAndRequestForResend();
        error = OK;
      }
      break;
  }
  return error;
}

void HttpNetworkTransaction::ResetStateForRestart() {
  ResetStateForAuthRestart();
  if (stream_) {
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
  }
  CacheNetErrorDetailsAndResetStream();
}

void HttpNetworkTransaction::ResetStateForAuthRestart() {
  send_start_time_ = base::TimeTicks();
  send_end_time_ = base::TimeTicks();

  pending_auth_target_ = HttpAuth::AUTH_NONE;
  read_buf_ = NULL;
  read_buf_len_ = 0;
  headers_valid_ = false;
  request_headers_.Clear();
  response_ = HttpResponseInfo();
  establishing_tunnel_ = false;
  remote_endpoint_ = IPEndPoint();
  net_error_details_.quic_broken = false;
  net_error_details_.quic_connection_error = quic::QUIC_NO_ERROR;
}

void HttpNetworkTransaction::CacheNetErrorDetailsAndResetStream() {
  if (stream_)
    stream_->PopulateNetErrorDetails(&net_error_details_);
  stream_.reset();
}

HttpResponseHeaders* HttpNetworkTransaction::GetResponseHeaders() const {
  return response_.headers.get();
}

bool HttpNetworkTransaction::ShouldResendRequest() const {
  bool connection_is_proven = stream_->IsConnectionReused();
  bool has_received_headers = GetResponseHeaders() != NULL;

  // NOTE: we resend a request only if we reused a keep-alive connection.
  // This automatically prevents an infinite resend loop because we'll run
  // out of the cached keep-alive connections eventually.
  if (connection_is_proven && !has_received_headers)
    return true;
  return false;
}

bool HttpNetworkTransaction::HasExceededMaxRetries() const {
  return (retry_attempts_ >= kMaxRetryAttempts);
}

bool HttpNetworkTransaction::CheckMaxRestarts() {
  num_restarts_++;
  return num_restarts_ < kMaxRestarts;
}

void HttpNetworkTransaction::ResetConnectionAndRequestForResend() {
  if (stream_.get()) {
    stream_->Close(true);
    CacheNetErrorDetailsAndResetStream();
  }

  // We need to clear request_headers_ because it contains the real request
  // headers, but we may need to resend the CONNECT request first to recreate
  // the SSL tunnel.
  request_headers_.Clear();
  next_state_ = STATE_CREATE_STREAM;  // Resend the request.
}

bool HttpNetworkTransaction::ShouldApplyProxyAuth() const {
  return UsingHttpProxyWithoutTunnel();
}

bool HttpNetworkTransaction::ShouldApplyServerAuth() const {
  return !(request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA);
}

int HttpNetworkTransaction::HandleAuthChallenge() {
  scoped_refptr<HttpResponseHeaders> headers(GetResponseHeaders());
  DCHECK(headers.get());

  int status = headers->response_code();
  if (status != HTTP_UNAUTHORIZED &&
      status != HTTP_PROXY_AUTHENTICATION_REQUIRED)
    return OK;
  HttpAuth::Target target = status == HTTP_PROXY_AUTHENTICATION_REQUIRED ?
                            HttpAuth::AUTH_PROXY : HttpAuth::AUTH_SERVER;
  if (target == HttpAuth::AUTH_PROXY && proxy_info_.is_direct())
    return ERR_UNEXPECTED_PROXY_AUTH;

  // This case can trigger when an HTTPS server responds with a "Proxy
  // authentication required" status code through a non-authenticating
  // proxy.
  if (!auth_controllers_[target].get())
    return ERR_UNEXPECTED_PROXY_AUTH;

  int rv = auth_controllers_[target]->HandleAuthChallenge(
      headers, response_.ssl_info,
      (request_->load_flags & LOAD_DO_NOT_SEND_AUTH_DATA) != 0, false,
      net_log_);
  if (auth_controllers_[target]->HaveAuthHandler())
    pending_auth_target_ = target;

  scoped_refptr<AuthChallengeInfo> auth_info =
      auth_controllers_[target]->auth_info();
  if (auth_info.get())
      response_.auth_challenge = auth_info;

  return rv;
}

bool HttpNetworkTransaction::HaveAuth(HttpAuth::Target target) const {
  return auth_controllers_[target].get() &&
      auth_controllers_[target]->HaveAuth();
}

GURL HttpNetworkTransaction::AuthURL(HttpAuth::Target target) const {
  switch (target) {
    case HttpAuth::AUTH_PROXY: {
      if (!proxy_info_.proxy_server().is_valid() ||
          proxy_info_.proxy_server().is_direct()) {
        return GURL();  // There is no proxy server.
      }
      const char* scheme = proxy_info_.is_https() ? "https://" : "http://";
      return GURL(scheme +
                  proxy_info_.proxy_server().host_port_pair().ToString());
    }
    case HttpAuth::AUTH_SERVER:
      if (ForWebSocketHandshake()) {
        const GURL& url = request_->url;
        url::Replacements<char> ws_to_http;
        if (url.SchemeIs("ws")) {
          ws_to_http.SetScheme("http", url::Component(0, 4));
        } else {
          DCHECK(url.SchemeIs("wss"));
          ws_to_http.SetScheme("https", url::Component(0, 5));
        }
        return url.ReplaceComponents(ws_to_http);
      }
      return request_->url;
    default:
     return GURL();
  }
}

bool HttpNetworkTransaction::ForWebSocketHandshake() const {
  return websocket_handshake_stream_base_create_helper_ &&
         request_->url.SchemeIsWSOrWSS();
}

void HttpNetworkTransaction::CopyConnectionAttemptsFromStreamRequest() {
  DCHECK(stream_request_);

  // Since the transaction can restart with auth credentials, it may create a
  // stream more than once. Accumulate all of the connection attempts across
  // those streams by appending them to the vector:
  for (const auto& attempt : stream_request_->connection_attempts())
    connection_attempts_.push_back(attempt);
}

bool HttpNetworkTransaction::ContentEncodingsValid() const {
  HttpResponseHeaders* headers = GetResponseHeaders();
  DCHECK(headers);

  std::string accept_encoding;
  request_headers_.GetHeader(HttpRequestHeaders::kAcceptEncoding,
                             &accept_encoding);
  std::set<std::string> allowed_encodings;
  if (!HttpUtil::ParseAcceptEncoding(accept_encoding, &allowed_encodings)) {
    FilterSourceStream::ReportContentDecodingFailed(SourceStream::TYPE_INVALID);
    return false;
  }

  std::string content_encoding;
  headers->GetNormalizedHeader("Content-Encoding", &content_encoding);
  std::set<std::string> used_encodings;
  if (!HttpUtil::ParseContentEncoding(content_encoding, &used_encodings)) {
    FilterSourceStream::ReportContentDecodingFailed(SourceStream::TYPE_INVALID);
    return false;
  }

  // When "Accept-Encoding" is not specified, it is parsed as "*".
  // If "*" encoding is advertised, then any encoding should be "accepted".
  // This does not mean, that it will be successfully decoded.
  if (allowed_encodings.find("*") != allowed_encodings.end())
    return true;

  bool result = true;
  for (auto const& encoding : used_encodings) {
    SourceStream::SourceType source_type =
        FilterSourceStream::ParseEncodingType(encoding);
    // We don't reject encodings we are not aware. They just will not decode.
    if (source_type == SourceStream::TYPE_UNKNOWN)
      continue;
    if (allowed_encodings.find(encoding) == allowed_encodings.end()) {
      FilterSourceStream::ReportContentDecodingFailed(
          SourceStream::TYPE_REJECTED);
      result = false;
      break;
    }
  }

  // Temporary workaround for http://crbug.com/714514
  if (headers->IsRedirect(nullptr)) {
    UMA_HISTOGRAM_BOOLEAN("Net.RedirectWithUnadvertisedContentEncoding",
                          !result);
    return true;
  }

  return result;
}

}  // namespace net

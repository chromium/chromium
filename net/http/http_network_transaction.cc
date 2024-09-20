// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_network_transaction.h"

#include <set>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/transport_info.h"
#include "net/base/upload_data_stream.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/filter/filter_source_stream.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_log_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_status_code.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/http/url_security_manager.h"
#include "net/log/net_log_event_type.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/next_proto.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_private_key.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace net {

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

// Returns true when Early Hints are allowed on the given protocol.
bool EarlyHintsAreAllowedOn(HttpConnectionInfo connection_info) {
  switch (connection_info) {
    case HttpConnectionInfo::kHTTP0_9:
    case HttpConnectionInfo::kHTTP1_0:
      return false;
    case HttpConnectionInfo::kHTTP1_1:
      return base::FeatureList::IsEnabled(features::kEnableEarlyHintsOnHttp11);
    default:
      // Implicitly allow HttpConnectionInfo::kUNKNOWN because this is the
      // default value and ConnectionInfo isn't always set.
      return true;
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebSocketFallbackResult {
  kSuccessHttp11 = 0,
  kSuccessHttp2,
  kSuccessHttp11AfterFallback,
  kFailure,
  kFailureAfterFallback,
  kMaxValue = kFailureAfterFallback,
};

WebSocketFallbackResult CalculateWebSocketFallbackResult(
    int result,
    bool http_1_1_was_required,
    HttpConnectionInfoCoarse connection_info) {
  if (result == OK) {
    if (connection_info == HttpConnectionInfoCoarse::kHTTP2) {
      return WebSocketFallbackResult::kSuccessHttp2;
    }
    return http_1_1_was_required
               ? WebSocketFallbackResult::kSuccessHttp11AfterFallback
               : WebSocketFallbackResult::kSuccessHttp11;
  }

  return http_1_1_was_required ? WebSocketFallbackResult::kFailureAfterFallback
                               : WebSocketFallbackResult::kFailure;
}

void RecordWebSocketFallbackResult(int result,
                                   bool http_1_1_was_required,
                                   HttpConnectionInfoCoarse connection_info) {
  CHECK_NE(connection_info, HttpConnectionInfoCoarse::kQUIC);

  // `connection_info` could be kOTHER in tests.
  if (connection_info == HttpConnectionInfoCoarse::kOTHER) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Net.WebSocket.FallbackResult",
      CalculateWebSocketFallbackResult(result, http_1_1_was_required,
                                       connection_info));
}

const std::string_view NegotiatedProtocolToHistogramSuffix(
    const HttpResponseInfo& response) {
  NextProto next_proto = NextProtoFromString(response.alpn_negotiated_protocol);
  switch (next_proto) {
    case kProtoHTTP11:
      return "H1";
    case kProtoHTTP2:
      return "H2";
    case kProtoQUIC:
      return "H3";
    case kProtoUnknown:
      return "Unknown";
  }
}

}  // namespace

const int HttpNetworkTransaction::kDrainBodyBufferSize;

HttpNetworkTransaction::HttpNetworkTransaction(RequestPriority priority,
                                               HttpNetworkSession* session)
    : io_callback_(base::BindRepeating(&HttpNetworkTransaction::OnIOComplete,
                                       base::Unretained(this))),
      session_(session),
      priority_(priority) {}

HttpNetworkTransaction::~HttpNetworkTransaction() {
#if BUILDFLAG(ENABLE_REPORTING)
  // If no error or success report has been generated yet at this point, then
  // this network transaction was prematurely cancelled.
  GenerateNetworkErrorLoggingReport(ERR_ABORTED);
#endif  // BUILDFLAG(ENABLE_REPORTING)

  if (stream_.get()) {
    // TODO(mbelshe): The stream_ should be able to compute whether or not the
    //                stream should be kept alive.  No reason to compute here
    //                and pass it in.
    if (!stream_->CanReuseConnection() || next_state_ != STATE_NONE ||
        close_connection_on_destruction_) {
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
  if (request_info->load_flags & LOAD_ONLY_FROM_CACHE)
    return ERR_CACHE_MISS;

  DCHECK(request_info->traffic_annotation.is_valid());
  DCHECK(request_info->IsConsistent());
  net_log_ = net_log;
  request_ = request_info;
  url_ = request_->url;
  network_anonymization_key_ = request_->network_anonymization_key;
#if BUILDFLAG(ENABLE_REPORTING)
  // Store values for later use in NEL report generation.
  request_method_ = request_->method;
  if (std::optional<std::string> header =
          request_->extra_headers.GetHeader(HttpRequestHeaders::kReferer);
      header) {
    request_referrer_.swap(header.value());
  }
  if (std::optional<std::string> header =
          request_->extra_headers.GetHeader(HttpRequestHeaders::kUserAgent);
      header) {
    request_user_agent_.swap(header.value());
  }
  request_reporting_upload_depth_ = request_->reporting_upload_depth;
  start_timeticks_ = base::TimeTicks::Now();
#endif  // BUILDFLAG(ENABLE_REPORTING)

  if (request_->idempotency == IDEMPOTENT ||
      (request_->idempotency == DEFAULT_IDEMPOTENCY &&
       HttpUtil::IsMethodSafe(request_info->method))) {
    can_send_early_data_ = true;
  }

  if (request_->load_flags & LOAD_PREFETCH) {
    response_.unused_since_prefetch = true;
  }

  if (request_->load_flags & LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME) {
    DCHECK(response_.unused_since_prefetch);
    response_.restricted_prefetch = true;
  }

  next_state_ = STATE_NOTIFY_BEFORE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  // This always returns ERR_IO_PENDING because DoCreateStream() does, but
  // GenerateNetworkErrorLoggingReportIfError() should be called here if any
  // other Error can be returned.
  DCHECK_EQ(rv, ERR_IO_PENDING);
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

  // This always returns ERR_IO_PENDING because DoCreateStream() does, but
  // GenerateNetworkErrorLoggingReportIfError() should be called here if any
  // other Error can be returned.
  DCHECK_EQ(rv, ERR_IO_PENDING);
  return rv;
}

int HttpNetworkTransaction::RestartWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key,
    CompletionOnceCallback callback) {
  // When we receive ERR_SSL_CLIENT_AUTH_CERT_NEEDED, we always tear down
  // existing streams and stream requests to force a new connection.
  DCHECK(!stream_request_.get());
  DCHECK(!stream_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  if (!CheckMaxRestarts())
    return ERR_TOO_MANY_RETRIES;

  // Add the credentials to the client auth cache. The next stream request will
  // then pick them up.
  session_->ssl_client_context()->SetClientCertificate(
      response_.cert_request_info->host_and_port, std::move(client_cert),
      std::move(client_private_key));

  if (!response_.cert_request_info->is_proxy)
    configured_client_cert_for_server_ = true;

  // Reset the other member variables.
  // Note: this is necessary only with SSL renegotiation.
  ResetStateForRestart();
  next_state_ = STATE_CREATE_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  // This always returns ERR_IO_PENDING because DoCreateStream() does, but
  // GenerateNetworkErrorLoggingReportIfError() should be called here if any
  // other Error can be returned.
  DCHECK_EQ(rv, ERR_IO_PENDING);
  return rv;
}

int HttpNetworkTransaction::RestartWithAuth(const AuthCredentials& credentials,
                                            CompletionOnceCallback callback) {
  if (!CheckMaxRestarts())
    return ERR_TOO_MANY_RETRIES;

  HttpAuth::Target target = pending_auth_target_;
  if (target == HttpAuth::AUTH_NONE) {
    NOTREACHED_IN_MIGRATION();
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
    DCHECK(stream_request_ != nullptr);
    auth_controllers_[target] = nullptr;
    ResetStateForRestart();
    rv = stream_request_->RestartTunnelWithProxyAuth();
  } else {
    // In this case, we've gathered credentials for the server or the proxy
    // but it is not during the tunneling phase.
    DCHECK(stream_request_ == nullptr);
    PrepareForAuthRestart(target);
    rv = DoLoop(OK);
    // Note: If an error is encountered while draining the old response body, no
    // Network Error Logging report will be generated, because the error was
    // with the old request, which will already have had a NEL report generated
    // for it due to the auth challenge (so we don't report a second error for
    // that request).
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
    // SetHTTP11Requited requires URLs be rewritten first, if there are any
    // applicable rules.
    GURL rewritten_url = request_->url;
    session_->params().host_mapping_rules.RewriteUrl(rewritten_url);

    session_->http_server_properties()->SetHTTP11Required(
        url::SchemeHostPort(rewritten_url), network_anonymization_key_);
  }

  bool keep_alive = false;
  // Even if the server says the connection is keep-alive, we have to be
  // able to find the end of each response in order to reuse the connection.
  if (stream_->CanReuseConnection()) {
    // If the response body hasn't been completely read, we need to drain
    // it first.
    if (!stream_->IsResponseBodyComplete()) {
      next_state_ = STATE_DRAIN_BODY_FOR_AUTH_RESTART;
      read_buf_ = base::MakeRefCounted<IOBufferWithSize>(
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
    std::unique_ptr<HttpStream> new_stream;
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
      next_state_ = STATE_CONNECTED_CALLBACK;
    }
    stream_ = std::move(new_stream);
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
    DCHECK(proxy_info_.AnyProxyInChain(
        [](const ProxyServer& s) { return s.is_http_like(); }));
    DCHECK_EQ(headers->response_code(), HTTP_PROXY_AUTHENTICATION_REQUIRED);
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

int64_t HttpNetworkTransaction::GetReceivedBodyBytes() const {
  return received_body_bytes_;
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

  // If `dns_resolution_{start/end}_time_override_` are set, and they are older
  // than `domain_lookup_{start/end}` of the `stream_`, use the overrides.
  // TODO(crbug.com/40812426): Remove this when we launch Happy Eyeballs v3.
  if (!dns_resolution_start_time_override_.is_null() &&
      !dns_resolution_end_time_override_.is_null() &&
      (dns_resolution_start_time_override_ <
       load_timing_info->connect_timing.domain_lookup_start) &&
      (dns_resolution_end_time_override_ <
       load_timing_info->connect_timing.domain_lookup_end)) {
    load_timing_info->connect_timing.domain_lookup_start =
        dns_resolution_start_time_override_;
    load_timing_info->connect_timing.domain_lookup_end =
        dns_resolution_end_time_override_;
  }

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
    BeforeNetworkStartCallback callback) {
  before_network_start_callback_ = std::move(callback);
}

void HttpNetworkTransaction::SetConnectedCallback(
    const ConnectedCallback& callback) {
  connected_callback_ = callback;
}

void HttpNetworkTransaction::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  DCHECK(!stream_);
  request_headers_callback_ = std::move(callback);
}

void HttpNetworkTransaction::SetEarlyResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!stream_);
  early_response_headers_callback_ = std::move(callback);
}

void HttpNetworkTransaction::SetResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!stream_);
  response_headers_callback_ = std::move(callback);
}

void HttpNetworkTransaction::SetModifyRequestHeadersCallback(
    base::RepeatingCallback<void(HttpRequestHeaders*)> callback) {
  modify_headers_callbacks_ = std::move(callback);
}

void HttpNetworkTransaction::SetIsSharedDictionaryReadAllowedCallback(
    base::RepeatingCallback<bool()> callback) {
  // This method should not be called for this class.
  NOTREACHED_IN_MIGRATION();
}

int HttpNetworkTransaction::ResumeNetworkStart() {
  DCHECK_EQ(next_state_, STATE_CREATE_STREAM);
  return DoLoop(OK);
}

void HttpNetworkTransaction::ResumeAfterConnected(int result) {
  DCHECK_EQ(next_state_, STATE_CONNECTED_CALLBACK_COMPLETE);
  OnIOComplete(result);
}

void HttpNetworkTransaction::CloseConnectionOnDestruction() {
  close_connection_on_destruction_ = true;
}

bool HttpNetworkTransaction::IsMdlMatchForMetrics() const {
  return proxy_info_.is_mdl_match();
}

void HttpNetworkTransaction::OnStreamReady(const ProxyInfo& used_proxy_info,
                                           std::unique_ptr<HttpStream> stream) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK(stream_request_.get());

  if (stream_) {
    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
  }
  stream_ = std::move(stream);
  stream_->SetRequestHeadersCallback(request_headers_callback_);
  proxy_info_ = used_proxy_info;
  // TODO(crbug.com/40473589): Remove `was_alpn_negotiated` when we remove
  // chrome.loadTimes API.
  response_.was_alpn_negotiated =
      stream_request_->negotiated_protocol() != kProtoUnknown;
  response_.alpn_negotiated_protocol =
      NextProtoToString(stream_request_->negotiated_protocol());
  response_.alternate_protocol_usage =
      stream_request_->alternate_protocol_usage();
  // TODO(crbug.com/40815866): Stop using `was_fetched_via_spdy`.
  response_.was_fetched_via_spdy =
      stream_request_->negotiated_protocol() == kProtoHTTP2;
  response_.dns_aliases = stream_->GetDnsAliases();

  dns_resolution_start_time_override_ =
      stream_request_->dns_resolution_start_time_override();
  dns_resolution_end_time_override_ =
      stream_request_->dns_resolution_end_time_override();

  SetProxyInfoInResponse(used_proxy_info, &response_);
  OnIOComplete(OK);
}

void HttpNetworkTransaction::OnBidirectionalStreamImplReady(
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<BidirectionalStreamImpl> stream) {
  NOTREACHED_IN_MIGRATION();
}

void HttpNetworkTransaction::OnWebSocketHandshakeStreamReady(
    const ProxyInfo& used_proxy_info,
    std::unique_ptr<WebSocketHandshakeStreamBase> stream) {
  OnStreamReady(used_proxy_info, std::move(stream));
}

void HttpNetworkTransaction::OnStreamFailed(
    int result,
    const NetErrorDetails& net_error_details,
    const ProxyInfo& used_proxy_info,
    ResolveErrorInfo resolve_error_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());
  net_error_details_ = net_error_details;
  proxy_info_ = used_proxy_info;
  SetProxyInfoInResponse(used_proxy_info, &response_);
  response_.resolve_error_info = resolve_error_info;

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnCertificateError(int result,
                                                const SSLInfo& ssl_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  DCHECK_NE(OK, result);
  DCHECK(stream_request_.get());
  DCHECK(!stream_.get());

  response_.ssl_info = ssl_info;
  if (ssl_info.cert) {
    observed_bad_certs_.emplace_back(ssl_info.cert, ssl_info.cert_status);
  }

  // TODO(mbelshe):  For now, we're going to pass the error through, and that
  // will close the stream_request in all cases.  This means that we're always
  // going to restart an entire STATE_CREATE_STREAM, even if the connection is
  // good and the user chooses to ignore the error.  This is not ideal, but not
  // the end of the world either.

  OnIOComplete(result);
}

void HttpNetworkTransaction::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  DCHECK(stream_request_.get());
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  establishing_tunnel_ = true;
  response_.headers = proxy_response.headers;
  response_.auth_challenge = proxy_response.auth_challenge;
  response_.did_use_http_auth = proxy_response.did_use_http_auth;
  SetProxyInfoInResponse(used_proxy_info, &response_);

  if (!ContentEncodingsValid()) {
    DoCallback(ERR_CONTENT_DECODING_FAILED);
    return;
  }

  headers_valid_ = true;
  proxy_info_ = used_proxy_info;

  auth_controllers_[HttpAuth::AUTH_PROXY] = auth_controller;
  pending_auth_target_ = HttpAuth::AUTH_PROXY;

  DoCallback(OK);
}

void HttpNetworkTransaction::OnNeedsClientAuth(SSLCertRequestInfo* cert_info) {
  DCHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);

  response_.cert_request_info = cert_info;
  OnIOComplete(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

void HttpNetworkTransaction::OnQuicBroken() {
  net_error_details_.quic_broken = true;
}

void HttpNetworkTransaction::OnSwitchesToHttpStreamPool(
    HttpStreamPoolSwitchingInfo switching_info) {
  CHECK_EQ(STATE_CREATE_STREAM_COMPLETE, next_state_);
  CHECK(stream_request_);
  stream_request_.reset();

  stream_request_ = session_->http_stream_pool()->RequestStream(
      this, std::move(switching_info), priority_,
      /*allowed_bad_certs=*/observed_bad_certs_, enable_ip_based_pooling_,
      enable_alternative_services_, net_log_);
  CHECK(!stream_request_->completed());
  // No IO completion yet.
}

ConnectionAttempts HttpNetworkTransaction::GetConnectionAttempts() const {
  return connection_attempts_;
}

bool HttpNetworkTransaction::IsSecureRequest() const {
  return request_->url.SchemeIsCryptographic();
}

bool HttpNetworkTransaction::UsingHttpProxyWithoutTunnel() const {
  return proxy_info_.proxy_chain().is_get_to_proxy_allowed() &&
         request_->url.SchemeIs("http");
}

void HttpNetworkTransaction::DoCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!callback_.is_null());

#if BUILDFLAG(ENABLE_REPORTING)
  // Just before invoking the caller's completion callback, generate a NEL
  // report about this network request if the result was an error.
  GenerateNetworkErrorLoggingReportIfError(rv);
#endif  // BUILDFLAG(ENABLE_REPORTING)

  // Since Run may result in Read being called, clear user_callback_ up front.
  std::move(callback_).Run(rv);
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
        rv = DoCreateStreamComplete(rv);
        break;
      case STATE_INIT_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoInitStream();
        break;
      case STATE_INIT_STREAM_COMPLETE:
        rv = DoInitStreamComplete(rv);
        break;
      case STATE_CONNECTED_CALLBACK:
        rv = DoConnectedCallback();
        break;
      case STATE_CONNECTED_CALLBACK_COMPLETE:
        rv = DoConnectedCallbackComplete(rv);
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
        NOTREACHED_IN_MIGRATION() << "bad state";
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
    std::move(before_network_start_callback_).Run(&defer);
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

  create_stream_start_time_ = base::TimeTicks::Now();
  if (ForWebSocketHandshake()) {
    stream_request_ =
        session_->http_stream_factory()->RequestWebSocketHandshakeStream(
            *request_, priority_, /*allowed_bad_certs=*/observed_bad_certs_,
            this, websocket_handshake_stream_base_create_helper_,
            enable_ip_based_pooling_, enable_alternative_services_, net_log_);
  } else {
    stream_request_ = session_->http_stream_factory()->RequestStream(
        *request_, priority_, /*allowed_bad_certs=*/observed_bad_certs_, this,
        enable_ip_based_pooling_, enable_alternative_services_, net_log_);
  }
  DCHECK(stream_request_.get());
  return ERR_IO_PENDING;
}

int HttpNetworkTransaction::DoCreateStreamComplete(int result) {
  CopyConnectionAttemptsFromStreamRequest();
  if (result == OK) {
    next_state_ = STATE_CONNECTED_CALLBACK;
    DCHECK(stream_.get());
    CHECK(!create_stream_start_time_.is_null());
    base::UmaHistogramTimes(
        base::StrCat(
            {"Net.NetworkTransaction.Create",
             (ForWebSocketHandshake() ? "WebSocketStreamTime."
                                      : "HttpStreamTime."),
             (IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ""),
             NegotiatedProtocolToHistogramSuffix(response_)}),
        base::TimeTicks::Now() - create_stream_start_time_);
  } else if (result == ERR_HTTP_1_1_REQUIRED ||
             result == ERR_PROXY_HTTP_1_1_REQUIRED) {
    return HandleHttp11Required(result);
  } else {
    // Handle possible client certificate errors that may have occurred if the
    // stream used SSL for one or more of the layers.
    result = HandleSSLClientAuthError(result);
  }

  // At this point we are done with the stream_request_.
  stream_request_.reset();
  return result;
}

int HttpNetworkTransaction::DoInitStream() {
  DCHECK(stream_.get());
  next_state_ = STATE_INIT_STREAM_COMPLETE;

  base::TimeTicks now = base::TimeTicks::Now();
  int rv = stream_->InitializeStream(can_send_early_data_, priority_, net_log_,
                                     io_callback_);

  // TODO(crbug.com/359404121): Remove this histogram after the investigation
  // completes.
  bool blocked = rv == ERR_IO_PENDING;
  if (blocked) {
    blocked_initialize_stream_start_time_ = now;
  }
  base::UmaHistogramBoolean(
      base::StrCat({"Net.NetworkTransaction.InitializeStreamBlocked",
                    IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ".",
                    NegotiatedProtocolToHistogramSuffix(response_)}),
      blocked);
  return rv;
}

int HttpNetworkTransaction::DoInitStreamComplete(int result) {
  // TODO(crbug.com/359404121): Remove this histogram after the investigation
  // completes.
  if (!blocked_initialize_stream_start_time_.is_null()) {
    base::UmaHistogramTimes(
        base::StrCat({"Net.NetworkTransaction.InitializeStreamBlockTime",
                      IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ".",
                      NegotiatedProtocolToHistogramSuffix(response_)}),
        base::TimeTicks::Now() - blocked_initialize_stream_start_time_);
  }

  if (result != OK) {
    if (result < 0)
      result = HandleIOError(result);

    // The stream initialization failed, so this stream will never be useful.
    if (stream_) {
      total_received_bytes_ += stream_->GetTotalReceivedBytes();
      total_sent_bytes_ += stream_->GetTotalSentBytes();
    }
    CacheNetErrorDetailsAndResetStream();

    return result;
  }

  next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN;
  return result;
}

int HttpNetworkTransaction::DoConnectedCallback() {
  // Register the HttpRequestInfo object on the stream here so that it's
  // available when invoking the `connected_callback_`, as
  // HttpStream::GetAcceptChViaAlps() needs the HttpRequestInfo to retrieve
  // the ACCEPT_CH frame payload.
  stream_->RegisterRequest(request_);
  next_state_ = STATE_CONNECTED_CALLBACK_COMPLETE;

  int result = stream_->GetRemoteEndpoint(&remote_endpoint_);
  if (result != OK) {
    // `GetRemoteEndpoint()` fails when the underlying socket is not connected
    // anymore, even though the peer's address is known. This can happen when
    // we picked a socket from socket pools while it was still connected, but
    // the remote side closes it before we get a chance to send our request.
    // See if we should retry the request based on the error code we got.
    return HandleIOError(result);
  }

  if (connected_callback_.is_null()) {
    return OK;
  }

  // Fire off notification that we have successfully connected.
  TransportType type = TransportType::kDirect;
  if (!proxy_info_.is_direct()) {
    type = TransportType::kProxied;
  }

  bool is_issued_by_known_root = false;
  if (IsSecureRequest()) {
    SSLInfo ssl_info;
    CHECK(stream_);
    stream_->GetSSLInfo(&ssl_info);
    is_issued_by_known_root = ssl_info.is_issued_by_known_root;
  }

  return connected_callback_.Run(
      TransportInfo(type, remote_endpoint_,
                    std::string{stream_->GetAcceptChViaAlps()},
                    is_issued_by_known_root,
                    NextProtoFromString(response_.alpn_negotiated_protocol)),
      base::BindOnce(&HttpNetworkTransaction::ResumeAfterConnected,
                     base::Unretained(this)));
}

int HttpNetworkTransaction::DoConnectedCallbackComplete(int result) {
  if (result != OK) {
    if (stream_) {
      stream_->Close(/*not_reusable=*/false);
    }

    // Stop the state machine here if the call failed.
    return result;
  }

  next_state_ = STATE_INIT_STREAM;
  return OK;
}

int HttpNetworkTransaction::DoGenerateProxyAuthToken() {
  next_state_ = STATE_GENERATE_PROXY_AUTH_TOKEN_COMPLETE;
  if (!ShouldApplyProxyAuth())
    return OK;
  HttpAuth::Target target = HttpAuth::AUTH_PROXY;
  if (!auth_controllers_[target].get())
    auth_controllers_[target] = base::MakeRefCounted<HttpAuthController>(
        target, AuthURL(target), request_->network_anonymization_key,
        session_->http_auth_cache(), session_->http_auth_handler_factory(),
        session_->host_resolver());
  int rv = auth_controllers_[target]->MaybeGenerateAuthToken(
      request_, io_callback_, net_log_);
  // TODO(crbug.com/359404121): Remove this histogram after the investigation
  // completes.
  const bool blocked = rv == ERR_IO_PENDING;
  if (blocked) {
    blocked_generate_proxy_auth_token_start_time_ = base::TimeTicks::Now();
  }
  base::UmaHistogramBoolean(
      base::StrCat({"Net.NetworkTransaction.GenerateProxyAuthTokenBlocked",
                    IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ".",
                    NegotiatedProtocolToHistogramSuffix(response_)}),
      blocked);
  return rv;
}

int HttpNetworkTransaction::DoGenerateProxyAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  // TODO(crbug.com/359404121): Remove this histogram after the investigation
  // completes.
  if (!blocked_generate_proxy_auth_token_start_time_.is_null()) {
    base::UmaHistogramTimes(
        base::StrCat({"Net.NetworkTransaction.GenerateProxyAuthTokenBlockTime",
                      IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ".",
                      NegotiatedProtocolToHistogramSuffix(response_)}),
        base::TimeTicks::Now() - blocked_generate_proxy_auth_token_start_time_);
  }
  if (rv == OK)
    next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN;
  return rv;
}

int HttpNetworkTransaction::DoGenerateServerAuthToken() {
  next_state_ = STATE_GENERATE_SERVER_AUTH_TOKEN_COMPLETE;
  HttpAuth::Target target = HttpAuth::AUTH_SERVER;
  if (!auth_controllers_[target].get()) {
    auth_controllers_[target] = base::MakeRefCounted<HttpAuthController>(
        target, AuthURL(target), request_->network_anonymization_key,
        session_->http_auth_cache(), session_->http_auth_handler_factory(),
        session_->host_resolver());
    if (request_->load_flags & LOAD_DO_NOT_USE_EMBEDDED_IDENTITY)
      auth_controllers_[target]->DisableEmbeddedIdentity();
  }
  if (!ShouldApplyServerAuth())
    return OK;
  int rv = auth_controllers_[target]->MaybeGenerateAuthToken(
      request_, io_callback_, net_log_);
  // TODO(crbug.com/359404121): Remove this histogram after the investigation
  // completes.
  const bool blocked = rv == ERR_IO_PENDING;
  if (blocked) {
    blocked_generate_server_auth_token_start_time_ = base::TimeTicks::Now();
  }
  base::UmaHistogramBoolean(
      base::StrCat({"Net.NetworkTransaction.GenerateServerAuthTokenBlocked",
                    IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ".",
                    NegotiatedProtocolToHistogramSuffix(response_)}),
      blocked);
  return rv;
}

int HttpNetworkTransaction::DoGenerateServerAuthTokenComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  // TODO(crbug.com/359404121): Remove this histogram after the investigation
  // completes.
  if (!blocked_generate_server_auth_token_start_time_.is_null()) {
    base::UmaHistogramTimes(
        base::StrCat({"Net.NetworkTransaction.GenerateServerAuthTokenBlockTime",
                      IsGoogleHostWithAlpnH3(url_.host()) ? "GoogleHost." : ".",
                      NegotiatedProtocolToHistogramSuffix(response_)}),
        base::TimeTicks::Now() -
            blocked_generate_server_auth_token_start_time_);
  }
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

  if (features::kIpPrivacyAddHeaderToProxiedRequests.Get() &&
      proxy_info_.is_for_ip_protection()) {
    CHECK(!proxy_info_.is_direct() || features::kIpPrivacyDirectOnly.Get());
    if (!proxy_info_.is_direct()) {
      request_headers_.SetHeader("IP-Protection", "1");
    }
  }

  request_headers_.MergeFrom(request_->extra_headers);

  if (modify_headers_callbacks_) {
    modify_headers_callbacks_.Run(&request_headers_);
  }

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

  stream_->SetRequestIdempotency(request_->idempotency);
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
  // We can get a ERR_SSL_CLIENT_AUTH_CERT_NEEDED here due to SSL renegotiation.
  // Server certificate errors are impossible. Rather than reverify the new
  // server certificate, BoringSSL forbids server certificates from changing.
  DCHECK(!IsCertificateError(result));
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    DCHECK(stream_.get());
    DCHECK(IsSecureRequest());
    // Should only reach this code when there's a certificate request.
    CHECK(response_.cert_request_info);

    total_received_bytes_ += stream_->GetTotalReceivedBytes();
    total_sent_bytes_ += stream_->GetTotalSentBytes();
    stream_->Close(true);
    CacheNetErrorDetailsAndResetStream();
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

  if (ForWebSocketHandshake()) {
    RecordWebSocketFallbackResult(
        result, http_1_1_was_required_,
        HttpConnectionInfoToCoarse(response_.connection_info));
  }

  if (result < 0)
    return HandleIOError(result);

  DCHECK(response_.headers.get());

  // Check for a 103 Early Hints response.
  if (response_.headers->response_code() == HTTP_EARLY_HINTS) {
    NetLogResponseHeaders(
        net_log_,
        NetLogEventType::HTTP_TRANSACTION_READ_EARLY_HINTS_RESPONSE_HEADERS,
        response_.headers.get());

    // Early Hints does not make sense for a WebSocket handshake.
    if (ForWebSocketHandshake()) {
      return ERR_FAILED;
    }

    // TODO(crbug.com/40496584): Validate headers?  "Content-Encoding" etc
    // should not appear since informational responses can't contain content.
    // https://www.rfc-editor.org/rfc/rfc9110#name-informational-1xx

    if (EarlyHintsAreAllowedOn(response_.connection_info) &&
        early_response_headers_callback_) {
      early_response_headers_callback_.Run(std::move(response_.headers));
    }

    // Reset response headers for the final response.
    response_.headers =
        base::MakeRefCounted<HttpResponseHeaders>(std::string());
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  if (!ContentEncodingsValid())
    return ERR_CONTENT_DECODING_FAILED;

  // On a 408 response from the server ("Request Timeout") on a stale socket,
  // retry the request for HTTP/1.1 but not HTTP/2 or QUIC because those
  // multiplex requests and have no need for 408.
  if (response_.headers->response_code() == HTTP_REQUEST_TIMEOUT &&
      HttpConnectionInfoToCoarse(response_.connection_info) ==
          HttpConnectionInfoCoarse::kHTTP1 &&
      stream_->IsConnectionReused()) {
#if BUILDFLAG(ENABLE_REPORTING)
    GenerateNetworkErrorLoggingReport(OK);
#endif  // BUILDFLAG(ENABLE_REPORTING)
    net_log_.AddEventWithNetErrorCode(
        NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR,
        response_.headers->response_code());
    // This will close the socket - it would be weird to try and reuse it, even
    // if the server doesn't actually close it.
    ResetConnectionAndRequestForResend(RetryReason::kHttpRequestTimeout);
    return OK;
  }

  NetLogResponseHeaders(net_log_,
                        NetLogEventType::HTTP_TRANSACTION_READ_RESPONSE_HEADERS,
                        response_.headers.get());
  if (response_headers_callback_)
    response_headers_callback_.Run(response_.headers);

  if (response_.headers->GetHttpVersion() < HttpVersion(1, 0)) {
    // HTTP/0.9 doesn't support the PUT method, so lack of response headers
    // indicates a buggy server.  See:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=193921
    if (request_->method == "PUT")
      return ERR_METHOD_NOT_SUPPORTED;
  }

  if (can_send_early_data_ &&
      response_.headers->response_code() == HTTP_TOO_EARLY) {
    return HandleIOError(ERR_EARLY_DATA_REJECTED);
  }

  // Check for an intermediate 100 Continue response.  An origin server is
  // allowed to send this response even if we didn't ask for it, so we just
  // need to skip over it.
  // We treat any other 1xx in this same way unless:
  //  * The response is 103, which is already handled above
  //  * This is a WebSocket request, in which case we pass it on up.
  if (response_.headers->response_code() / 100 == 1 &&
      !ForWebSocketHandshake()) {
    response_.headers =
        base::MakeRefCounted<HttpResponseHeaders>(std::string());
    next_state_ = STATE_READ_HEADERS;
    return OK;
  }

  const bool has_body_with_null_source =
      request_->upload_data_stream &&
      request_->upload_data_stream->has_null_source();
  if (response_.headers->response_code() == 421 &&
      (enable_ip_based_pooling_ || enable_alternative_services_) &&
      !has_body_with_null_source) {
#if BUILDFLAG(ENABLE_REPORTING)
    GenerateNetworkErrorLoggingReport(OK);
#endif  // BUILDFLAG(ENABLE_REPORTING)
    // Retry the request with both IP based pooling and Alternative Services
    // disabled.
    enable_ip_based_pooling_ = false;
    enable_alternative_services_ = false;
    net_log_.AddEvent(
        NetLogEventType::HTTP_TRANSACTION_RESTART_MISDIRECTED_REQUEST);
    ResetConnectionAndRequestForResend(RetryReason::kHttpMisdirectedRequest);
    return OK;
  }

  if (IsSecureRequest()) {
    stream_->GetSSLInfo(&response_.ssl_info);
    if (response_.ssl_info.is_valid() &&
        !IsCertStatusError(response_.ssl_info.cert_status)) {
      session_->http_stream_factory()->ProcessAlternativeServices(
          session_, network_anonymization_key_, response_.headers.get(),
          url::SchemeHostPort(request_->url));
    }
  }

  int rv = HandleAuthChallenge();
  if (rv != OK)
    return rv;

#if BUILDFLAG(ENABLE_REPORTING)
  // Note: This just handles the legacy Report-To header, which is still
  // required for NEL. The newer Reporting-Endpoints header is processed in
  // network::PopulateParsedHeaders().
  ProcessReportToHeader();

  // Note: Unless there is a pre-existing NEL policy for this origin, any NEL
  // reports generated before the NEL header is processed here will just be
  // dropped by the NetworkErrorLoggingService.
  ProcessNetworkErrorLoggingHeader();

  // Generate NEL report here if we have to report an HTTP error (4xx or 5xx
  // code), or if the response body will not be read, or on a redirect.
  // Note: This will report a success for a redirect even if an error is
  // encountered later while draining the body.
  int response_code = response_.headers->response_code();
  if ((response_code >= 400 && response_code < 600) ||
      response_code == HTTP_NO_CONTENT || response_code == HTTP_RESET_CONTENT ||
      response_code == HTTP_NOT_MODIFIED || request_->method == "HEAD" ||
      response_.headers->GetContentLength() == 0 ||
      response_.headers->IsRedirect(nullptr /* location */)) {
    GenerateNetworkErrorLoggingReport(OK);
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

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
  DCHECK(stream_ != nullptr);

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
  } else {
    received_body_bytes_ += result;
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
      HistogramBrokenAlternateProtocolLocation(
          BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_NETWORK_TRANSACTION);
      session_->http_server_properties()->MarkAlternativeServiceBroken(
          retried_alternative_service_, network_anonymization_key_);
    }

#if BUILDFLAG(ENABLE_REPORTING)
    GenerateNetworkErrorLoggingReport(result);
#endif  // BUILDFLAG(ENABLE_REPORTING)
  }

  // Clear these to avoid leaving around old state.
  read_buf_ = nullptr;
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
    // Note: No Network Error Logging report is generated here because a report
    // will have already been generated for the original request due to the auth
    // challenge, so a second report is not generated for the same request here.
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

#if BUILDFLAG(ENABLE_REPORTING)
void HttpNetworkTransaction::ProcessReportToHeader() {
  std::string value;
  if (!response_.headers->GetNormalizedHeader("Report-To", &value))
    return;

  ReportingService* reporting_service = session_->reporting_service();
  if (!reporting_service)
    return;

  // Only accept Report-To headers on HTTPS connections that have no
  // certificate errors.
  if (!response_.ssl_info.is_valid())
    return;
  if (IsCertStatusError(response_.ssl_info.cert_status))
    return;

  reporting_service->ProcessReportToHeader(url::Origin::Create(url_),
                                           network_anonymization_key_, value);
}

void HttpNetworkTransaction::ProcessNetworkErrorLoggingHeader() {
  std::string value;
  if (!response_.headers->GetNormalizedHeader(
          NetworkErrorLoggingService::kHeaderName, &value)) {
    return;
  }

  NetworkErrorLoggingService* network_error_logging_service =
      session_->network_error_logging_service();
  if (!network_error_logging_service)
    return;

  // Don't accept NEL headers received via a proxy, because the IP address of
  // the destination server is not known.
  if (response_.WasFetchedViaProxy()) {
    return;
  }

  // Only accept NEL headers on HTTPS connections that have no certificate
  // errors.
  if (!response_.ssl_info.is_valid() ||
      IsCertStatusError(response_.ssl_info.cert_status)) {
    return;
  }

  if (remote_endpoint_.address().empty())
    return;

  network_error_logging_service->OnHeader(network_anonymization_key_,
                                          url::Origin::Create(url_),
                                          remote_endpoint_.address(), value);
}

void HttpNetworkTransaction::GenerateNetworkErrorLoggingReportIfError(int rv) {
  if (rv < 0 && rv != ERR_IO_PENDING)
    GenerateNetworkErrorLoggingReport(rv);
}

void HttpNetworkTransaction::GenerateNetworkErrorLoggingReport(int rv) {
  // |rv| should be a valid Error
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK_LE(rv, 0);

  if (network_error_logging_report_generated_)
    return;
  network_error_logging_report_generated_ = true;

  NetworkErrorLoggingService* service =
      session_->network_error_logging_service();
  if (!service)
    return;

  // Don't report on proxy auth challenges.
  if (response_.headers && response_.headers->response_code() ==
                               HTTP_PROXY_AUTHENTICATION_REQUIRED) {
    return;
  }

  // Don't generate NEL reports if we are behind a proxy, to avoid leaking
  // internal network details.
  if (response_.WasFetchedViaProxy()) {
    return;
  }

  // Ignore errors from non-HTTPS origins.
  if (!url_.SchemeIsCryptographic())
    return;

  NetworkErrorLoggingService::RequestDetails details;

  details.network_anonymization_key = network_anonymization_key_;
  details.uri = url_;
  if (!request_referrer_.empty())
    details.referrer = GURL(request_referrer_);
  details.user_agent = request_user_agent_;
  if (!remote_endpoint_.address().empty()) {
    details.server_ip = remote_endpoint_.address();
  } else if (!connection_attempts_.empty()) {
    // When we failed to connect to the server, `remote_endpoint_` is not set.
    // In such case, we use the last endpoint address of `connection_attempts_`
    // for the NEL report. This address information is important for the
    // downgrade step to protect against port scan attack.
    // https://www.w3.org/TR/network-error-logging/#generate-a-network-error-report
    details.server_ip = connection_attempts_.back().endpoint.address();
  } else {
    details.server_ip = IPAddress();
  }
  // HttpResponseHeaders::response_code() returns 0 if response code couldn't
  // be parsed, which is also how NEL represents the same.
  if (response_.headers) {
    details.status_code = response_.headers->response_code();
  } else {
    details.status_code = 0;
  }
  // If we got response headers, assume that the connection used HTTP/1.1
  // unless ALPN negotiation tells us otherwise (handled below).
  if (response_.was_alpn_negotiated) {
    details.protocol = response_.alpn_negotiated_protocol;
  } else {
    details.protocol = "http/1.1";
  }
  details.method = request_method_;
  details.elapsed_time = base::TimeTicks::Now() - start_timeticks_;
  details.type = static_cast<Error>(rv);
  details.reporting_upload_depth = request_reporting_upload_depth_;

  service->OnRequest(std::move(details));
}
#endif  // BUILDFLAG(ENABLE_REPORTING)

int HttpNetworkTransaction::HandleHttp11Required(int error) {
  DCHECK(error == ERR_HTTP_1_1_REQUIRED ||
         error == ERR_PROXY_HTTP_1_1_REQUIRED);

  http_1_1_was_required_ = true;

  // HttpServerProperties should have been updated, so when the request is sent
  // again, it will automatically use HTTP/1.1.
  ResetConnectionAndRequestForResend(RetryReason::kHttp11Required);
  return OK;
}

int HttpNetworkTransaction::HandleSSLClientAuthError(int error) {
  // Client certificate errors may come from either the origin server or the
  // proxy.
  //
  // Origin errors are handled here, while most proxy errors are handled in the
  // HttpStreamFactory and below, while handshaking with the proxy. However, in
  // TLS 1.2 with False Start, or TLS 1.3, client certificate errors are
  // reported immediately after the handshake. The error will then surface out
  // of the first Read() rather than Connect().
  //
  // If the request is tunneled (i.e. the origin is HTTPS), this first Read()
  // occurs while establishing the tunnel and HttpStreamFactory handles the
  // proxy error. However, if the request is not tunneled (i.e. the origin is
  // HTTP), this first Read() happens late and is ultimately surfaced out of
  // DoReadHeadersComplete(). This method will then be responsible for both
  // origin and proxy errors.
  //
  // See https://crbug.com/828965.
  if (error != ERR_SSL_PROTOCOL_ERROR && !IsClientCertificateError(error)) {
    return error;
  }

  bool is_server = !UsingHttpProxyWithoutTunnel();
  HostPortPair host_port_pair;
  // TODO(crbug.com/40284947): Remove check and return error when
  // multi-proxy chain.
  if (is_server) {
    host_port_pair = HostPortPair::FromURL(request_->url);
  } else {
    CHECK(proxy_info_.proxy_chain().is_single_proxy());
    host_port_pair = proxy_info_.proxy_chain().First().host_port_pair();
  }

  // Check that something in the proxy chain or endpoint are using HTTPS.
  if (DCHECK_IS_ON()) {
    bool server_using_tls = IsSecureRequest();
    bool proxy_using_tls = proxy_info_.AnyProxyInChain(
        [](const ProxyServer& s) { return s.is_secure_http_like(); });
    DCHECK(server_using_tls || proxy_using_tls);
  }

  if (session_->ssl_client_context()->ClearClientCertificate(host_port_pair)) {
    // The private key handle may have gone stale due to, e.g., the user
    // unplugging their smartcard. Operating systems do not provide reliable
    // notifications for this, so if the signature failed and the user was
    // not already prompted for certificate on this request, retry to ask
    // the user for a new one.
    //
    // TODO(davidben): There is no corresponding feature for proxy client
    // certificates. Ideally this would live at a lower level, common to both,
    // but |configured_client_cert_for_server_| is not accessible below the
    // socket pools.
    if (is_server && error == ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED &&
        !configured_client_cert_for_server_ && !HasExceededMaxRetries()) {
      retry_attempts_++;
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      ResetConnectionAndRequestForResend(
          RetryReason::kSslClientAuthSignatureFailed);
      return OK;
    }
  }
  return error;
}

// static
std::optional<HttpNetworkTransaction::RetryReason>
HttpNetworkTransaction::GetRetryReasonForIOError(int error) {
  switch (error) {
    case ERR_CONNECTION_RESET:
      return RetryReason::kConnectionReset;
    case ERR_CONNECTION_CLOSED:
      return RetryReason::kConnectionClosed;
    case ERR_CONNECTION_ABORTED:
      return RetryReason::kConnectionAborted;
    case ERR_SOCKET_NOT_CONNECTED:
      return RetryReason::kSocketNotConnected;
    case ERR_EMPTY_RESPONSE:
      return RetryReason::kEmptyResponse;
    case ERR_EARLY_DATA_REJECTED:
      return RetryReason::kEarlyDataRejected;
    case ERR_WRONG_VERSION_ON_EARLY_DATA:
      return RetryReason::kWrongVersionOnEarlyData;
    case ERR_HTTP2_PING_FAILED:
      return RetryReason::kHttp2PingFailed;
    case ERR_HTTP2_SERVER_REFUSED_STREAM:
      return RetryReason::kHttp2ServerRefusedStream;
    case ERR_QUIC_HANDSHAKE_FAILED:
      return RetryReason::kQuicHandshakeFailed;
    case ERR_QUIC_GOAWAY_REQUEST_CAN_BE_RETRIED:
      return RetryReason::kQuicGoawayRequestCanBeRetried;
    case ERR_QUIC_PROTOCOL_ERROR:
      return RetryReason::kQuicProtocolError;
  }
  return std::nullopt;
}

// This method determines whether it is safe to resend the request after an
// IO error. It should only be called in response to errors received before
// final set of response headers have been successfully parsed, that the
// transaction may need to be retried on.
// It should not be used in other cases, such as a Connect error.
int HttpNetworkTransaction::HandleIOError(int error) {
  // Because the peer may request renegotiation with client authentication at
  // any time, check and handle client authentication errors.
  error = HandleSSLClientAuthError(error);

#if BUILDFLAG(ENABLE_REPORTING)
  GenerateNetworkErrorLoggingReportIfError(error);
#endif  // BUILDFLAG(ENABLE_REPORTING)

  std::optional<HttpNetworkTransaction::RetryReason> retry_reason =
      GetRetryReasonForIOError(error);
  if (!retry_reason) {
    return error;
  }
  switch (*retry_reason) {
    // If we try to reuse a connection that the server is in the process of
    // closing, we may end up successfully writing out our request (or a
    // portion of our request) only to find a connection error when we try to
    // read from (or finish writing to) the socket.
    case RetryReason::kConnectionReset:
    case RetryReason::kConnectionClosed:
    case RetryReason::kConnectionAborted:
    // There can be a race between the socket pool checking checking whether a
    // socket is still connected, receiving the FIN, and sending/reading data
    // on a reused socket.  If we receive the FIN between the connectedness
    // check and writing/reading from the socket, we may first learn the socket
    // is disconnected when we get a ERR_SOCKET_NOT_CONNECTED.  This will most
    // likely happen when trying to retrieve its IP address.
    // See http://crbug.com/105824 for more details.
    case RetryReason::kSocketNotConnected:
    // If a socket is closed on its initial request, HttpStreamParser returns
    // ERR_EMPTY_RESPONSE. This may still be close/reuse race if the socket was
    // preconnected but failed to be used before the server timed it out.
    case RetryReason::kEmptyResponse:
      if (ShouldResendRequest()) {
        net_log_.AddEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        ResetConnectionAndRequestForResend(*retry_reason);
        error = OK;
      }
      break;
    case RetryReason::kEarlyDataRejected:
    case RetryReason::kWrongVersionOnEarlyData:
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      // Disable early data on a reset.
      can_send_early_data_ = false;
      ResetConnectionAndRequestForResend(*retry_reason);
      error = OK;
      break;
    case RetryReason::kHttp2PingFailed:
    case RetryReason::kHttp2ServerRefusedStream:
    case RetryReason::kQuicHandshakeFailed:
    case RetryReason::kQuicGoawayRequestCanBeRetried:
      if (HasExceededMaxRetries())
        break;
      net_log_.AddEventWithNetErrorCode(
          NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
      retry_attempts_++;
      ResetConnectionAndRequestForResend(*retry_reason);
      error = OK;
      break;
    case RetryReason::kQuicProtocolError:
      if (HasExceededMaxRetries() || GetResponseHeaders() != nullptr ||
          !stream_->GetAlternativeService(&retried_alternative_service_)) {
        // If the response headers have already been received and passed up
        // then the request can not be retried. Also, if there was no
        // alternative service used for this request, then there is no
        // alternative service to be disabled.
        break;
      }

      if (session_->http_server_properties()->IsAlternativeServiceBroken(
              retried_alternative_service_, network_anonymization_key_)) {
        // If the alternative service was marked as broken while the request
        // was in flight, retry the request which will not use the broken
        // alternative service.
        net_log_.AddEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        retry_attempts_++;
        ResetConnectionAndRequestForResend(*retry_reason);
        error = OK;
      } else if (session_->context()
                     .quic_context->params()
                     ->retry_without_alt_svc_on_quic_errors) {
        // Disable alternative services for this request and retry it. If the
        // retry succeeds, then the alternative service will be marked as
        // broken then.
        enable_alternative_services_ = false;
        net_log_.AddEventWithNetErrorCode(
            NetLogEventType::HTTP_TRANSACTION_RESTART_AFTER_ERROR, error);
        retry_attempts_++;
        ResetConnectionAndRequestForResend(*retry_reason);
        error = OK;
      }
      break;

    // The following reasons are not covered here.
    case RetryReason::kHttpRequestTimeout:
    case RetryReason::kHttpMisdirectedRequest:
    case RetryReason::kHttp11Required:
    case RetryReason::kSslClientAuthSignatureFailed:
      NOTREACHED_IN_MIGRATION();
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
  read_buf_ = nullptr;
  read_buf_len_ = 0;
  headers_valid_ = false;
  request_headers_.Clear();
  response_ = HttpResponseInfo();
  SetProxyInfoInResponse(proxy_info_, &response_);
  establishing_tunnel_ = false;
  remote_endpoint_ = IPEndPoint();
  net_error_details_.quic_broken = false;
  net_error_details_.quic_connection_error = quic::QUIC_NO_ERROR;
#if BUILDFLAG(ENABLE_REPORTING)
  network_error_logging_report_generated_ = false;
  start_timeticks_ = base::TimeTicks::Now();
#endif  // BUILDFLAG(ENABLE_REPORTING)
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
  bool has_received_headers = GetResponseHeaders() != nullptr;

  // NOTE: we resend a request only if we reused a keep-alive connection.
  // This automatically prevents an infinite resend loop because we'll run
  // out of the cached keep-alive connections eventually.
  return connection_is_proven && !has_received_headers;
}

bool HttpNetworkTransaction::HasExceededMaxRetries() const {
  return (retry_attempts_ >= kMaxRetryAttempts);
}

bool HttpNetworkTransaction::CheckMaxRestarts() {
  num_restarts_++;
  return num_restarts_ < kMaxRestarts;
}

void HttpNetworkTransaction::ResetConnectionAndRequestForResend(
    RetryReason retry_reason) {
  // TODO:(crbug.com/1495705): Remove this CHECK after fixing the bug.
  CHECK(request_);
  base::UmaHistogramEnumeration(
      IsGoogleHostWithAlpnH3(url_.host())
          ? "Net.NetworkTransactionH3SupportedGoogleHost.RetryReason"
          : "Net.NetworkTransaction.RetryReason",
      retry_reason);

  if (stream_.get()) {
    stream_->Close(true);
    CacheNetErrorDetailsAndResetStream();
  }

  // We need to clear request_headers_ because it contains the real request
  // headers, but we may need to resend the CONNECT request first to recreate
  // the SSL tunnel.
  request_headers_.Clear();
  next_state_ = STATE_CREATE_STREAM;  // Resend the request.

#if BUILDFLAG(ENABLE_REPORTING)
  // Reset for new request.
  network_error_logging_report_generated_ = false;
  start_timeticks_ = base::TimeTicks::Now();
#endif  // BUILDFLAG(ENABLE_REPORTING)

  ResetStateForRestart();
}

bool HttpNetworkTransaction::ShouldApplyProxyAuth() const {
  // TODO(crbug.com/40284947): Update to handle multi-proxy chains.
  if (proxy_info_.proxy_chain().is_multi_proxy()) {
    return false;
  }
  return UsingHttpProxyWithoutTunnel();
}

bool HttpNetworkTransaction::ShouldApplyServerAuth() const {
  return request_->privacy_mode == PRIVACY_MODE_DISABLED;
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
      headers, response_.ssl_info, !ShouldApplyServerAuth(), false, net_log_);
  if (auth_controllers_[target]->HaveAuthHandler())
    pending_auth_target_ = target;

  auth_controllers_[target]->TakeAuthInfo(&response_.auth_challenge);

  return rv;
}

bool HttpNetworkTransaction::HaveAuth(HttpAuth::Target target) const {
  return auth_controllers_[target].get() &&
      auth_controllers_[target]->HaveAuth();
}

GURL HttpNetworkTransaction::AuthURL(HttpAuth::Target target) const {
  switch (target) {
    case HttpAuth::AUTH_PROXY: {
      // TODO(crbug.com/40284947): Update to handle multi-proxy chain.
      CHECK(proxy_info_.proxy_chain().is_single_proxy());
      if (!proxy_info_.proxy_chain().IsValid() ||
          proxy_info_.proxy_chain().is_direct()) {
        return GURL();  // There is no proxy chain.
      }
      // TODO(crbug.com/40704785): Mapping proxy addresses to
      // URLs is a lossy conversion, shouldn't do this.
      auto& proxy_server = proxy_info_.proxy_chain().First();
      const char* scheme =
          proxy_server.is_secure_http_like() ? "https://" : "http://";
      return GURL(scheme + proxy_server.host_port_pair().ToString());
    }
    case HttpAuth::AUTH_SERVER:
      if (ForWebSocketHandshake()) {
        return ChangeWebSocketSchemeToHttpScheme(request_->url);
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

  std::set<std::string> allowed_encodings;
  if (!HttpUtil::ParseAcceptEncoding(
          request_headers_.GetHeader(HttpRequestHeaders::kAcceptEncoding)
              .value_or(std::string()),
          &allowed_encodings)) {
    return false;
  }

  std::string content_encoding;
  headers->GetNormalizedHeader("Content-Encoding", &content_encoding);
  std::set<std::string> used_encodings;
  if (!HttpUtil::ParseContentEncoding(content_encoding, &used_encodings))
    return false;

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
      result = false;
      break;
    }
  }

  // Temporary workaround for http://crbug.com/714514
  if (headers->IsRedirect(nullptr)) {
    return true;
  }

  return result;
}

// static
void HttpNetworkTransaction::SetProxyInfoInResponse(
    const ProxyInfo& proxy_info,
    HttpResponseInfo* response_info) {
  response_info->was_mdl_match = proxy_info.is_mdl_match();
  if (proxy_info.is_empty()) {
    response_info->proxy_chain = ProxyChain();
  } else {
    response_info->proxy_chain = proxy_info.proxy_chain();
  }
}

}  // namespace net

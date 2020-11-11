// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_job.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/port_util.h"
#include "net/base/proxy_delegate.h"
#include "net/base/trace_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/proxy_fallback.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/quic/bidirectional_stream_quic_impl.h"
#include "net/quic/quic_http_stream.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/connect_job.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/bidirectional_stream_spdy_impl.h"
#include "net/spdy/http2_push_promise_index.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Experiment to preconnect only one connection if HttpServerProperties is
// not supported or initialized.
const base::Feature kLimitEarlyPreconnectsExperiment{
    "LimitEarlyPreconnects", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Returns parameters associated with the start of a HTTP stream job.
base::Value NetLogHttpStreamJobParams(const NetLogSource& source,
                                      const GURL& original_url,
                                      const GURL& url,
                                      bool expect_spdy,
                                      bool using_quic,
                                      RequestPriority priority) {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (source.IsValid())
    source.AddToEventParameters(&dict);
  dict.SetStringKey("original_url", original_url.GetOrigin().spec());
  dict.SetStringKey("url", url.GetOrigin().spec());
  dict.SetBoolKey("expect_spdy", expect_spdy);
  dict.SetBoolKey("using_quic", using_quic);
  dict.SetStringKey("priority", RequestPriorityToString(priority));
  return dict;
}

// Returns parameters associated with the Proto (with NPN negotiation) of a HTTP
// stream.
base::Value NetLogHttpStreamProtoParams(NextProto negotiated_protocol) {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetStringKey("proto", NextProtoToString(negotiated_protocol));
  return dict;
}

HttpStreamFactory::Job::Job(Delegate* delegate,
                            JobType job_type,
                            HttpNetworkSession* session,
                            const HttpRequestInfo& request_info,
                            RequestPriority priority,
                            const ProxyInfo& proxy_info,
                            const SSLConfig& server_ssl_config,
                            const SSLConfig& proxy_ssl_config,
                            HostPortPair destination,
                            GURL origin_url,
                            NextProto alternative_protocol,
                            quic::ParsedQuicVersion quic_version,
                            bool is_websocket,
                            bool enable_ip_based_pooling,
                            NetLog* net_log)
    : request_info_(request_info),
      priority_(priority),
      proxy_info_(proxy_info),
      server_ssl_config_(server_ssl_config),
      proxy_ssl_config_(proxy_ssl_config),
      net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::HTTP_STREAM_JOB)),
      io_callback_(
          base::BindRepeating(&Job::OnIOComplete, base::Unretained(this))),
      connection_(new ClientSocketHandle),
      session_(session),
      next_state_(STATE_NONE),
      destination_(destination),
      origin_url_(origin_url),
      is_websocket_(is_websocket),
      try_websocket_over_http2_(is_websocket_ &&
                                origin_url_.SchemeIs(url::kWssScheme) &&
                                proxy_info_.is_direct() &&
                                session_->params().enable_websocket_over_http2),
      // Don't use IP connection pooling for HTTP over HTTPS proxies. It doesn't
      // get us much, and testing it is more effort than its worth.
      enable_ip_based_pooling_(
          enable_ip_based_pooling &&
          !(proxy_info_.proxy_server().is_secure_http_like() &&
            origin_url_.SchemeIs(url::kHttpScheme))),
      delegate_(delegate),
      job_type_(job_type),
      using_ssl_(origin_url_.SchemeIs(url::kHttpsScheme) ||
                 origin_url_.SchemeIs(url::kWssScheme)),
      using_quic_(alternative_protocol == kProtoQUIC ||
                  (ShouldForceQuic(session,
                                   destination,
                                   origin_url,
                                   proxy_info,
                                   using_ssl_))),
      quic_version_(quic_version),
      expect_spdy_(alternative_protocol == kProtoHTTP2 && !using_quic_),
      using_spdy_(false),
      should_reconsider_proxy_(false),
      quic_request_(session_->quic_stream_factory()),
      expect_on_quic_host_resolution_(false),
      using_existing_quic_session_(false),
      establishing_tunnel_(false),
      was_alpn_negotiated_(false),
      negotiated_protocol_(kProtoUnknown),
      num_streams_(0),
      pushed_stream_id_(kNoPushedStreamFound),
      spdy_session_direct_(
          !(proxy_info.is_https() && origin_url_.SchemeIs(url::kHttpScheme))),
      spdy_session_key_(
          using_quic_ ? SpdySessionKey()
                      : GetSpdySessionKey(spdy_session_direct_,
                                          proxy_info_.proxy_server(),
                                          origin_url_,
                                          request_info_.privacy_mode,
                                          request_info_.socket_tag,
                                          request_info_.network_isolation_key,
                                          request_info_.disable_secure_dns)),
      stream_type_(HttpStreamRequest::BIDIRECTIONAL_STREAM),
      init_connection_already_resumed_(false) {
  // QUIC can only be spoken to servers, never to proxies.
  if (alternative_protocol == kProtoQUIC)
    DCHECK(proxy_info_.is_direct());

  // The Job is forced to use QUIC without a designated version, try the
  // preferred QUIC version that is supported by default.
  if (quic_version_ == quic::ParsedQuicVersion::Unsupported() &&
      ShouldForceQuic(session, destination, origin_url, proxy_info,
                      using_ssl_)) {
    quic_version_ =
        session->context().quic_context->params()->supported_versions[0];
  }

  if (using_quic_)
    DCHECK_NE(quic_version_, quic::ParsedQuicVersion::Unsupported());

  DCHECK(session);
  if (alternative_protocol != kProtoUnknown) {
    // If the alternative service protocol is specified, then the job type must
    // be either ALTERNATIVE or PRECONNECT.
    DCHECK(job_type_ == ALTERNATIVE || job_type_ == PRECONNECT);
  }

  if (expect_spdy_) {
    DCHECK(origin_url_.SchemeIs(url::kHttpsScheme));
  }
  if (using_quic_) {
    DCHECK(session_->IsQuicEnabled());
  }
  if (job_type_ == PRECONNECT || is_websocket_) {
    DCHECK(request_info_.socket_tag == SocketTag());
  }
  if (is_websocket_) {
    DCHECK(origin_url_.SchemeIsWSOrWSS());
  } else {
    DCHECK(!origin_url_.SchemeIsWSOrWSS());
  }
}

HttpStreamFactory::Job::~Job() {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB);

  // When we're in a partially constructed state, waiting for the user to
  // provide certificate handling information or authentication, we can't reuse
  // this stream at all.
  if (next_state_ == STATE_WAITING_USER_ACTION) {
    connection_->socket()->Disconnect();
    connection_.reset();
  }

  // The stream could be in a partial state.  It is not reusable.
  if (stream_.get() && next_state_ != STATE_DONE)
    stream_->Close(true /* not reusable */);
}

void HttpStreamFactory::Job::Start(HttpStreamRequest::StreamType stream_type) {
  stream_type_ = stream_type;
  StartInternal();
}

int HttpStreamFactory::Job::Preconnect(int num_streams) {
  DCHECK_GT(num_streams, 0);
  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  DCHECK(http_server_properties);
  // Preconnect one connection if either of the following is true:
  //   (1) kLimitEarlyPreconnectsStreamExperiment is turned on,
  //   HttpServerProperties is not initialized, and url scheme is cryptographic.
  //   (2) The server supports H2 or QUIC.
  bool connect_one_stream =
      base::FeatureList::IsEnabled(kLimitEarlyPreconnectsExperiment) &&
      !http_server_properties->IsInitialized() &&
      request_info_.url.SchemeIsCryptographic();
  if (connect_one_stream || http_server_properties->SupportsRequestPriority(
                                url::SchemeHostPort(request_info_.url),
                                request_info_.network_isolation_key)) {
    num_streams_ = 1;
  } else {
    num_streams_ = num_streams;
  }
  return StartInternal();
}

int HttpStreamFactory::Job::RestartTunnelWithProxyAuth() {
  DCHECK(establishing_tunnel_);
  DCHECK(restart_with_auth_callback_);

  std::move(restart_with_auth_callback_).Run();
  return ERR_IO_PENDING;
}

LoadState HttpStreamFactory::Job::GetLoadState() const {
  switch (next_state_) {
    case STATE_INIT_CONNECTION_COMPLETE:
    case STATE_CREATE_STREAM_COMPLETE:
      return using_quic_ ? LOAD_STATE_CONNECTING : connection_->GetLoadState();
    default:
      return LOAD_STATE_IDLE;
  }
}

void HttpStreamFactory::Job::Resume() {
  DCHECK_EQ(job_type_, MAIN);
  DCHECK_EQ(next_state_, STATE_WAIT_COMPLETE);
  OnIOComplete(OK);
}

void HttpStreamFactory::Job::Orphan() {
  DCHECK_EQ(job_type_, ALTERNATIVE);
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_JOB_ORPHANED);

  // Watching for SPDY sessions isn't supported on orphaned jobs.
  // TODO(mmenke): Fix that.
  spdy_session_request_.reset();
}

void HttpStreamFactory::Job::SetPriority(RequestPriority priority) {
  priority_ = priority;
  // Ownership of |connection_| is passed to the newly created stream
  // or H2 session in DoCreateStream(), and the consumer is not
  // notified immediately, so this call may occur when |connection_|
  // is null.
  //
  // Note that streams are created without a priority associated with them,
  // and it is up to the consumer to set their priority via
  // HttpStream::InitializeStream().  So there is no need for this code
  // to propagate priority changes to the newly created stream.
  if (connection_ && connection_->is_initialized())
    connection_->SetPriority(priority);
  // TODO(akalin): Maybe Propagate this to the preconnect state.
}

bool HttpStreamFactory::Job::was_alpn_negotiated() const {
  return was_alpn_negotiated_;
}

NextProto HttpStreamFactory::Job::negotiated_protocol() const {
  return negotiated_protocol_;
}

bool HttpStreamFactory::Job::using_spdy() const {
  return using_spdy_;
}

size_t HttpStreamFactory::Job::EstimateMemoryUsage() const {
  StreamSocket::SocketMemoryStats stats;
  if (connection_)
    connection_->DumpMemoryStats(&stats);
  return stats.total_size;
}

const SSLConfig& HttpStreamFactory::Job::server_ssl_config() const {
  return server_ssl_config_;
}

const SSLConfig& HttpStreamFactory::Job::proxy_ssl_config() const {
  return proxy_ssl_config_;
}

const ProxyInfo& HttpStreamFactory::Job::proxy_info() const {
  return proxy_info_;
}

ResolveErrorInfo HttpStreamFactory::Job::resolve_error_info() const {
  return resolve_error_info_;
}

void HttpStreamFactory::Job::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(using_ssl_);
  DCHECK(!establishing_tunnel_);
  DCHECK(connection_.get() && connection_->socket());
  connection_->socket()->GetSSLInfo(ssl_info);
}

// static
bool HttpStreamFactory::Job::ShouldForceQuic(HttpNetworkSession* session,
                                             const HostPortPair& destination,
                                             const GURL& origin_url,
                                             const ProxyInfo& proxy_info,
                                             bool using_ssl) {
  if (!session->IsQuicEnabled())
    return false;
  // If this is going through a QUIC proxy, only force QUIC for insecure
  // requests. If the request is secure, a tunnel will be needed, and those are
  // handled by the socket pools, using an HttpProxyConnectJob.
  if (proxy_info.is_quic())
    return !using_ssl;
  const QuicParams* quic_params = session->context().quic_context->params();
  return (base::Contains(quic_params->origins_to_force_quic_on,
                         HostPortPair()) ||
          base::Contains(quic_params->origins_to_force_quic_on, destination)) &&
         proxy_info.is_direct() && origin_url.SchemeIs(url::kHttpsScheme);
}

// static
SpdySessionKey HttpStreamFactory::Job::GetSpdySessionKey(
    bool spdy_session_direct,
    const ProxyServer& proxy_server,
    const GURL& origin_url,
    PrivacyMode privacy_mode,
    const SocketTag& socket_tag,
    const NetworkIsolationKey& network_isolation_key,
    bool disable_secure_dns) {
  // In the case that we're using an HTTPS proxy for an HTTP url, look for a
  // HTTP/2 proxy session *to* the proxy, instead of to the  origin server.
  if (!spdy_session_direct) {
    return SpdySessionKey(proxy_server.host_port_pair(), ProxyServer::Direct(),
                          PRIVACY_MODE_DISABLED,
                          SpdySessionKey::IsProxySession::kTrue, socket_tag,
                          network_isolation_key, disable_secure_dns);
  }
  return SpdySessionKey(HostPortPair::FromURL(origin_url), proxy_server,
                        privacy_mode, SpdySessionKey::IsProxySession::kFalse,
                        socket_tag, network_isolation_key, disable_secure_dns);
}

bool HttpStreamFactory::Job::CanUseExistingSpdySession() const {
  DCHECK(!using_quic_);

  if (proxy_info_.is_direct() &&
      session_->http_server_properties()->RequiresHTTP11(
          url::SchemeHostPort(request_info_.url),
          request_info_.network_isolation_key)) {
    return false;
  }

  if (is_websocket_)
    return try_websocket_over_http2_;

  DCHECK(origin_url_.SchemeIsHTTPOrHTTPS());

  // We need to make sure that if a HTTP/2 session was created for
  // https://somehost/ then we do not use that session for http://somehost:443/.
  // The only time we can use an existing session is if the request URL is
  // https (the normal case) or if we are connecting to a HTTP/2 proxy.
  // https://crbug.com/133176
  return origin_url_.SchemeIs(url::kHttpsScheme) ||
         proxy_info_.proxy_server().is_https();
}

void HttpStreamFactory::Job::OnStreamReadyCallback() {
  DCHECK(stream_.get());
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK(!is_websocket_ || try_websocket_over_http2_);

  MaybeCopyConnectionAttemptsFromSocketOrHandle();

  delegate_->OnStreamReady(this, server_ssl_config_);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnWebSocketHandshakeStreamReadyCallback() {
  DCHECK(websocket_stream_);
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK(is_websocket_);

  MaybeCopyConnectionAttemptsFromSocketOrHandle();

  delegate_->OnWebSocketHandshakeStreamReady(
      this, server_ssl_config_, proxy_info_, std::move(websocket_stream_));
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnBidirectionalStreamImplReadyCallback() {
  DCHECK(bidirectional_stream_impl_);

  MaybeCopyConnectionAttemptsFromSocketOrHandle();

  delegate_->OnBidirectionalStreamImplReady(this, server_ssl_config_,
                                            proxy_info_);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnStreamFailedCallback(int result) {
  DCHECK_NE(job_type_, PRECONNECT);

  MaybeCopyConnectionAttemptsFromSocketOrHandle();

  delegate_->OnStreamFailed(this, result, server_ssl_config_);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnCertificateErrorCallback(
    int result,
    const SSLInfo& ssl_info) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK(!spdy_session_request_);

  MaybeCopyConnectionAttemptsFromSocketOrHandle();

  delegate_->OnCertificateError(this, result, server_ssl_config_, ssl_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnNeedsProxyAuthCallback(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK(establishing_tunnel_);
  DCHECK(!restart_with_auth_callback_);

  restart_with_auth_callback_ = std::move(restart_with_auth_callback);

  // This is called out of band, so need to abort the SpdySessionRequest to
  // prevent being passed a new session while waiting on proxy auth credentials.
  spdy_session_request_.reset();

  delegate_->OnNeedsProxyAuth(this, response, server_ssl_config_, proxy_info_,
                              auth_controller);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnNeedsClientAuthCallback(
    SSLCertRequestInfo* cert_info) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK(!spdy_session_request_);

  delegate_->OnNeedsClientAuth(this, server_ssl_config_, cert_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnPreconnectsComplete() {
  delegate_->OnPreconnectsComplete(this);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnIOComplete(int result) {
  TRACE_EVENT0(NetTracingCategory(), "HttpStreamFactory::Job::OnIOComplete");
  RunLoop(result);
}

void HttpStreamFactory::Job::RunLoop(int result) {
  TRACE_EVENT0(NetTracingCategory(), "HttpStreamFactory::Job::RunLoop");
  result = DoLoop(result);

  if (result == ERR_IO_PENDING)
    return;

  // Stop watching for new SpdySessions, to avoid receiving a new SPDY session
  // while doing anything other than waiting to establish a connection.
  spdy_session_request_.reset();

  if (job_type_ == PRECONNECT) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamFactory::Job::OnPreconnectsComplete,
                       ptr_factory_.GetWeakPtr()));
    return;
  }

  if (IsCertificateError(result)) {
    // Retrieve SSL information from the socket.
    SSLInfo ssl_info;
    GetSSLInfo(&ssl_info);

    next_state_ = STATE_WAITING_USER_ACTION;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamFactory::Job::OnCertificateErrorCallback,
                       ptr_factory_.GetWeakPtr(), result, ssl_info));
    return;
  }

  switch (result) {
    case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &Job::OnNeedsClientAuthCallback, ptr_factory_.GetWeakPtr(),
              base::RetainedRef(connection_->ssl_cert_request_info())));
      return;

    case OK:
      next_state_ = STATE_DONE;
      if (is_websocket_) {
        DCHECK(websocket_stream_);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&Job::OnWebSocketHandshakeStreamReadyCallback,
                           ptr_factory_.GetWeakPtr()));
      } else if (stream_type_ == HttpStreamRequest::BIDIRECTIONAL_STREAM) {
        if (!bidirectional_stream_impl_) {
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(&Job::OnStreamFailedCallback,
                                        ptr_factory_.GetWeakPtr(), ERR_FAILED));
        } else {
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::BindOnce(&Job::OnBidirectionalStreamImplReadyCallback,
                             ptr_factory_.GetWeakPtr()));
        }
      } else {
        DCHECK(stream_.get());
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&Job::OnStreamReadyCallback,
                                      ptr_factory_.GetWeakPtr()));
      }
      return;

    default:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&Job::OnStreamFailedCallback,
                                    ptr_factory_.GetWeakPtr(), result));
      return;
  }
}

int HttpStreamFactory::Job::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);
  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_START:
        DCHECK_EQ(OK, rv);
        rv = DoStart();
        break;
      case STATE_WAIT:
        DCHECK_EQ(OK, rv);
        rv = DoWait();
        break;
      case STATE_WAIT_COMPLETE:
        rv = DoWaitComplete(rv);
        break;
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_WAITING_USER_ACTION:
        rv = DoWaitingUserAction(rv);
        break;
      case STATE_CREATE_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoCreateStream();
        break;
      case STATE_CREATE_STREAM_COMPLETE:
        rv = DoCreateStreamComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int HttpStreamFactory::Job::StartInternal() {
  CHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_START;
  RunLoop(OK);
  return ERR_IO_PENDING;
}

int HttpStreamFactory::Job::DoStart() {
  const NetLogWithSource* net_log = delegate_->GetNetLog();

  if (net_log) {
    net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_JOB, [&] {
      return NetLogHttpStreamJobParams(net_log->source(), request_info_.url,
                                       origin_url_, expect_spdy_, using_quic_,
                                       priority_);
    });
    net_log->AddEventReferencingSource(
        NetLogEventType::HTTP_STREAM_REQUEST_STARTED_JOB, net_log_.source());
  }

  // Don't connect to restricted ports.
  if (!IsPortAllowedForScheme(destination_.port(),
                              request_info_.url.scheme_piece())) {
    return ERR_UNSAFE_PORT;
  }

  if (!session_->params().enable_quic_proxies_for_https_urls &&
      proxy_info_.is_quic() && request_info_.url.SchemeIsCryptographic()) {
    return ERR_NOT_IMPLEMENTED;
  }

  next_state_ = STATE_WAIT;
  return OK;
}

int HttpStreamFactory::Job::DoWait() {
  next_state_ = STATE_WAIT_COMPLETE;
  bool should_wait = delegate_->ShouldWait(this);
  net_log_.AddEntryWithBoolParams(NetLogEventType::HTTP_STREAM_JOB_WAITING,
                                  NetLogEventPhase::BEGIN, "should_wait",
                                  should_wait);
  if (should_wait)
    return ERR_IO_PENDING;

  return OK;
}

int HttpStreamFactory::Job::DoWaitComplete(int result) {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB_WAITING);
  DCHECK_EQ(OK, result);
  next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

void HttpStreamFactory::Job::ResumeInitConnection() {
  if (init_connection_already_resumed_)
    return;
  DCHECK_EQ(next_state_, STATE_INIT_CONNECTION);
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_JOB_RESUME_INIT_CONNECTION);
  init_connection_already_resumed_ = true;
  OnIOComplete(OK);
}

int HttpStreamFactory::Job::DoInitConnection() {
  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_JOB_INIT_CONNECTION);
  int result = DoInitConnectionImpl();
  if (!expect_on_quic_host_resolution_) {
    delegate_->OnConnectionInitialized(this, result);
  }
  return result;
}

int HttpStreamFactory::Job::DoInitConnectionImpl() {
  DCHECK(!connection_->is_initialized());

  if (using_quic_ && !proxy_info_.is_quic() && !proxy_info_.is_direct()) {
    // QUIC can not be spoken to non-QUIC proxies.  This error should not be
    // user visible, because the non-alternative Job should be resumed.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  DCHECK(proxy_info_.proxy_server().is_valid());
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  if (proxy_info_.is_secure_http_like()) {
    // Disable network fetches for HTTPS proxies, since the network requests
    // are probably going to need to go through the proxy too.
    proxy_ssl_config_.disable_cert_verification_network_fetches = true;
  }
  if (using_ssl_) {
    // Prior to HTTP/2 and SPDY, some servers use TLS renegotiation to request
    // TLS client authentication after the HTTP request was sent. Allow
    // renegotiation for only those connections.
    //
    // Note that this does NOT implement the provision in
    // https://http2.github.io/http2-spec/#rfc.section.9.2.1 which allows the
    // server to request a renegotiation immediately before sending the
    // connection preface as waiting for the preface would cost the round trip
    // that False Start otherwise saves.
    server_ssl_config_.renego_allowed_default = true;
    server_ssl_config_.renego_allowed_for_protos.push_back(kProtoHTTP11);
  }

  if (using_quic_)
    return DoInitConnectionImplQuic();

  // Check first if there is a pushed stream matching the request, or an HTTP/2
  // connection this request can pool to.  If so, then go straight to using
  // that.
  if (CanUseExistingSpdySession()) {
    if (!is_websocket_) {
      session_->spdy_session_pool()->push_promise_index()->ClaimPushedStream(
          spdy_session_key_, origin_url_, request_info_,
          &existing_spdy_session_, &pushed_stream_id_);
    }
    if (!existing_spdy_session_) {
      if (!spdy_session_request_) {
        // If not currently watching for an H2 session, use
        // SpdySessionPool::RequestSession() to check for a session, and start
        // watching for one.
        bool should_throttle_connect = ShouldThrottleConnectForSpdy();
        base::RepeatingClosure resume_callback =
            should_throttle_connect
                ? base::BindRepeating(
                      &HttpStreamFactory::Job::ResumeInitConnection,
                      ptr_factory_.GetWeakPtr())
                : base::RepeatingClosure();

        bool is_blocking_request_for_session;
        existing_spdy_session_ = session_->spdy_session_pool()->RequestSession(
            spdy_session_key_, enable_ip_based_pooling_, is_websocket_,
            net_log_, resume_callback, this, &spdy_session_request_,
            &is_blocking_request_for_session);
        if (!existing_spdy_session_ && should_throttle_connect &&
            !is_blocking_request_for_session) {
          net_log_.AddEvent(NetLogEventType::HTTP_STREAM_JOB_THROTTLED);
          next_state_ = STATE_INIT_CONNECTION;
          base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
              FROM_HERE, resume_callback,
              base::TimeDelta::FromMilliseconds(kHTTP2ThrottleMs));
          return ERR_IO_PENDING;
        }
      } else if (enable_ip_based_pooling_) {
        // If already watching for an H2 session, still need to check for an
        // existing connection that can be reused through IP pooling, as those
        // don't post session available notifications.
        //
        // TODO(mmenke):  Make sessions created through IP pooling invoke the
        // callback.
        existing_spdy_session_ =
            session_->spdy_session_pool()->FindAvailableSession(
                spdy_session_key_, enable_ip_based_pooling_, is_websocket_,
                net_log_);
      }
    }
    if (existing_spdy_session_) {
      // Stop watching for SpdySessions.
      spdy_session_request_.reset();

      // If we're preconnecting, but we already have a SpdySession, we don't
      // actually need to preconnect any sockets, so we're done.
      if (job_type_ == PRECONNECT)
        return OK;
      using_spdy_ = true;
      next_state_ = STATE_CREATE_STREAM;
      return OK;
    }
  }

  if (proxy_info_.is_http_like())
    establishing_tunnel_ = using_ssl_;

  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  if (http_server_properties) {
    http_server_properties->MaybeForceHTTP11(
        url::SchemeHostPort(request_info_.url),
        request_info_.network_isolation_key, &server_ssl_config_);
    if (proxy_info_.is_https()) {
      http_server_properties->MaybeForceHTTP11(
          url::SchemeHostPort(
              url::kHttpsScheme,
              proxy_info_.proxy_server().host_port_pair().host(),
              proxy_info_.proxy_server().host_port_pair().port()),
          request_info_.network_isolation_key, &proxy_ssl_config_);
    }
  }

  if (job_type_ == PRECONNECT) {
    DCHECK(!is_websocket_);
    DCHECK(request_info_.socket_tag == SocketTag());
    return PreconnectSocketsForHttpRequest(
        GetSocketGroup(), destination_, request_info_.load_flags, priority_,
        session_, proxy_info_, server_ssl_config_, proxy_ssl_config_,
        request_info_.privacy_mode, request_info_.network_isolation_key,
        request_info_.disable_secure_dns, net_log_, num_streams_);
  }

  ClientSocketPool::ProxyAuthCallback proxy_auth_callback =
      base::BindRepeating(&HttpStreamFactory::Job::OnNeedsProxyAuthCallback,
                          base::Unretained(this));
  if (is_websocket_) {
    DCHECK(request_info_.socket_tag == SocketTag());
    DCHECK(!request_info_.disable_secure_dns);
    SSLConfig websocket_server_ssl_config = server_ssl_config_;
    websocket_server_ssl_config.alpn_protos.clear();
    return InitSocketHandleForWebSocketRequest(
        GetSocketGroup(), destination_, request_info_.load_flags, priority_,
        session_, proxy_info_, websocket_server_ssl_config, proxy_ssl_config_,
        request_info_.privacy_mode, request_info_.network_isolation_key,
        net_log_, connection_.get(), io_callback_, proxy_auth_callback);
  }

  return InitSocketHandleForHttpRequest(
      GetSocketGroup(), destination_, request_info_.load_flags, priority_,
      session_, proxy_info_, server_ssl_config_, proxy_ssl_config_,
      request_info_.privacy_mode, request_info_.network_isolation_key,
      request_info_.disable_secure_dns, request_info_.socket_tag, net_log_,
      connection_.get(), io_callback_, proxy_auth_callback);
}

int HttpStreamFactory::Job::DoInitConnectionImplQuic() {
  HostPortPair destination;
  SSLConfig* ssl_config;
  GURL url(request_info_.url);
  if (proxy_info_.is_quic()) {
    // A proxy's certificate is expected to be valid for the proxy hostname.
    destination = proxy_info_.proxy_server().host_port_pair();
    ssl_config = &proxy_ssl_config_;
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpsScheme);
    replacements.SetHostStr(destination.host());
    const std::string new_port = base::NumberToString(destination.port());
    replacements.SetPortStr(new_port);
    replacements.ClearUsername();
    replacements.ClearPassword();
    replacements.ClearPath();
    replacements.ClearQuery();
    replacements.ClearRef();
    url = url.ReplaceComponents(replacements);
  } else {
    DCHECK(using_ssl_);
    // The certificate of a QUIC alternative server is expected to be valid
    // for the origin of the request (in addition to being valid for the
    // server itself).
    destination = destination_;
    ssl_config = &server_ssl_config_;
  }
  int rv = quic_request_.Request(
      destination, quic_version_, request_info_.privacy_mode, priority_,
      request_info_.socket_tag, request_info_.network_isolation_key,
      request_info_.disable_secure_dns, ssl_config->GetCertVerifyFlags(), url,
      net_log_, &net_error_details_,
      base::BindOnce(&Job::OnFailedOnDefaultNetwork, ptr_factory_.GetWeakPtr()),
      io_callback_);
  if (rv == OK) {
    using_existing_quic_session_ = true;
  } else if (rv == ERR_IO_PENDING) {
    // There's no available QUIC session. Inform the delegate how long to
    // delay the main job.
    delegate_->MaybeSetWaitTimeForMainJob(
        quic_request_.GetTimeDelayForWaitingJob());
    expect_on_quic_host_resolution_ = quic_request_.WaitForHostResolution(
        base::BindOnce(&Job::OnQuicHostResolution, base::Unretained(this)));
  }
  return rv;
}

void HttpStreamFactory::Job::OnQuicHostResolution(int result) {
  DCHECK(expect_on_quic_host_resolution_);
  expect_on_quic_host_resolution_ = false;
  delegate_->OnConnectionInitialized(this, result);
}

void HttpStreamFactory::Job::OnFailedOnDefaultNetwork(int result) {
  DCHECK_EQ(job_type_, ALTERNATIVE);
  DCHECK(using_quic_);
  delegate_->OnFailedOnDefaultNetwork(this);
}

int HttpStreamFactory::Job::DoInitConnectionComplete(int result) {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB_INIT_CONNECTION);

  // No need to continue waiting for a session, once a connection is
  // established.
  spdy_session_request_.reset();

  if (job_type_ == PRECONNECT) {
    if (using_quic_)
      return result;
    DCHECK_EQ(OK, result);
    return OK;
  }

  resolve_error_info_ = connection_->resolve_error_info();

  // |result| may be the result of any of the stacked pools. The following
  // logic is used when determining how to interpret an error.
  // If |result| < 0:
  //   and connection_->socket() != NULL, then the SSL handshake ran and it
  //     is a potentially recoverable error.
  //   and connection_->socket == NULL and connection_->is_ssl_error() is true,
  //     then the SSL handshake ran with an unrecoverable error.
  //   otherwise, the error came from one of the other pools.
  bool ssl_started = using_ssl_ && (result == OK || connection_->socket() ||
                                    connection_->is_ssl_error());

  if (ssl_started && result == OK) {
    if (using_quic_) {
      was_alpn_negotiated_ = true;
      negotiated_protocol_ = kProtoQUIC;
    } else {
      if (connection_->socket()->WasAlpnNegotiated()) {
        was_alpn_negotiated_ = true;
        negotiated_protocol_ = connection_->socket()->GetNegotiatedProtocol();
        net_log_.AddEvent(NetLogEventType::HTTP_STREAM_REQUEST_PROTO, [&] {
          return NetLogHttpStreamProtoParams(negotiated_protocol_);
        });
        if (negotiated_protocol_ == kProtoHTTP2) {
          if (is_websocket_) {
            // WebSocket is not supported over a fresh HTTP/2 connection.
            return ERR_NOT_IMPLEMENTED;
          }

          using_spdy_ = true;
        }
      }
    }
  } else if (proxy_info_.is_secure_http_like() && connection_->socket() &&
             result == OK) {
    ProxyClientSocket* proxy_socket =
        static_cast<ProxyClientSocket*>(connection_->socket());
    // http://crbug.com/642354
    if (!proxy_socket->IsConnected())
      return ERR_CONNECTION_CLOSED;
    if (proxy_socket->IsUsingSpdy()) {
      was_alpn_negotiated_ = true;
      negotiated_protocol_ = proxy_socket->GetProxyNegotiatedProtocol();
      using_spdy_ = true;

      // Using unencrypted websockets over an H2 proxy is not currently
      // supported.
      // TODO(mmenke): Should this case be treated like
      // |try_websocket_over_http2_|, or should we force HTTP/1.1?
      if (is_websocket_ && !try_websocket_over_http2_)
        return ERR_NOT_IMPLEMENTED;
    }
  }

  if (proxy_info_.is_quic() && using_quic_ && result < 0)
    return ReconsiderProxyAfterError(result);

  if (expect_spdy_ && !using_spdy_)
    return ERR_ALPN_NEGOTIATION_FAILED;

  if (!ssl_started && result < 0 && (expect_spdy_ || using_quic_))
    return result;

  if (using_quic_) {
    if (result < 0)
      return result;

    if (stream_type_ == HttpStreamRequest::BIDIRECTIONAL_STREAM) {
      std::unique_ptr<QuicChromiumClientSession::Handle> session =
          quic_request_.ReleaseSessionHandle();
      if (!session) {
        // Quic session is closed before stream can be created.
        return ERR_CONNECTION_CLOSED;
      }
      bidirectional_stream_impl_.reset(
          new BidirectionalStreamQuicImpl(std::move(session)));
    } else {
      std::unique_ptr<QuicChromiumClientSession::Handle> session =
          quic_request_.ReleaseSessionHandle();
      if (!session) {
        // Quic session is closed before stream can be created.
        return ERR_CONNECTION_CLOSED;
      }
      stream_ = std::make_unique<QuicHttpStream>(std::move(session));
    }
    next_state_ = STATE_NONE;
    return OK;
  }

  if (result < 0 && !ssl_started)
    return ReconsiderProxyAfterError(result);

  establishing_tunnel_ = false;

  // Handle SSL errors below.
  if (using_ssl_) {
    DCHECK(ssl_started);
    if (IsCertificateError(result)) {
      SSLInfo ssl_info;
      GetSSLInfo(&ssl_info);
      if (ssl_info.cert) {
        // Add the bad certificate to the set of allowed certificates in the
        // SSL config object. This data structure will be consulted after
        // calling RestartIgnoringLastError(). And the user will be asked
        // interactively before RestartIgnoringLastError() is ever called.
        server_ssl_config_.allowed_bad_certs.emplace_back(ssl_info.cert,
                                                          ssl_info.cert_status);
      }
    }
    if (result < 0)
      return result;
  }

  next_state_ = STATE_CREATE_STREAM;
  return OK;
}

int HttpStreamFactory::Job::DoWaitingUserAction(int result) {
  // This state indicates that the stream request is in a partially
  // completed state, and we've called back to the delegate for more
  // information.

  // We're always waiting here for the delegate to call us back.
  return ERR_IO_PENDING;
}

int HttpStreamFactory::Job::SetSpdyHttpStreamOrBidirectionalStreamImpl(
    base::WeakPtr<SpdySession> session) {
  DCHECK(using_spdy_);

  if (is_websocket_) {
    DCHECK_NE(job_type_, PRECONNECT);
    DCHECK(delegate_->websocket_handshake_stream_create_helper());

    if (!try_websocket_over_http2_) {
      // Plaintext WebSocket is not supported over HTTP/2 proxy,
      // see https://crbug.com/684681.
      return ERR_NOT_IMPLEMENTED;
    }

    websocket_stream_ = delegate_->websocket_handshake_stream_create_helper()
                            ->CreateHttp2Stream(session);
    return OK;
  }
  if (stream_type_ == HttpStreamRequest::BIDIRECTIONAL_STREAM) {
    bidirectional_stream_impl_ = std::make_unique<BidirectionalStreamSpdyImpl>(
        session, net_log_.source());
    return OK;
  }

  // TODO(willchan): Delete this code, because eventually, the HttpStreamFactory
  // will be creating all the SpdyHttpStreams, since it will know when
  // SpdySessions become available.

  stream_ = std::make_unique<SpdyHttpStream>(session, pushed_stream_id_,
                                             net_log_.source());
  return OK;
}

int HttpStreamFactory::Job::DoCreateStream() {
  DCHECK(connection_->socket() || existing_spdy_session_.get());
  DCHECK(!using_quic_);

  next_state_ = STATE_CREATE_STREAM_COMPLETE;

  if (!using_spdy_) {
    DCHECK(!expect_spdy_);
    bool using_proxy = (proxy_info_.is_http_like()) &&
                       request_info_.url.SchemeIs(url::kHttpScheme);
    if (is_websocket_) {
      DCHECK_NE(job_type_, PRECONNECT);
      DCHECK(delegate_->websocket_handshake_stream_create_helper());
      websocket_stream_ =
          delegate_->websocket_handshake_stream_create_helper()
              ->CreateBasicStream(std::move(connection_), using_proxy,
                                  session_->websocket_endpoint_lock_manager());
    } else {
      if (request_info_.upload_data_stream &&
          !request_info_.upload_data_stream->AllowHTTP1()) {
        return ERR_H2_OR_QUIC_REQUIRED;
      }
      stream_ = std::make_unique<HttpBasicStream>(std::move(connection_),
                                                  using_proxy);
    }
    return OK;
  }

  CHECK(!stream_.get());

  // It is possible that a pushed stream has been opened by a server since last
  // time Job checked above.
  if (!existing_spdy_session_) {
    // WebSocket over HTTP/2 is only allowed to use existing HTTP/2 connections.
    // Therefore |using_spdy_| could not have been set unless a connection had
    // already been found.
    DCHECK(!is_websocket_);

    session_->spdy_session_pool()->push_promise_index()->ClaimPushedStream(
        spdy_session_key_, origin_url_, request_info_, &existing_spdy_session_,
        &pushed_stream_id_);
    // It is also possible that an HTTP/2 connection has been established since
    // last time Job checked above.
    if (!existing_spdy_session_) {
      existing_spdy_session_ =
          session_->spdy_session_pool()->FindAvailableSession(
              spdy_session_key_, enable_ip_based_pooling_,
              /* is_websocket = */ false, net_log_);
    }
  }
  if (existing_spdy_session_) {
    // We picked up an existing session, so we don't need our socket.
    if (connection_->socket())
      connection_->socket()->Disconnect();
    connection_->Reset();

    int set_result =
        SetSpdyHttpStreamOrBidirectionalStreamImpl(existing_spdy_session_);
    existing_spdy_session_.reset();
    return set_result;
  }

  // Close idle sockets in this group, since subsequent requests will go over
  // |spdy_session|.
  if (connection_->socket()->IsConnected())
    connection_->CloseIdleSocketsInGroup("Switching to HTTP2 session");

  // If |spdy_session_direct_| is false, then |proxy_info_| is guaranteed to
  // have a non-empty proxy list.
  bool is_trusted_proxy =
      !spdy_session_direct_ && proxy_info_.proxy_server().is_trusted_proxy();

  base::WeakPtr<SpdySession> spdy_session =
      session_->spdy_session_pool()->CreateAvailableSessionFromSocketHandle(
          spdy_session_key_, is_trusted_proxy, std::move(connection_),
          net_log_);

  if (!spdy_session->HasAcceptableTransportSecurity()) {
    spdy_session->CloseSessionOnError(ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY,
                                      "");
    return ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY;
  }

  url::SchemeHostPort scheme_host_port(
      using_ssl_ ? url::kHttpsScheme : url::kHttpScheme,
      spdy_session_key_.host_port_pair().host(),
      spdy_session_key_.host_port_pair().port());

  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  if (http_server_properties) {
    http_server_properties->SetSupportsSpdy(scheme_host_port,
                                            request_info_.network_isolation_key,
                                            true /* supports_spdy */);
  }

  // Create a SpdyHttpStream or a BidirectionalStreamImpl attached to the
  // session.
  return SetSpdyHttpStreamOrBidirectionalStreamImpl(spdy_session);
}

int HttpStreamFactory::Job::DoCreateStreamComplete(int result) {
  if (result < 0)
    return result;

  session_->proxy_resolution_service()->ReportSuccess(proxy_info_);
  next_state_ = STATE_NONE;
  return OK;
}

void HttpStreamFactory::Job::OnSpdySessionAvailable(
    base::WeakPtr<SpdySession> spdy_session) {
  DCHECK(spdy_session);

  // No need for the connection any more, since |spdy_session| can be used
  // instead, and there's no benefit from keeping the old ConnectJob in the
  // socket pool.
  if (connection_)
    connection_->ResetAndCloseSocket();

  // Once a connection is initialized, or if there's any out-of-band callback,
  // like proxy auth challenge, the SpdySessionRequest is cancelled.
  DCHECK(next_state_ == STATE_INIT_CONNECTION ||
         next_state_ == STATE_INIT_CONNECTION_COMPLETE);

  // Ignore calls to ResumeInitConnection() from either the timer or the
  // SpdySessionPool.
  init_connection_already_resumed_ = true;

  // If this is a preconnect, nothing left do to.
  if (job_type_ == PRECONNECT) {
    OnPreconnectsComplete();
    return;
  }

  using_spdy_ = true;
  existing_spdy_session_ = spdy_session;
  next_state_ = STATE_CREATE_STREAM;

  // This will synchronously close |connection_|, so no need to worry about it
  // calling back into |this|.
  RunLoop(net::OK);
}

int HttpStreamFactory::Job::ReconsiderProxyAfterError(int error) {
  // Check if the error was a proxy failure.
  if (!CanFalloverToNextProxy(proxy_info_.proxy_server(), error, &error))
    return error;

  should_reconsider_proxy_ = true;
  return error;
}

ClientSocketPoolManager::SocketGroupType
HttpStreamFactory::Job::GetSocketGroup() const {
  std::string scheme = origin_url_.scheme();

  if (scheme == url::kHttpsScheme || scheme == url::kWssScheme)
    return ClientSocketPoolManager::SSL_GROUP;

  DCHECK(scheme == url::kHttpScheme || scheme == url::kWsScheme);
  return ClientSocketPoolManager::NORMAL_GROUP;
}

// If the connection succeeds, failed connection attempts leading up to the
// success will be returned via the successfully connected socket. If the
// connection fails, failed connection attempts will be returned via the
// ClientSocketHandle. Check whether a socket was returned and copy the
// connection attempts from the proper place.
void HttpStreamFactory::Job::MaybeCopyConnectionAttemptsFromSocketOrHandle() {
  if (!connection_)
    return;

  ConnectionAttempts socket_attempts = connection_->connection_attempts();
  if (connection_->socket()) {
    connection_->socket()->GetConnectionAttempts(&socket_attempts);
  }

  delegate_->AddConnectionAttemptsToRequest(this, socket_attempts);
}

HttpStreamFactory::JobFactory::JobFactory() = default;

HttpStreamFactory::JobFactory::~JobFactory() = default;

std::unique_ptr<HttpStreamFactory::Job>
HttpStreamFactory::JobFactory::CreateMainJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HostPortPair destination,
    GURL origin_url,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log) {
  return std::make_unique<HttpStreamFactory::Job>(
      delegate, job_type, session, request_info, priority, proxy_info,
      server_ssl_config, proxy_ssl_config, destination, origin_url,
      kProtoUnknown, quic::ParsedQuicVersion::Unsupported(), is_websocket,
      enable_ip_based_pooling, net_log);
}

std::unique_ptr<HttpStreamFactory::Job>
HttpStreamFactory::JobFactory::CreateAltSvcJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const HttpRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const SSLConfig& server_ssl_config,
    const SSLConfig& proxy_ssl_config,
    HostPortPair destination,
    GURL origin_url,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log) {
  return std::make_unique<HttpStreamFactory::Job>(
      delegate, job_type, session, request_info, priority, proxy_info,
      server_ssl_config, proxy_ssl_config, destination, origin_url,
      alternative_protocol, quic_version, is_websocket, enable_ip_based_pooling,
      net_log);
}

bool HttpStreamFactory::Job::ShouldThrottleConnectForSpdy() const {
  DCHECK(!using_quic_);
  DCHECK(!spdy_session_request_);

  // If the job has previously been throttled, don't throttle it again.
  if (init_connection_already_resumed_)
    return false;

  url::SchemeHostPort scheme_host_port(
      using_ssl_ ? url::kHttpsScheme : url::kHttpScheme,
      spdy_session_key_.host_port_pair().host(),
      spdy_session_key_.host_port_pair().port());
  // Only throttle the request if the server is believed to support H2.
  return session_->http_server_properties()->GetSupportsSpdy(
      scheme_host_port, request_info_.network_isolation_key);
}

}  // namespace net

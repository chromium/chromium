// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_job.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/port_util.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/session_usage.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/http/proxy_fallback.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/bidirectional_stream_quic_impl.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_key.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/connect_job.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/bidirectional_stream_spdy_impl.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

// Experiment to preconnect only one connection if HttpServerProperties is
// not supported or initialized.
BASE_FEATURE(kLimitEarlyPreconnectsExperiment,
             "LimitEarlyPreconnects",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

const char* NetLogHttpStreamJobType(HttpStreamFactory::JobType job_type) {
  switch (job_type) {
    case HttpStreamFactory::MAIN:
      return "main";
    case HttpStreamFactory::ALTERNATIVE:
      return "alternative";
    case HttpStreamFactory::DNS_ALPN_H3:
      return "dns_alpn_h3";
    case HttpStreamFactory::PRECONNECT:
      return "preconnect";
    case HttpStreamFactory::PRECONNECT_DNS_ALPN_H3:
      return "preconnect_dns_alpn_h3";
  }
  return "";
}

// Returns parameters associated with the ALPN protocol of a HTTP stream.
base::Value::Dict NetLogHttpStreamProtoParams(NextProto negotiated_protocol) {
  base::Value::Dict dict;

  dict.Set("proto", NextProtoToString(negotiated_protocol));
  return dict;
}

HttpStreamFactory::Job::Job(
    Delegate* delegate,
    JobType job_type,
    HttpNetworkSession* session,
    const StreamRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    url::SchemeHostPort destination,
    GURL origin_url,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log)
    : request_info_(request_info),
      priority_(priority),
      proxy_info_(proxy_info),
      allowed_bad_certs_(allowed_bad_certs),
      net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::HTTP_STREAM_JOB)),
      io_callback_(
          base::BindRepeating(&Job::OnIOComplete, base::Unretained(this))),
      connection_(std::make_unique<ClientSocketHandle>()),
      session_(session),
      destination_(std::move(destination)),
      origin_url_(std::move(origin_url)),
      is_websocket_(is_websocket),
      try_websocket_over_http2_(is_websocket_ &&
                                origin_url_.SchemeIs(url::kWssScheme)),
      // Only support IP-based pooling for non-proxied streams.
      enable_ip_based_pooling_(enable_ip_based_pooling &&
                               proxy_info.is_direct()),
      delegate_(delegate),
      job_type_(job_type),
      using_ssl_(origin_url_.SchemeIs(url::kHttpsScheme) ||
                 origin_url_.SchemeIs(url::kWssScheme)),
      using_quic_(
          alternative_protocol == kProtoQUIC ||
          session->ShouldForceQuic(destination_, proxy_info, is_websocket_) ||
          job_type == DNS_ALPN_H3 || job_type == PRECONNECT_DNS_ALPN_H3),
      quic_version_(quic_version),
      expect_spdy_(alternative_protocol == kProtoHTTP2 && !using_quic_),
      quic_request_(session_->quic_session_pool()),
      spdy_session_key_(using_quic_
                            ? SpdySessionKey()
                            : GetSpdySessionKey(proxy_info_.proxy_chain(),
                                                origin_url_,
                                                request_info_)) {
  // Websocket `destination` schemes should be converted to HTTP(S).
  DCHECK(base::EqualsCaseInsensitiveASCII(destination_.scheme(),
                                          url::kHttpScheme) ||
         base::EqualsCaseInsensitiveASCII(destination_.scheme(),
                                          url::kHttpsScheme));

  // This class is specific to a single `ProxyChain`, so `proxy_info_` must be
  // non-empty. Entries beyond the first are ignored. It should simply take a
  // `ProxyChain`, but the full `ProxyInfo` is passed back to
  // `HttpNetworkTransaction`, which consumes additional fields.
  DCHECK(!proxy_info_.is_empty());

  // The Job is forced to use QUIC without a designated version, try the
  // preferred QUIC version that is supported by default.
  if (quic_version_ == quic::ParsedQuicVersion::Unsupported() &&
      session->ShouldForceQuic(destination_, proxy_info, is_websocket_)) {
    quic_version_ =
        session->context().quic_context->params()->supported_versions[0];
  }

  if (using_quic_) {
    DCHECK((quic_version_ != quic::ParsedQuicVersion::Unsupported()) ||
           (job_type_ == DNS_ALPN_H3) || (job_type_ == PRECONNECT_DNS_ALPN_H3));
  }

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
  if (started_) {
    net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB);
  }

  // When we're in a partially constructed state, waiting for the user to
  // provide certificate handling information or authentication, we can't reuse
  // this stream at all.
  if (next_state_ == STATE_WAITING_USER_ACTION) {
    connection_->socket()->Disconnect();
    connection_.reset();
  }

  // The stream could be in a partial state.  It is not reusable.
  if (stream_.get() && next_state_ != STATE_DONE) {
    stream_->Close(true /* not reusable */);
  }
}

void HttpStreamFactory::Job::Start(HttpStreamRequest::StreamType stream_type) {
  started_ = true;
  stream_type_ = stream_type;

  const NetLogWithSource* delegate_net_log = delegate_->GetNetLog();
  if (delegate_net_log) {
    net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_JOB, [&] {
      base::Value::Dict dict;
      const auto& source = delegate_net_log->source();
      if (source.IsValid()) {
        source.AddToEventParameters(dict);
      }
      dict.Set("logical_destination",
               url::SchemeHostPort(origin_url_).Serialize());
      dict.Set("destination", destination_.Serialize());
      dict.Set("expect_spdy", expect_spdy_);
      dict.Set("using_quic", using_quic_);
      dict.Set("priority", RequestPriorityToString(priority_));
      dict.Set("type", NetLogHttpStreamJobType(job_type_));
      return dict;
    });
    delegate_net_log->AddEventReferencingSource(
        NetLogEventType::HTTP_STREAM_REQUEST_STARTED_JOB, net_log_.source());
  }

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
      origin_url_.SchemeIsCryptographic();
  if (connect_one_stream || http_server_properties->SupportsRequestPriority(
                                url::SchemeHostPort(origin_url_),
                                request_info_.network_anonymization_key)) {
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
  DCHECK(job_type_ == ALTERNATIVE || job_type_ == DNS_ALPN_H3);
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
  if (connection_ && connection_->is_initialized()) {
    connection_->SetPriority(priority);
  }
  // TODO(akalin): Maybe Propagate this to the preconnect state.
}

bool HttpStreamFactory::Job::HasAvailableSpdySession() const {
  return !using_quic_ && CanUseExistingSpdySession() &&
         session_->spdy_session_pool()->HasAvailableSession(spdy_session_key_,
                                                            is_websocket_);
}

bool HttpStreamFactory::Job::HasAvailableQuicSession() const {
  if (!using_quic_) {
    return false;
  }
  bool require_dns_https_alpn =
      (job_type_ == DNS_ALPN_H3) || (job_type_ == PRECONNECT_DNS_ALPN_H3);

  QuicSessionKey quic_session_key(
      HostPortPair::FromURL(origin_url_), request_info_.privacy_mode,
      proxy_info_.proxy_chain(), SessionUsage::kDestination,
      request_info_.socket_tag, request_info_.network_anonymization_key,
      request_info_.secure_dns_policy, require_dns_https_alpn);
  return session_->quic_session_pool()->CanUseExistingSession(quic_session_key,
                                                              destination_);
}

bool HttpStreamFactory::Job::TargettedSocketGroupHasActiveSocket() const {
  DCHECK(!using_quic_);
  DCHECK(!is_websocket_);
  ClientSocketPool* pool = session_->GetSocketPool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, proxy_info_.proxy_chain());
  DCHECK(pool);
  ClientSocketPool::GroupId connection_group(
      destination_, request_info_.privacy_mode,
      request_info_.network_anonymization_key, request_info_.secure_dns_policy,
      disable_cert_verification_network_fetches());
  return pool->HasActiveSocket(connection_group);
}

NextProto HttpStreamFactory::Job::negotiated_protocol() const {
  return negotiated_protocol_;
}

bool HttpStreamFactory::Job::using_spdy() const {
  return negotiated_protocol_ == kProtoHTTP2;
}

bool HttpStreamFactory::Job::disable_cert_verification_network_fetches() const {
  return !!(request_info_.load_flags & LOAD_DISABLE_CERT_NETWORK_FETCHES);
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

bool HttpStreamFactory::Job::UsingHttpProxyWithoutTunnel() const {
  return !using_quic_ && !using_ssl_ && !is_websocket_ &&
         proxy_info_.proxy_chain().is_get_to_proxy_allowed();
}

bool HttpStreamFactory::Job::CanUseExistingSpdySession() const {
  DCHECK(!using_quic_);

  if (proxy_info_.is_direct() &&
      session_->http_server_properties()->RequiresHTTP11(
          url::SchemeHostPort(origin_url_),
          request_info_.network_anonymization_key)) {
    return false;
  }

  if (is_websocket_) {
    return try_websocket_over_http2_;
  }

  DCHECK(origin_url_.SchemeIsHTTPOrHTTPS());

  // We need to make sure that if a HTTP/2 session was created for
  // https://somehost/ then we do not use that session for http://somehost:443/.
  // The only time we can use an existing session is if the request URL is
  // https (the normal case) or if we are connecting to an HTTPS proxy to make
  // a GET request for an HTTP destination. https://crbug.com/133176
  if (origin_url_.SchemeIs(url::kHttpsScheme)) {
    return true;
  }
  if (!proxy_info_.is_empty()) {
    const ProxyChain& proxy_chain = proxy_info_.proxy_chain();
    if (!proxy_chain.is_direct() && proxy_chain.is_get_to_proxy_allowed() &&
        proxy_chain.Last().is_https()) {
      return true;
    }
  }
  return false;
}

void HttpStreamFactory::Job::OnStreamReadyCallback() {
  DCHECK(stream_.get());
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
  DCHECK(!is_websocket_ || try_websocket_over_http2_);

  MaybeCopyConnectionAttemptsFromHandle();

  delegate_->OnStreamReady(this);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnWebSocketHandshakeStreamReadyCallback() {
  DCHECK(websocket_stream_);
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
  DCHECK(is_websocket_);

  MaybeCopyConnectionAttemptsFromHandle();

  delegate_->OnWebSocketHandshakeStreamReady(this, proxy_info_,
                                             std::move(websocket_stream_));
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnBidirectionalStreamImplReadyCallback() {
  DCHECK(bidirectional_stream_impl_);

  MaybeCopyConnectionAttemptsFromHandle();

  delegate_->OnBidirectionalStreamImplReady(this, proxy_info_);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnStreamFailedCallback(int result) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);

  MaybeCopyConnectionAttemptsFromHandle();

  delegate_->OnStreamFailed(this, result);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnCertificateErrorCallback(
    int result,
    const SSLInfo& ssl_info) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
  DCHECK(!spdy_session_request_);

  MaybeCopyConnectionAttemptsFromHandle();

  delegate_->OnCertificateError(this, result, ssl_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnNeedsProxyAuthCallback(
    const HttpResponseInfo& response,
    HttpAuthController* auth_controller,
    base::OnceClosure restart_with_auth_callback) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
  DCHECK(establishing_tunnel_);
  DCHECK(!restart_with_auth_callback_);

  restart_with_auth_callback_ = std::move(restart_with_auth_callback);

  // This is called out of band, so need to abort the SpdySessionRequest to
  // prevent being passed a new session while waiting on proxy auth credentials.
  spdy_session_request_.reset();

  delegate_->OnNeedsProxyAuth(this, response, proxy_info_, auth_controller);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnNeedsClientAuthCallback(
    SSLCertRequestInfo* cert_info) {
  DCHECK_NE(job_type_, PRECONNECT);
  DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
  DCHECK(!spdy_session_request_);

  delegate_->OnNeedsClientAuth(this, cert_info);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnPreconnectsComplete(int result) {
  delegate_->OnPreconnectsComplete(this, result);
  // |this| may be deleted after this call.
}

void HttpStreamFactory::Job::OnIOComplete(int result) {
  RunLoop(result);
}

void HttpStreamFactory::Job::RunLoop(int result) {
  result = DoLoop(result);

  if (result == ERR_IO_PENDING) {
    return;
  }

  // Stop watching for new SpdySessions, to avoid receiving a new SPDY session
  // while doing anything other than waiting to establish a connection.
  spdy_session_request_.reset();

  if ((job_type_ == PRECONNECT) || (job_type_ == PRECONNECT_DNS_ALPN_H3)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamFactory::Job::OnPreconnectsComplete,
                       ptr_factory_.GetWeakPtr(), result));
    return;
  }

  if (IsCertificateError(result)) {
    // Retrieve SSL information from the socket.
    SSLInfo ssl_info;
    GetSSLInfo(&ssl_info);

    next_state_ = STATE_WAITING_USER_ACTION;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpStreamFactory::Job::OnCertificateErrorCallback,
                       ptr_factory_.GetWeakPtr(), result, ssl_info));
    return;
  }

  switch (result) {
    case ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &Job::OnNeedsClientAuthCallback, ptr_factory_.GetWeakPtr(),
              base::RetainedRef(connection_->ssl_cert_request_info())));
      return;

    case OK:
      next_state_ = STATE_DONE;
      if (is_websocket_) {
        DCHECK(websocket_stream_);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&Job::OnWebSocketHandshakeStreamReadyCallback,
                           ptr_factory_.GetWeakPtr()));
      } else if (stream_type_ == HttpStreamRequest::BIDIRECTIONAL_STREAM) {
        if (!bidirectional_stream_impl_) {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(&Job::OnStreamFailedCallback,
                                        ptr_factory_.GetWeakPtr(), ERR_FAILED));
        } else {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&Job::OnBidirectionalStreamImplReadyCallback,
                             ptr_factory_.GetWeakPtr()));
        }
      } else {
        DCHECK(stream_.get());
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&Job::OnStreamReadyCallback,
                                      ptr_factory_.GetWeakPtr()));
      }
      return;

    default:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
        NOTREACHED_IN_MIGRATION() << "bad state";
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
  // Don't connect to restricted ports.
  if (!IsPortAllowedForScheme(destination_.port(),
                              origin_url_.scheme_piece())) {
    return ERR_UNSAFE_PORT;
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
  if (should_wait) {
    return ERR_IO_PENDING;
  }

  return OK;
}

int HttpStreamFactory::Job::DoWaitComplete(int result) {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB_WAITING);
  DCHECK_EQ(OK, result);
  next_state_ = STATE_INIT_CONNECTION;
  return OK;
}

void HttpStreamFactory::Job::ResumeInitConnection() {
  if (init_connection_already_resumed_) {
    return;
  }
  DCHECK_EQ(next_state_, STATE_INIT_CONNECTION);
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_JOB_RESUME_INIT_CONNECTION);
  init_connection_already_resumed_ = true;
  OnIOComplete(OK);
}

int HttpStreamFactory::Job::DoInitConnection() {
  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_JOB_INIT_CONNECTION);
  int result = DoInitConnectionImpl();
  if (!expect_on_quic_session_created_ && !expect_on_quic_host_resolution_) {
    delegate_->OnConnectionInitialized(this, result);
  }
  return result;
}

int HttpStreamFactory::Job::DoInitConnectionImpl() {
  DCHECK(!connection_->is_initialized());

  if (using_quic_ && !proxy_info_.is_direct() &&
      !proxy_info_.proxy_chain().Last().is_quic()) {
    // QUIC can not be spoken to non-QUIC proxies.  This error should not be
    // user visible, because the non-alternative Job should be resumed.
    return ERR_NO_SUPPORTED_PROXIES;
  }

  DCHECK(proxy_info_.proxy_chain().IsValid());
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  if (using_quic_) {
    // TODO(mmenke): Clean this up. `disable_cert_verification_network_fetches`
    // is enabled in ConnectJobFactory for H1/H2 connections. Also need to add
    // it to the SpdySessionKey for H2 connections.
    SSLConfig server_ssl_config;
    server_ssl_config.disable_cert_verification_network_fetches =
        disable_cert_verification_network_fetches();
    return DoInitConnectionImplQuic(server_ssl_config.GetCertVerifyFlags());
  }

  // Check first if there is a pushed stream matching the request, or an HTTP/2
  // connection this request can pool to.  If so, then go straight to using
  // that.
  if (CanUseExistingSpdySession()) {
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
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE, resume_callback, base::Milliseconds(kHTTP2ThrottleMs));
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
      if (job_type_ == PRECONNECT) {
        return OK;
      }
      negotiated_protocol_ = kProtoHTTP2;
      next_state_ = STATE_CREATE_STREAM;
      return OK;
    }
  }

  establishing_tunnel_ = !UsingHttpProxyWithoutTunnel();

  if (job_type_ == PRECONNECT) {
    DCHECK(!is_websocket_);
    DCHECK(request_info_.socket_tag == SocketTag());

    // The lifeime of the preconnect tasks is not controlled by |connection_|.
    // It may outlives |this|. So we can't use |io_callback_| which holds
    // base::Unretained(this).
    auto callback =
        base::BindOnce(&Job::OnIOComplete, ptr_factory_.GetWeakPtr());

    return PreconnectSocketsForHttpRequest(
        destination_, request_info_.load_flags, priority_, session_,
        proxy_info_, allowed_bad_certs_, request_info_.privacy_mode,
        request_info_.network_anonymization_key,
        request_info_.secure_dns_policy, net_log_, num_streams_,
        std::move(callback));
  }

  ClientSocketPool::ProxyAuthCallback proxy_auth_callback =
      base::BindRepeating(&HttpStreamFactory::Job::OnNeedsProxyAuthCallback,
                          base::Unretained(this));
  if (is_websocket_) {
    DCHECK(request_info_.socket_tag == SocketTag());
    DCHECK_EQ(SecureDnsPolicy::kAllow, request_info_.secure_dns_policy);
    return InitSocketHandleForWebSocketRequest(
        destination_, request_info_.load_flags, priority_, session_,
        proxy_info_, allowed_bad_certs_, request_info_.privacy_mode,
        request_info_.network_anonymization_key, net_log_, connection_.get(),
        io_callback_, proxy_auth_callback);
  }

  return InitSocketHandleForHttpRequest(
      destination_, request_info_.load_flags, priority_, session_, proxy_info_,
      allowed_bad_certs_, request_info_.privacy_mode,
      request_info_.network_anonymization_key, request_info_.secure_dns_policy,
      request_info_.socket_tag, net_log_, connection_.get(), io_callback_,
      proxy_auth_callback);
}

int HttpStreamFactory::Job::DoInitConnectionImplQuic(
    int server_cert_verifier_flags) {
  url::SchemeHostPort destination;

  bool require_dns_https_alpn =
      (job_type_ == DNS_ALPN_H3) || (job_type_ == PRECONNECT_DNS_ALPN_H3);

  ProxyChain proxy_chain = proxy_info_.proxy_chain();
  if (!proxy_chain.is_direct()) {
    // We only support proxying QUIC over QUIC. While MASQUE defines mechanisms
    // to carry QUIC traffic over non-QUIC proxies, the performance of these
    // mechanisms would be worse than simply using H/1 or H/2 to reach the
    // destination. The error for an invalid condition should not be user
    // visible, because the non-alternative Job should be resumed.
    if (proxy_chain.AnyProxy(
            [](const ProxyServer& s) { return !s.is_quic(); })) {
      return ERR_NO_SUPPORTED_PROXIES;
    }
  }

  std::optional<NetworkTrafficAnnotationTag> traffic_annotation =
      proxy_info_.traffic_annotation().is_valid()
          ? std::make_optional<NetworkTrafficAnnotationTag>(
                proxy_info_.traffic_annotation())
          : std::nullopt;

  // The QuicSessionRequest will take care of connecting to any proxies in the
  // proxy chain.
  int rv = quic_request_.Request(
      destination_, quic_version_, proxy_chain, std::move(traffic_annotation),
      session_->context().http_user_agent_settings.get(),
      SessionUsage::kDestination, request_info_.privacy_mode, priority_,
      request_info_.socket_tag, request_info_.network_anonymization_key,
      request_info_.secure_dns_policy, require_dns_https_alpn,
      server_cert_verifier_flags, origin_url_, net_log_, &net_error_details_,
      base::BindOnce(&Job::OnFailedOnDefaultNetwork, ptr_factory_.GetWeakPtr()),
      io_callback_);
  if (rv == OK) {
    using_existing_quic_session_ = true;
  } else if (rv == ERR_IO_PENDING) {
    // There's no available QUIC session. Inform the delegate how long to
    // delay the main job.
    delegate_->MaybeSetWaitTimeForMainJob(
        quic_request_.GetTimeDelayForWaitingJob());
    // Set up to get notified of either host resolution completion or session
    // creation, in order to call the delegate's `OnConnectionInitialized`
    // callback.
    expect_on_quic_host_resolution_ = quic_request_.WaitForHostResolution(
        base::BindOnce(&Job::OnQuicHostResolution, base::Unretained(this)));
    expect_on_quic_session_created_ = quic_request_.WaitForQuicSessionCreation(
        base::BindOnce(&Job::OnQuicSessionCreated, ptr_factory_.GetWeakPtr()));
  }
  return rv;
}

void HttpStreamFactory::Job::OnQuicHostResolution(int result) {
  DCHECK(expect_on_quic_host_resolution_);
  expect_on_quic_host_resolution_ = false;

  delegate_->OnQuicHostResolution(destination_,
                                  quic_request_.dns_resolution_start_time(),
                                  quic_request_.dns_resolution_end_time());

  // If no `OnQuicSessionCreated` call is expected, then consider the
  // connection "initialized" and inform the delegate. Note that
  // `OnQuicHostResolution` is actually called somewhat _after_ host resolution
  // is complete -- the `Job` has already run to the point where it can make no
  // further progress.
  if (!expect_on_quic_session_created_) {
    delegate_->OnConnectionInitialized(this, result);
  }
}

void HttpStreamFactory::Job::OnQuicSessionCreated(int result) {
  DCHECK(expect_on_quic_session_created_);
  expect_on_quic_session_created_ = false;
  delegate_->OnConnectionInitialized(this, result);
}

void HttpStreamFactory::Job::OnFailedOnDefaultNetwork(int result) {
  DCHECK(job_type_ == ALTERNATIVE || job_type_ == DNS_ALPN_H3);
  DCHECK(using_quic_);
  delegate_->OnFailedOnDefaultNetwork(this);
}

int HttpStreamFactory::Job::DoInitConnectionComplete(int result) {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_JOB_INIT_CONNECTION);

  establishing_tunnel_ = false;

  // No need to continue waiting for a session, once a connection is
  // established.
  spdy_session_request_.reset();

  if ((job_type_ == PRECONNECT) || (job_type_ == PRECONNECT_DNS_ALPN_H3)) {
    if (using_quic_) {
      return result;
    }
    DCHECK_EQ(OK, result);
    return OK;
  }

  resolve_error_info_ = connection_->resolve_error_info();

  // Determine the protocol (HTTP/1.1, HTTP/2, or HTTP/3). This covers both the
  // origin and some proxy cases. First, if the URL is HTTPS (or WSS), we may
  // negotiate HTTP/2 or HTTP/3 with the origin. Second, non-tunneled requests
  // (i.e. HTTP URLs) through an HTTPS or QUIC proxy work by sending the request
  // to the proxy directly. In that case, this logic also handles the proxy's
  // negotiated protocol. HTTPS requests are always tunneled, so at most one of
  // these applies.
  //
  // Tunneled requests may also negotiate ALPN at the proxy, but
  // HttpProxyConnectJob handles ALPN. The resulting StreamSocket will not
  // report an ALPN protocol.
  if (result == OK) {
    if (using_quic_) {
      // TODO(davidben): Record these values consistently between QUIC and TCP
      // below. In the QUIC case, we only record it for origin connections. In
      // the TCP case, we also record it for non-tunneled, proxied requests.
      if (using_ssl_) {
        negotiated_protocol_ = kProtoQUIC;
      }
    } else if (connection_->socket()->GetNegotiatedProtocol() !=
               kProtoUnknown) {
      // Only connections that use TLS (either to the origin or via a GET to a
      // secure proxy) can negotiate ALPN.
      bool get_to_secure_proxy =
          IsGetToProxy(proxy_info_.proxy_chain(), origin_url_) &&
          proxy_info_.proxy_chain().Last().is_secure_http_like();
      DCHECK(using_ssl_ || get_to_secure_proxy);
      negotiated_protocol_ = connection_->socket()->GetNegotiatedProtocol();
      net_log_.AddEvent(NetLogEventType::HTTP_STREAM_REQUEST_PROTO, [&] {
        return NetLogHttpStreamProtoParams(negotiated_protocol_);
      });
      if (using_spdy()) {
        if (is_websocket_) {
          // WebSocket is not supported over a fresh HTTP/2 connection. This
          // should not be reachable. For the origin, we do not request HTTP/2
          // on fresh WebSockets connections, because not all HTTP/2 servers
          // implement RFC 8441. For proxies, WebSockets are always tunneled.
          //
          // TODO(davidben): This isn't a CHECK() because, previously, it was
          // reachable in https://crbug.com/828865. However, if reachable, it
          // means a bug in the socket pools. The socket pools have since been
          // cleaned up, so this may no longer be reachable. Restore the CHECK
          // and see if this is still needed.
          return ERR_NOT_IMPLEMENTED;
        }
      }
    }
  }

  if (using_quic_ && result < 0 && !proxy_info_.is_direct() &&
      proxy_info_.proxy_chain().Last().is_quic()) {
    return ReconsiderProxyAfterError(result);
  }

  if (expect_spdy_ && !using_spdy()) {
    return ERR_ALPN_NEGOTIATION_FAILED;
  }

  // |result| may be the result of any of the stacked protocols. The following
  // logic is used when determining how to interpret an error.
  // If |result| < 0:
  //   and connection_->socket() != NULL, then the SSL handshake ran and it
  //     is a potentially recoverable error.
  //   and connection_->socket == NULL and connection_->is_ssl_error() is true,
  //     then the SSL handshake ran with an unrecoverable error.
  //   otherwise, the error came from one of the other protocols.
  bool ssl_started = using_ssl_ && (result == OK || connection_->socket() ||
                                    connection_->is_ssl_error());
  if (!ssl_started && result < 0 && (expect_spdy_ || using_quic_)) {
    return result;
  }

  if (using_quic_) {
    if (result < 0) {
      return result;
    }

    if (stream_type_ == HttpStreamRequest::BIDIRECTIONAL_STREAM) {
      std::unique_ptr<QuicChromiumClientSession::Handle> session =
          quic_request_.ReleaseSessionHandle();
      if (!session) {
        // Quic session is closed before stream can be created.
        return ERR_CONNECTION_CLOSED;
      }
      bidirectional_stream_impl_ =
          std::make_unique<BidirectionalStreamQuicImpl>(std::move(session));
    } else {
      std::unique_ptr<QuicChromiumClientSession::Handle> session =
          quic_request_.ReleaseSessionHandle();
      if (!session) {
        // Quic session is closed before stream can be created.
        return ERR_CONNECTION_CLOSED;
      }
      auto dns_aliases =
          session->GetDnsAliasesForSessionKey(quic_request_.session_key());
      stream_ = std::make_unique<QuicHttpStream>(std::move(session),
                                                 std::move(dns_aliases));
    }
    next_state_ = STATE_CREATE_STREAM_COMPLETE;
    return OK;
  }

  if (result < 0) {
    if (!ssl_started) {
      return ReconsiderProxyAfterError(result);
    }
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
  DCHECK(using_spdy());
  auto dns_aliases = session_->spdy_session_pool()->GetDnsAliasesForSessionKey(
      spdy_session_key_);

  if (is_websocket_) {
    DCHECK_NE(job_type_, PRECONNECT);
    DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
    DCHECK(delegate_->websocket_handshake_stream_create_helper());

    if (!try_websocket_over_http2_) {
      // TODO(davidben): Is this reachable? We shouldn't receive a SpdySession
      // if not requested.
      return ERR_NOT_IMPLEMENTED;
    }

    websocket_stream_ =
        delegate_->websocket_handshake_stream_create_helper()
            ->CreateHttp2Stream(session, std::move(dns_aliases));
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

  stream_ = std::make_unique<SpdyHttpStream>(session, net_log_.source(),
                                             std::move(dns_aliases));
  return OK;
}

int HttpStreamFactory::Job::DoCreateStream() {
  DCHECK(connection_->socket() || existing_spdy_session_.get());
  DCHECK(!using_quic_);

  next_state_ = STATE_CREATE_STREAM_COMPLETE;

  if (!using_spdy()) {
    DCHECK(!expect_spdy_);
    bool is_for_get_to_http_proxy = UsingHttpProxyWithoutTunnel();
    if (is_websocket_) {
      DCHECK_NE(job_type_, PRECONNECT);
      DCHECK_NE(job_type_, PRECONNECT_DNS_ALPN_H3);
      DCHECK(delegate_->websocket_handshake_stream_create_helper());
      websocket_stream_ =
          delegate_->websocket_handshake_stream_create_helper()
              ->CreateBasicStream(std::move(connection_),
                                  is_for_get_to_http_proxy,
                                  session_->websocket_endpoint_lock_manager());
    } else {
      if (!request_info_.is_http1_allowed) {
        return ERR_H2_OR_QUIC_REQUIRED;
      }
      stream_ = std::make_unique<HttpBasicStream>(std::move(connection_),
                                                  is_for_get_to_http_proxy);
    }
    return OK;
  }

  CHECK(!stream_.get());

  // It is also possible that an HTTP/2 connection has been established since
  // last time Job checked above.
  if (!existing_spdy_session_) {
    // WebSocket over HTTP/2 is only allowed to use existing HTTP/2 connections.
    // Therefore `using_spdy()` could not have been set unless a connection had
    // already been found.
    DCHECK(!is_websocket_);

    existing_spdy_session_ =
        session_->spdy_session_pool()->FindAvailableSession(
            spdy_session_key_, enable_ip_based_pooling_,
            /* is_websocket = */ false, net_log_);
  }
  if (existing_spdy_session_) {
    // We picked up an existing session, so we don't need our socket.
    if (connection_->socket()) {
      connection_->socket()->Disconnect();
    }
    connection_->Reset();

    int set_result =
        SetSpdyHttpStreamOrBidirectionalStreamImpl(existing_spdy_session_);
    existing_spdy_session_.reset();
    return set_result;
  }

  // Close idle sockets in this group, since subsequent requests will go over
  // |spdy_session|.
  if (connection_->socket()->IsConnected()) {
    connection_->CloseIdleSocketsInGroup("Switching to HTTP2 session");
  }

  base::WeakPtr<SpdySession> spdy_session;
  int rv =
      session_->spdy_session_pool()->CreateAvailableSessionFromSocketHandle(
          spdy_session_key_, std::move(connection_), net_log_, &spdy_session);

  if (rv != OK) {
    return rv;
  }

  url::SchemeHostPort scheme_host_port(
      using_ssl_ ? url::kHttpsScheme : url::kHttpScheme,
      spdy_session_key_.host_port_pair().host(),
      spdy_session_key_.host_port_pair().port());

  HttpServerProperties* http_server_properties =
      session_->http_server_properties();
  if (http_server_properties) {
    http_server_properties->SetSupportsSpdy(
        scheme_host_port, request_info_.network_anonymization_key,
        true /* supports_spdy */);
  }

  // Create a SpdyHttpStream or a BidirectionalStreamImpl attached to the
  // session.
  return SetSpdyHttpStreamOrBidirectionalStreamImpl(spdy_session);
}

int HttpStreamFactory::Job::DoCreateStreamComplete(int result) {
  if (result < 0) {
    return result;
  }

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
  if (connection_) {
    connection_->ResetAndCloseSocket();
  }

  // Once a connection is initialized, or if there's any out-of-band callback,
  // like proxy auth challenge, the SpdySessionRequest is cancelled.
  DCHECK(next_state_ == STATE_INIT_CONNECTION ||
         next_state_ == STATE_INIT_CONNECTION_COMPLETE);

  // Ignore calls to ResumeInitConnection() from either the timer or the
  // SpdySessionPool.
  init_connection_already_resumed_ = true;

  // If this is a preconnect, nothing left do to.
  if (job_type_ == PRECONNECT) {
    OnPreconnectsComplete(OK);
    return;
  }

  negotiated_protocol_ = kProtoHTTP2;
  existing_spdy_session_ = spdy_session;
  next_state_ = STATE_CREATE_STREAM;

  // This will synchronously close |connection_|, so no need to worry about it
  // calling back into |this|.
  RunLoop(OK);
}

int HttpStreamFactory::Job::ReconsiderProxyAfterError(int error) {
  // Check if the error was a proxy failure.
  if (!CanFalloverToNextProxy(proxy_info_.proxy_chain(), error, &error,
                              proxy_info_.is_for_ip_protection())) {
    return error;
  }

  should_reconsider_proxy_ = true;
  return error;
}

void HttpStreamFactory::Job::MaybeCopyConnectionAttemptsFromHandle() {
  if (!connection_) {
    return;
  }

  delegate_->AddConnectionAttemptsToRequest(this,
                                            connection_->connection_attempts());
}

HttpStreamFactory::JobFactory::JobFactory() = default;

HttpStreamFactory::JobFactory::~JobFactory() = default;

std::unique_ptr<HttpStreamFactory::Job>
HttpStreamFactory::JobFactory::CreateJob(
    HttpStreamFactory::Job::Delegate* delegate,
    HttpStreamFactory::JobType job_type,
    HttpNetworkSession* session,
    const StreamRequestInfo& request_info,
    RequestPriority priority,
    const ProxyInfo& proxy_info,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    url::SchemeHostPort destination,
    GURL origin_url,
    bool is_websocket,
    bool enable_ip_based_pooling,
    NetLog* net_log,
    NextProto alternative_protocol,
    quic::ParsedQuicVersion quic_version) {
  return std::make_unique<HttpStreamFactory::Job>(
      delegate, job_type, session, request_info, priority, proxy_info,
      allowed_bad_certs, std::move(destination), origin_url,
      alternative_protocol, quic_version, is_websocket, enable_ip_based_pooling,
      net_log);
}

bool HttpStreamFactory::Job::ShouldThrottleConnectForSpdy() const {
  DCHECK(!using_quic_);
  DCHECK(!spdy_session_request_);

  // If the job has previously been throttled, don't throttle it again.
  if (init_connection_already_resumed_) {
    return false;
  }

  url::SchemeHostPort scheme_host_port(
      using_ssl_ ? url::kHttpsScheme : url::kHttpScheme,
      spdy_session_key_.host_port_pair().host(),
      spdy_session_key_.host_port_pair().port());
  // Only throttle the request if the server is believed to support H2.
  return session_->http_server_properties()->GetSupportsSpdy(
      scheme_host_port, request_info_.network_anonymization_key);
}

}  // namespace net

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include <inttypes.h>

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_pool.h"
#include "net/http/url_security_manager.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool_manager_impl.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "url/scheme_host_port.h"

namespace net {

// The maximum receive window sizes for HTTP/2 sessions and streams.
const int32_t kSpdySessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int32_t kSpdyStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB

// Value of SETTINGS_ENABLE_PUSH reflecting that server push is not supported.
const uint32_t kSpdyDisablePush = 0;

namespace {

// Keep all HTTP2 parameters in |http2_settings|, even the ones that are not
// implemented, to be sent to the server.
// Set default values for settings that |http2_settings| does not specify.
spdy::SettingsMap AddDefaultHttp2Settings(spdy::SettingsMap http2_settings) {
  // Server push is not supported.
  http2_settings[spdy::SETTINGS_ENABLE_PUSH] = kSpdyDisablePush;

  // For other setting parameters, set default values only if |http2_settings|
  // does not have a value set for given setting.
  auto it = http2_settings.find(spdy::SETTINGS_HEADER_TABLE_SIZE);
  if (it == http2_settings.end()) {
    http2_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;
  }

  it = http2_settings.find(spdy::SETTINGS_INITIAL_WINDOW_SIZE);
  if (it == http2_settings.end()) {
    http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
        kSpdyStreamMaxRecvWindowSize;
  }

  it = http2_settings.find(spdy::SETTINGS_MAX_HEADER_LIST_SIZE);
  if (it == http2_settings.end()) {
    http2_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
        kSpdyMaxHeaderListSize;
  }

  return http2_settings;
}

bool OriginToForceQuicOnInternal(const QuicParams& quic_params,
                                 const url::SchemeHostPort& destination) {
  // TODO(crbug.com/40181080): Consider converting `origins_to_force_quic_on` to
  // use url::SchemeHostPort.
  return (
      base::Contains(quic_params.origins_to_force_quic_on, HostPortPair()) ||
      base::Contains(quic_params.origins_to_force_quic_on,
                     HostPortPair::FromSchemeHostPort(destination)));
}

}  // unnamed namespace

HttpNetworkSessionParams::HttpNetworkSessionParams()
    : spdy_session_max_recv_window_size(kSpdySessionMaxRecvWindowSize),
      spdy_session_max_queued_capped_frames(kSpdySessionMaxQueuedCappedFrames),
      time_func(&base::TimeTicks::Now) {
  enable_early_data =
      base::FeatureList::IsEnabled(features::kEnableTLS13EarlyData);
  use_dns_https_svcb_alpn =
      base::FeatureList::IsEnabled(features::kUseDnsHttpsSvcbAlpn);
}

HttpNetworkSessionParams::HttpNetworkSessionParams(
    const HttpNetworkSessionParams& other) = default;

HttpNetworkSessionParams::~HttpNetworkSessionParams() = default;

HttpNetworkSessionContext::HttpNetworkSessionContext()
    : client_socket_factory(nullptr),
      host_resolver(nullptr),
      cert_verifier(nullptr),
      transport_security_state(nullptr),
      sct_auditing_delegate(nullptr),
      proxy_resolution_service(nullptr),
      proxy_delegate(nullptr),
      http_user_agent_settings(nullptr),
      ssl_config_service(nullptr),
      http_auth_handler_factory(nullptr),
      net_log(nullptr),
      socket_performance_watcher_factory(nullptr),
      network_quality_estimator(nullptr),
      quic_context(nullptr),
#if BUILDFLAG(ENABLE_REPORTING)
      reporting_service(nullptr),
      network_error_logging_service(nullptr),
#endif
      quic_crypto_client_stream_factory(
          QuicCryptoClientStreamFactory::GetDefaultFactory()) {
}

HttpNetworkSessionContext::HttpNetworkSessionContext(
    const HttpNetworkSessionContext& other) = default;

HttpNetworkSessionContext::~HttpNetworkSessionContext() = default;

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(const HttpNetworkSessionParams& params,
                                       const HttpNetworkSessionContext& context)
    : net_log_(context.net_log),
      http_server_properties_(context.http_server_properties),
      cert_verifier_(context.cert_verifier),
      http_auth_handler_factory_(context.http_auth_handler_factory),
      host_resolver_(context.host_resolver),
#if BUILDFLAG(ENABLE_REPORTING)
      reporting_service_(context.reporting_service),
      network_error_logging_service_(context.network_error_logging_service),
#endif
      proxy_resolution_service_(context.proxy_resolution_service),
      ssl_config_service_(context.ssl_config_service),
      http_auth_cache_(
          params.key_auth_cache_server_entries_by_network_anonymization_key),
      ssl_client_session_cache_(SSLClientSessionCache::Config()),
      ssl_client_context_(context.ssl_config_service,
                          context.cert_verifier,
                          context.transport_security_state,
                          &ssl_client_session_cache_,
                          context.sct_auditing_delegate),
      quic_session_pool_(context.net_log,
                         context.host_resolver,
                         context.ssl_config_service,
                         context.client_socket_factory,
                         context.http_server_properties,
                         context.cert_verifier,
                         context.transport_security_state,
                         context.proxy_delegate,
                         context.sct_auditing_delegate,
                         context.socket_performance_watcher_factory,
                         context.quic_crypto_client_stream_factory,
                         context.quic_context),
      spdy_session_pool_(context.host_resolver,
                         &ssl_client_context_,
                         context.http_server_properties,
                         context.transport_security_state,
                         context.quic_context->params()->supported_versions,
                         params.enable_spdy_ping_based_connection_checking,
                         params.enable_http2,
                         params.enable_quic,
                         params.spdy_session_max_recv_window_size,
                         params.spdy_session_max_queued_capped_frames,
                         AddDefaultHttp2Settings(params.http2_settings),
                         params.enable_http2_settings_grease,
                         params.greased_http2_frame,
                         params.http2_end_stream_with_data_frame,
                         params.enable_priority_update,
                         params.spdy_go_away_on_ip_change,
                         params.time_func,
                         context.network_quality_estimator,
                         // cleanup_sessions_on_ip_address_changed
                         !params.ignore_ip_address_changes),
      http_stream_factory_(std::make_unique<HttpStreamFactory>(this)),
      params_(params),
      context_(context) {
  DCHECK(proxy_resolution_service_);
  DCHECK(ssl_config_service_);
  CHECK(http_server_properties_);
  DCHECK(context_.client_socket_factory);

  normal_socket_pool_manager_ = std::make_unique<ClientSocketPoolManagerImpl>(
      CreateCommonConnectJobParams(false /* for_websockets */),
      CreateCommonConnectJobParams(true /* for_websockets */),
      NORMAL_SOCKET_POOL,
      // cleanup_on_ip_address_change
      !params.ignore_ip_address_changes);
  websocket_socket_pool_manager_ =
      std::make_unique<ClientSocketPoolManagerImpl>(
          CreateCommonConnectJobParams(false /* for_websockets */),
          CreateCommonConnectJobParams(true /* for_websockets */),
          WEBSOCKET_SOCKET_POOL,
          // cleanup_on_ip_address_change
          !params.ignore_ip_address_changes);

  if (params_.enable_http2) {
    next_protos_.push_back(kProtoHTTP2);
    if (base::FeatureList::IsEnabled(features::kAlpsForHttp2)) {
      // Enable ALPS for HTTP/2 with empty data.
      application_settings_[kProtoHTTP2] = {};
    }
  }

  next_protos_.push_back(kProtoHTTP11);

  http_server_properties_->SetMaxServerConfigsStoredInProperties(
      context.quic_context->params()->max_server_configs_stored_in_properties);
  http_server_properties_->SetBrokenAlternativeServicesDelayParams(
      context.quic_context->params()
          ->initial_delay_for_broken_alternative_service,
      context.quic_context->params()->exponential_backoff_on_initial_delay);

  if (!params_.disable_idle_sockets_close_on_memory_pressure) {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&HttpNetworkSession::OnMemoryPressure,
                                       base::Unretained(this)));
  }

  if (base::FeatureList::IsEnabled(features::kHappyEyeballsV3)) {
    http_stream_pool_ = std::make_unique<HttpStreamPool>(
        this,
        /*cleanup_on_ip_address_change=*/!params.ignore_ip_address_changes);
  }
}

HttpNetworkSession::~HttpNetworkSession() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (http_stream_pool_) {
    http_stream_pool_->OnShuttingDown();
  }
  response_drainers_.clear();
  // TODO(bnc): CloseAllSessions() is also called in SpdySessionPool destructor,
  // one of the two calls should be removed.
  spdy_session_pool_.CloseAllSessions();
}

void HttpNetworkSession::StartResponseDrainer(
    std::unique_ptr<HttpResponseBodyDrainer> drainer) {
  DCHECK(!base::Contains(response_drainers_, drainer.get()));
  HttpResponseBodyDrainer* drainer_ptr = drainer.get();
  response_drainers_.insert(std::move(drainer));
  drainer_ptr->Start(this);
}

void HttpNetworkSession::RemoveResponseDrainer(
    HttpResponseBodyDrainer* drainer) {
  DCHECK(base::Contains(response_drainers_, drainer));

  response_drainers_.erase(response_drainers_.find(drainer));
}

ClientSocketPool* HttpNetworkSession::GetSocketPool(
    SocketPoolType pool_type,
    const ProxyChain& proxy_chain) {
  return GetSocketPoolManager(pool_type)->GetSocketPool(proxy_chain);
}

base::Value HttpNetworkSession::SocketPoolInfoToValue() const {
  // TODO(yutak): Should merge values from normal pools and WebSocket pools.
  return normal_socket_pool_manager_->SocketPoolInfoToValue();
}

std::unique_ptr<base::Value> HttpNetworkSession::SpdySessionPoolInfoToValue()
    const {
  return spdy_session_pool_.SpdySessionPoolInfoToValue();
}

base::Value HttpNetworkSession::QuicInfoToValue() const {
  base::Value::Dict dict;
  dict.Set("sessions", quic_session_pool_.QuicSessionPoolInfoToValue());
  dict.Set("quic_enabled", IsQuicEnabled());

  const QuicParams* quic_params = context_.quic_context->params();

  base::Value::List connection_options;
  for (const auto& option : quic_params->connection_options) {
    connection_options.Append(quic::QuicTagToString(option));
  }
  dict.Set("connection_options", std::move(connection_options));

  base::Value::List supported_versions;
  for (const auto& version : quic_params->supported_versions) {
    supported_versions.Append(ParsedQuicVersionToString(version));
  }
  dict.Set("supported_versions", std::move(supported_versions));

  base::Value::List origins_to_force_quic_on;
  for (const auto& origin : quic_params->origins_to_force_quic_on) {
    origins_to_force_quic_on.Append(origin.ToString());
  }
  dict.Set("origins_to_force_quic_on", std::move(origins_to_force_quic_on));

  dict.Set("max_packet_length",
           static_cast<int>(quic_params->max_packet_length));
  dict.Set(
      "max_server_configs_stored_in_properties",
      static_cast<int>(quic_params->max_server_configs_stored_in_properties));
  dict.Set("idle_connection_timeout_seconds",
           static_cast<int>(quic_params->idle_connection_timeout.InSeconds()));
  dict.Set("reduced_ping_timeout_seconds",
           static_cast<int>(quic_params->reduced_ping_timeout.InSeconds()));
  dict.Set("retry_without_alt_svc_on_quic_errors",
           quic_params->retry_without_alt_svc_on_quic_errors);
  dict.Set("close_sessions_on_ip_change",
           quic_params->close_sessions_on_ip_change);
  dict.Set("goaway_sessions_on_ip_change",
           quic_params->goaway_sessions_on_ip_change);
  dict.Set("migrate_sessions_on_network_change_v2",
           quic_params->migrate_sessions_on_network_change_v2);
  dict.Set("migrate_sessions_early_v2", quic_params->migrate_sessions_early_v2);
  dict.Set("retransmittable_on_wire_timeout_milliseconds",
           static_cast<int>(
               quic_params->retransmittable_on_wire_timeout.InMilliseconds()));
  dict.Set("retry_on_alternate_network_before_handshake",
           quic_params->retry_on_alternate_network_before_handshake);
  dict.Set("migrate_idle_sessions", quic_params->migrate_idle_sessions);
  dict.Set(
      "idle_session_migration_period_seconds",
      static_cast<int>(quic_params->idle_session_migration_period.InSeconds()));
  dict.Set("max_time_on_non_default_network_seconds",
           static_cast<int>(
               quic_params->max_time_on_non_default_network.InSeconds()));
  dict.Set("max_num_migrations_to_non_default_network_on_write_error",
           quic_params->max_migrations_to_non_default_network_on_write_error);
  dict.Set(
      "max_num_migrations_to_non_default_network_on_path_degrading",
      quic_params->max_migrations_to_non_default_network_on_path_degrading);
  dict.Set("allow_server_migration", quic_params->allow_server_migration);
  dict.Set("estimate_initial_rtt", quic_params->estimate_initial_rtt);
  dict.Set("initial_rtt_for_handshake_milliseconds",
           static_cast<int>(
               quic_params->initial_rtt_for_handshake.InMilliseconds()));

  return base::Value(std::move(dict));
}

void HttpNetworkSession::CloseAllConnections(int net_error,
                                             const char* net_log_reason_utf8) {
  normal_socket_pool_manager_->FlushSocketPoolsWithError(net_error,
                                                         net_log_reason_utf8);
  websocket_socket_pool_manager_->FlushSocketPoolsWithError(
      net_error, net_log_reason_utf8);
  if (http_stream_pool_) {
    http_stream_pool_->FlushWithError(net_error, net_log_reason_utf8);
  }
  spdy_session_pool_.CloseCurrentSessions(static_cast<Error>(net_error));
  quic_session_pool_.CloseAllSessions(net_error, quic::QUIC_PEER_GOING_AWAY);
}

void HttpNetworkSession::CloseIdleConnections(const char* net_log_reason_utf8) {
  normal_socket_pool_manager_->CloseIdleSockets(net_log_reason_utf8);
  websocket_socket_pool_manager_->CloseIdleSockets(net_log_reason_utf8);
  if (http_stream_pool_) {
    http_stream_pool_->CloseIdleStreams(net_log_reason_utf8);
  }
  spdy_session_pool_.CloseCurrentIdleSessions(net_log_reason_utf8);
}

bool HttpNetworkSession::IsQuicEnabled() const {
  return params_.enable_quic;
}

void HttpNetworkSession::DisableQuic() {
  params_.enable_quic = false;
}

bool HttpNetworkSession::ShouldForceQuic(const url::SchemeHostPort& destination,
                                         const ProxyInfo& proxy_info,
                                         bool is_websocket) {
  if (!IsQuicEnabled()) {
    return false;
  }
  if (is_websocket) {
    return false;
  }
  // If a proxy is being used, the last proxy in the chain must be QUIC if we
  // are to use QUIC on top of it.
  if (!proxy_info.is_direct() && !proxy_info.proxy_chain().Last().is_quic()) {
    return false;
  }
  return OriginToForceQuicOnInternal(*context_.quic_context->params(),
                                     destination) &&
         GURL::SchemeIsCryptographic(destination.scheme());
}

void HttpNetworkSession::IgnoreCertificateErrorsForTesting() {
  params_.ignore_certificate_errors = true;
}

void HttpNetworkSession::ClearSSLSessionCache() {
  ssl_client_session_cache_.Flush();
}

CommonConnectJobParams HttpNetworkSession::CreateCommonConnectJobParams(
    bool for_websockets) {
  // Use null websocket_endpoint_lock_manager, which is only set for WebSockets,
  // and only when not using a proxy.
  return CommonConnectJobParams(
      context_.client_socket_factory, context_.host_resolver, &http_auth_cache_,
      context_.http_auth_handler_factory, &spdy_session_pool_,
      &context_.quic_context->params()->supported_versions, &quic_session_pool_,
      context_.proxy_delegate, context_.http_user_agent_settings,
      &ssl_client_context_, context_.socket_performance_watcher_factory,
      context_.network_quality_estimator, context_.net_log,
      for_websockets ? &websocket_endpoint_lock_manager_ : nullptr,
      context_.http_server_properties, &next_protos_, &application_settings_,
      &params_.ignore_certificate_errors, &params_.enable_early_data);
}

ClientSocketPoolManager* HttpNetworkSession::GetSocketPoolManager(
    SocketPoolType pool_type) {
  switch (pool_type) {
    case NORMAL_SOCKET_POOL:
      return normal_socket_pool_manager_.get();
    case WEBSOCKET_SOCKET_POOL:
      return websocket_socket_pool_manager_.get();
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return nullptr;
}

void HttpNetworkSession::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DCHECK(!params_.disable_idle_sockets_close_on_memory_pressure);

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      CloseIdleConnections("Low memory");
      break;
  }
}

}  // namespace net

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session.h"

#include <inttypes.h>

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_body_drainer.h"
#include "net/http/http_stream_factory.h"
#include "net/http/url_security_manager.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_stream_factory.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_pool_manager_impl.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_tag.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"

namespace net {

// The maximum receive window sizes for HTTP/2 sessions and streams.
const int32_t kSpdySessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
const int32_t kSpdyStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB

namespace {

// Keep all HTTP2 parameters in |http2_settings|, even the ones that are not
// implemented, to be sent to the server.
// Set default values for settings that |http2_settings| does not specify.
spdy::SettingsMap AddDefaultHttp2Settings(spdy::SettingsMap http2_settings) {
  // Set default values only if |http2_settings| does not have
  // a value set for given setting.
  auto it = http2_settings.find(spdy::SETTINGS_HEADER_TABLE_SIZE);
  if (it == http2_settings.end())
    http2_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;

  it = http2_settings.find(spdy::SETTINGS_MAX_CONCURRENT_STREAMS);
  if (it == http2_settings.end())
    http2_settings[spdy::SETTINGS_MAX_CONCURRENT_STREAMS] =
        kSpdyMaxConcurrentPushedStreams;

  it = http2_settings.find(spdy::SETTINGS_INITIAL_WINDOW_SIZE);
  if (it == http2_settings.end())
    http2_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] =
        kSpdyStreamMaxRecvWindowSize;

  it = http2_settings.find(spdy::SETTINGS_MAX_HEADER_LIST_SIZE);
  if (it == http2_settings.end())
    http2_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
        kSpdyMaxHeaderListSize;

  return http2_settings;
}

}  // unnamed namespace

HttpNetworkSession::Params::Params()
    : enable_server_push_cancellation(false),
      ignore_certificate_errors(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      enable_user_alternate_protocol_ports(false),
      enable_spdy_ping_based_connection_checking(true),
      enable_http2(true),
      spdy_session_max_recv_window_size(kSpdySessionMaxRecvWindowSize),
      spdy_session_max_queued_capped_frames(kSpdySessionMaxQueuedCappedFrames),
      http2_end_stream_with_data_frame(false),
      time_func(&base::TimeTicks::Now),
      enable_http2_alternative_service(false),
      enable_websocket_over_http2(false),
      enable_quic(true),
      enable_quic_proxies_for_https_urls(false),
      disable_idle_sockets_close_on_memory_pressure(false),
      key_auth_cache_server_entries_by_network_isolation_key(false),
      enable_priority_update(false) {
  enable_early_data =
      base::FeatureList::IsEnabled(features::kEnableTLS13EarlyData);
}

HttpNetworkSession::Params::Params(const Params& other) = default;

HttpNetworkSession::Params::~Params() = default;

HttpNetworkSession::Context::Context()
    : client_socket_factory(nullptr),
      host_resolver(nullptr),
      cert_verifier(nullptr),
      transport_security_state(nullptr),
      ct_policy_enforcer(nullptr),
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

HttpNetworkSession::Context::Context(const Context& other) = default;

HttpNetworkSession::Context::~Context() = default;

// TODO(mbelshe): Move the socket factories into HttpStreamFactory.
HttpNetworkSession::HttpNetworkSession(const Params& params,
                                       const Context& context)
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
          params.key_auth_cache_server_entries_by_network_isolation_key),
      ssl_client_session_cache_(SSLClientSessionCache::Config()),
      ssl_client_context_(context.ssl_config_service,
                          context.cert_verifier,
                          context.transport_security_state,
                          context.ct_policy_enforcer,
                          &ssl_client_session_cache_,
                          context.sct_auditing_delegate),
      push_delegate_(nullptr),
      quic_stream_factory_(context.net_log,
                           context.host_resolver,
                           context.ssl_config_service,
                           context.client_socket_factory
                               ? context.client_socket_factory
                               : ClientSocketFactory::GetDefaultFactory(),
                           context.http_server_properties,
                           context.cert_verifier,
                           context.ct_policy_enforcer,
                           context.transport_security_state,
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
                         params.greased_http2_frame,
                         params.http2_end_stream_with_data_frame,
                         params.enable_priority_update,
                         params.time_func,
                         context.network_quality_estimator),
      http_stream_factory_(std::make_unique<HttpStreamFactory>(this)),
      params_(params),
      context_(context) {
  DCHECK(proxy_resolution_service_);
  DCHECK(ssl_config_service_);
  CHECK(http_server_properties_);

  normal_socket_pool_manager_ = std::make_unique<ClientSocketPoolManagerImpl>(
      CreateCommonConnectJobParams(false /* for_websockets */),
      CreateCommonConnectJobParams(true /* for_websockets */),
      NORMAL_SOCKET_POOL);
  websocket_socket_pool_manager_ =
      std::make_unique<ClientSocketPoolManagerImpl>(
          CreateCommonConnectJobParams(false /* for_websockets */),
          CreateCommonConnectJobParams(true /* for_websockets */),
          WEBSOCKET_SOCKET_POOL);

  if (params_.enable_http2)
    next_protos_.push_back(kProtoHTTP2);

  next_protos_.push_back(kProtoHTTP11);

  http_server_properties_->SetMaxServerConfigsStoredInProperties(
      context.quic_context->params()->max_server_configs_stored_in_properties);

  if (!params_.disable_idle_sockets_close_on_memory_pressure) {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(&HttpNetworkSession::OnMemoryPressure,
                                       base::Unretained(this)));
  }
}

HttpNetworkSession::~HttpNetworkSession() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  response_drainers_.clear();
  // TODO(bnc): CloseAllSessions() is also called in SpdySessionPool destructor,
  // one of the two calls should be removed.
  spdy_session_pool_.CloseAllSessions();
}

void HttpNetworkSession::AddResponseDrainer(
    std::unique_ptr<HttpResponseBodyDrainer> drainer) {
  DCHECK(!base::Contains(response_drainers_, drainer.get()));
  HttpResponseBodyDrainer* drainer_ptr = drainer.get();
  response_drainers_[drainer_ptr] = std::move(drainer);
}

void HttpNetworkSession::RemoveResponseDrainer(
    HttpResponseBodyDrainer* drainer) {
  DCHECK(base::Contains(response_drainers_, drainer));
  response_drainers_[drainer].release();
  response_drainers_.erase(drainer);
}

ClientSocketPool* HttpNetworkSession::GetSocketPool(
    SocketPoolType pool_type,
    const ProxyServer& proxy_server) {
  return GetSocketPoolManager(pool_type)->GetSocketPool(proxy_server);
}

std::unique_ptr<base::Value> HttpNetworkSession::SocketPoolInfoToValue() const {
  // TODO(yutak): Should merge values from normal pools and WebSocket pools.
  return normal_socket_pool_manager_->SocketPoolInfoToValue();
}

std::unique_ptr<base::Value> HttpNetworkSession::SpdySessionPoolInfoToValue()
    const {
  return spdy_session_pool_.SpdySessionPoolInfoToValue();
}

base::Value HttpNetworkSession::QuicInfoToValue() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("sessions",
              base::Value::FromUniquePtrValue(
                  quic_stream_factory_.QuicStreamFactoryInfoToValue()));
  dict.SetBoolKey("quic_enabled", IsQuicEnabled());

  const QuicParams* quic_params = context_.quic_context->params();

  base::Value connection_options(base::Value::Type::LIST);
  for (const auto& option : quic_params->connection_options)
    connection_options.Append(quic::QuicTagToString(option));
  dict.SetKey("connection_options", std::move(connection_options));

  base::Value supported_versions(base::Value::Type::LIST);
  for (const auto& version : quic_params->supported_versions)
    supported_versions.Append(ParsedQuicVersionToString(version));
  dict.SetKey("supported_versions", std::move(supported_versions));

  base::Value origins_to_force_quic_on(base::Value::Type::LIST);
  for (const auto& origin : quic_params->origins_to_force_quic_on)
    origins_to_force_quic_on.Append(origin.ToString());
  dict.SetKey("origins_to_force_quic_on", std::move(origins_to_force_quic_on));

  dict.SetIntKey("max_packet_length", quic_params->max_packet_length);
  dict.SetIntKey("max_server_configs_stored_in_properties",
                 quic_params->max_server_configs_stored_in_properties);
  dict.SetIntKey("idle_connection_timeout_seconds",
                 quic_params->idle_connection_timeout.InSeconds());
  dict.SetIntKey("reduced_ping_timeout_seconds",
                 quic_params->reduced_ping_timeout.InSeconds());
  dict.SetBoolKey("retry_without_alt_svc_on_quic_errors",
                  quic_params->retry_without_alt_svc_on_quic_errors);
  dict.SetBoolKey("disable_bidirectional_streams",
                  quic_params->disable_bidirectional_streams);
  dict.SetBoolKey("close_sessions_on_ip_change",
                  quic_params->close_sessions_on_ip_change);
  dict.SetBoolKey("goaway_sessions_on_ip_change",
                  quic_params->goaway_sessions_on_ip_change);
  dict.SetBoolKey("migrate_sessions_on_network_change_v2",
                  quic_params->migrate_sessions_on_network_change_v2);
  dict.SetBoolKey("migrate_sessions_early_v2",
                  quic_params->migrate_sessions_early_v2);
  dict.SetIntKey("retransmittable_on_wire_timeout_milliseconds",
                 quic_params->retransmittable_on_wire_timeout.InMilliseconds());
  dict.SetBoolKey("retry_on_alternate_network_before_handshake",
                  quic_params->retry_on_alternate_network_before_handshake);
  dict.SetBoolKey("migrate_idle_sessions", quic_params->migrate_idle_sessions);
  dict.SetIntKey("idle_session_migration_period_seconds",
                 quic_params->idle_session_migration_period.InSeconds());
  dict.SetIntKey("max_time_on_non_default_network_seconds",
                 quic_params->max_time_on_non_default_network.InSeconds());
  dict.SetIntKey(
      "max_num_migrations_to_non_default_network_on_write_error",
      quic_params->max_migrations_to_non_default_network_on_write_error);
  dict.SetIntKey(
      "max_num_migrations_to_non_default_network_on_path_degrading",
      quic_params->max_migrations_to_non_default_network_on_path_degrading);
  dict.SetBoolKey("allow_server_migration",
                  quic_params->allow_server_migration);
  dict.SetBoolKey("race_stale_dns_on_connection",
                  quic_params->race_stale_dns_on_connection);
  dict.SetBoolKey("go_away_on_path_degrading",
                  quic_params->go_away_on_path_degrading);
  dict.SetBoolKey("estimate_initial_rtt", quic_params->estimate_initial_rtt);
  dict.SetBoolKey("server_push_cancellation",
                  params_.enable_server_push_cancellation);
  dict.SetIntKey("initial_rtt_for_handshake_milliseconds",
                 quic_params->initial_rtt_for_handshake.InMilliseconds());

  return dict;
}

void HttpNetworkSession::CloseAllConnections(int net_error,
                                             const char* net_log_reason_utf8) {
  normal_socket_pool_manager_->FlushSocketPoolsWithError(net_error,
                                                         net_log_reason_utf8);
  websocket_socket_pool_manager_->FlushSocketPoolsWithError(
      net_error, net_log_reason_utf8);
  spdy_session_pool_.CloseCurrentSessions(static_cast<net::Error>(net_error));
  quic_stream_factory_.CloseAllSessions(net_error, quic::QUIC_PEER_GOING_AWAY);
}

void HttpNetworkSession::CloseIdleConnections(const char* net_log_reason_utf8) {
  normal_socket_pool_manager_->CloseIdleSockets(net_log_reason_utf8);
  websocket_socket_pool_manager_->CloseIdleSockets(net_log_reason_utf8);
  spdy_session_pool_.CloseCurrentIdleSessions(net_log_reason_utf8);
}

void HttpNetworkSession::SetServerPushDelegate(
    std::unique_ptr<ServerPushDelegate> push_delegate) {
  DCHECK(push_delegate);
  if (!params_.enable_server_push_cancellation || push_delegate_)
    return;

  push_delegate_ = std::move(push_delegate);
  spdy_session_pool_.set_server_push_delegate(push_delegate_.get());
  quic_stream_factory_.set_server_push_delegate(push_delegate_.get());
}

void HttpNetworkSession::GetAlpnProtos(NextProtoVector* alpn_protos) const {
  *alpn_protos = next_protos_;
}

void HttpNetworkSession::GetSSLConfig(SSLConfig* server_config,
                                      SSLConfig* proxy_config) const {
  GetAlpnProtos(&server_config->alpn_protos);
  server_config->ignore_certificate_errors = params_.ignore_certificate_errors;
  *proxy_config = *server_config;
  server_config->early_data_enabled = params_.enable_early_data;
}

void HttpNetworkSession::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_absolute_name) const {
  std::string name = base::StringPrintf("net/http_network_session_0x%" PRIxPTR,
                                        reinterpret_cast<uintptr_t>(this));
  base::trace_event::MemoryAllocatorDump* http_network_session_dump =
      pmd->GetAllocatorDump(name);
  if (http_network_session_dump == nullptr) {
    http_network_session_dump = pmd->CreateAllocatorDump(name);
    normal_socket_pool_manager_->DumpMemoryStats(
        pmd, http_network_session_dump->absolute_name());
    spdy_session_pool_.DumpMemoryStats(
        pmd, http_network_session_dump->absolute_name());
    if (http_stream_factory_) {
      http_stream_factory_->DumpMemoryStats(
          pmd, http_network_session_dump->absolute_name());
    }
    quic_stream_factory_.DumpMemoryStats(
        pmd, http_network_session_dump->absolute_name());
    ssl_client_session_cache_.DumpMemoryStats(pmd, name);
  }

  // Create an empty row under parent's dump so size can be attributed correctly
  // if |this| is shared between URLRequestContexts.
  base::trace_event::MemoryAllocatorDump* empty_row_dump =
      pmd->CreateAllocatorDump(base::StringPrintf(
          "%s/http_network_session", parent_absolute_name.c_str()));
  pmd->AddOwnershipEdge(empty_row_dump->guid(),
                        http_network_session_dump->guid());
}

bool HttpNetworkSession::IsQuicEnabled() const {
  return params_.enable_quic;
}

void HttpNetworkSession::DisableQuic() {
  params_.enable_quic = false;
}

void HttpNetworkSession::ClearSSLSessionCache() {
  ssl_client_session_cache_.Flush();
}

CommonConnectJobParams HttpNetworkSession::CreateCommonConnectJobParams(
    bool for_websockets) {
  // Use null websocket_endpoint_lock_manager, which is only set for WebSockets,
  // and only when not using a proxy.
  return CommonConnectJobParams(
      context_.client_socket_factory ? context_.client_socket_factory
                                     : ClientSocketFactory::GetDefaultFactory(),
      context_.host_resolver, &http_auth_cache_,
      context_.http_auth_handler_factory, &spdy_session_pool_,
      &context_.quic_context->params()->supported_versions,
      &quic_stream_factory_, context_.proxy_delegate,
      context_.http_user_agent_settings, &ssl_client_context_,
      context_.socket_performance_watcher_factory,
      context_.network_quality_estimator, context_.net_log,
      for_websockets ? &websocket_endpoint_lock_manager_ : nullptr);
}

ClientSocketPoolManager* HttpNetworkSession::GetSocketPoolManager(
    SocketPoolType pool_type) {
  switch (pool_type) {
    case NORMAL_SOCKET_POOL:
      return normal_socket_pool_manager_.get();
    case WEBSOCKET_SOCKET_POOL:
      return websocket_socket_pool_manager_.get();
    default:
      NOTREACHED();
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

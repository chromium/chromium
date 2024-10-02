// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/ip_endpoint.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket_handle.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/quiche/http2/hpack/hpack_constants.h"
#include "net/third_party/quiche/src/quiche/http2/hpack/hpack_static_table.h"

namespace net {

namespace {

enum SpdySessionGetTypes {
  CREATED_NEW                 = 0,
  FOUND_EXISTING              = 1,
  FOUND_EXISTING_FROM_IP_POOL = 2,
  IMPORTED_FROM_SOCKET        = 3,
  SPDY_SESSION_GET_MAX        = 4
};

}  // namespace

SpdySessionPool::SpdySessionRequest::Delegate::Delegate() = default;
SpdySessionPool::SpdySessionRequest::Delegate::~Delegate() = default;

SpdySessionPool::SpdySessionRequest::SpdySessionRequest(
    const SpdySessionKey& key,
    bool enable_ip_based_pooling,
    bool is_websocket,
    bool is_blocking_request_for_session,
    Delegate* delegate,
    SpdySessionPool* spdy_session_pool)
    : key_(key),
      enable_ip_based_pooling_(enable_ip_based_pooling),
      is_websocket_(is_websocket),
      is_blocking_request_for_session_(is_blocking_request_for_session),
      delegate_(delegate),
      spdy_session_pool_(spdy_session_pool) {}

SpdySessionPool::SpdySessionRequest::~SpdySessionRequest() {
  if (spdy_session_pool_)
    spdy_session_pool_->RemoveRequestForSpdySession(this);
}

void SpdySessionPool::SpdySessionRequest::OnRemovedFromPool() {
  DCHECK(spdy_session_pool_);
  spdy_session_pool_ = nullptr;
}

SpdySessionPool::SpdySessionPool(
    HostResolver* resolver,
    SSLClientContext* ssl_client_context,
    HttpServerProperties* http_server_properties,
    TransportSecurityState* transport_security_state,
    const quic::ParsedQuicVersionVector& quic_supported_versions,
    bool enable_ping_based_connection_checking,
    bool is_http2_enabled,
    bool is_quic_enabled,
    size_t session_max_recv_window_size,
    int session_max_queued_capped_frames,
    const spdy::SettingsMap& initial_settings,
    bool enable_http2_settings_grease,
    const std::optional<GreasedHttp2Frame>& greased_http2_frame,
    bool http2_end_stream_with_data_frame,
    bool enable_priority_update,
    bool go_away_on_ip_change,
    SpdySessionPool::TimeFunc time_func,
    NetworkQualityEstimator* network_quality_estimator,
    bool cleanup_sessions_on_ip_address_changed)
    : http_server_properties_(http_server_properties),
      transport_security_state_(transport_security_state),
      ssl_client_context_(ssl_client_context),
      resolver_(resolver),
      quic_supported_versions_(quic_supported_versions),
      enable_ping_based_connection_checking_(
          enable_ping_based_connection_checking),
      is_http2_enabled_(is_http2_enabled),
      is_quic_enabled_(is_quic_enabled),
      session_max_recv_window_size_(session_max_recv_window_size),
      session_max_queued_capped_frames_(session_max_queued_capped_frames),
      initial_settings_(initial_settings),
      enable_http2_settings_grease_(enable_http2_settings_grease),
      greased_http2_frame_(greased_http2_frame),
      http2_end_stream_with_data_frame_(http2_end_stream_with_data_frame),
      enable_priority_update_(enable_priority_update),
      go_away_on_ip_change_(go_away_on_ip_change),
      time_func_(time_func),
      network_quality_estimator_(network_quality_estimator),
      cleanup_sessions_on_ip_address_changed_(
          cleanup_sessions_on_ip_address_changed) {
  if (cleanup_sessions_on_ip_address_changed_)
    NetworkChangeNotifier::AddIPAddressObserver(this);
  if (ssl_client_context_)
    ssl_client_context_->AddObserver(this);
}

SpdySessionPool::~SpdySessionPool() {
#if DCHECK_IS_ON()
  for (const auto& request_info : spdy_session_request_map_) {
    // The should be no pending SpdySessionRequests on destruction, though there
    // may be callbacks waiting to be invoked, since they use weak pointers and
    // there's no API to unregister them.
    DCHECK(request_info.second.request_set.empty());
  }
#endif  // DCHECK_IS_ON()

  // TODO(bnc): CloseAllSessions() is also called in HttpNetworkSession
  // destructor, one of the two calls should be removed.
  CloseAllSessions();

  while (!sessions_.empty()) {
    // Destroy sessions to enforce that lifetime is scoped to SpdySessionPool.
    // Write callbacks queued upon session drain are not invoked.
    RemoveUnavailableSession((*sessions_.begin())->GetWeakPtr());
  }

  if (ssl_client_context_)
    ssl_client_context_->RemoveObserver(this);
  if (cleanup_sessions_on_ip_address_changed_)
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

int SpdySessionPool::CreateAvailableSessionFromSocketHandle(
    const SpdySessionKey& key,
    std::unique_ptr<StreamSocketHandle> stream_socket_handle,
    const NetLogWithSource& net_log,
    base::WeakPtr<SpdySession>* session) {
  TRACE_EVENT0(NetTracingCategory(),
               "SpdySessionPool::CreateAvailableSessionFromSocketHandle");

  std::unique_ptr<SpdySession> new_session =
      CreateSession(key, net_log.net_log());
  std::set<std::string> dns_aliases =
      stream_socket_handle->socket()->GetDnsAliases();

  new_session->InitializeWithSocketHandle(std::move(stream_socket_handle),
                                          this);

  base::expected<base::WeakPtr<SpdySession>, int> insert_result = InsertSession(
      key, std::move(new_session), net_log, std::move(dns_aliases),
      /*perform_post_insertion_checks=*/true);
  if (insert_result.has_value()) {
    *session = std::move(insert_result.value());
    return OK;
  }
  return insert_result.error();
}

base::expected<base::WeakPtr<SpdySession>, int>
SpdySessionPool::CreateAvailableSessionFromSocket(
    const SpdySessionKey& key,
    std::unique_ptr<StreamSocket> socket_stream,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(),
               "SpdySessionPool::CreateAvailableSessionFromSocket");

  std::unique_ptr<SpdySession> new_session =
      CreateSession(key, net_log.net_log());
  std::set<std::string> dns_aliases = socket_stream->GetDnsAliases();

  new_session->InitializeWithSocket(std::move(socket_stream), connect_timing,
                                    this);

  const bool perform_post_insertion_checks = base::FeatureList::IsEnabled(
      features::kSpdySessionForProxyAdditionalChecks);
  return InsertSession(key, std::move(new_session), net_log,
                       std::move(dns_aliases), perform_post_insertion_checks);
}

base::WeakPtr<SpdySession> SpdySessionPool::FindAvailableSession(
    const SpdySessionKey& key,
    bool enable_ip_based_pooling,
    bool is_websocket,
    const NetLogWithSource& net_log) {
  auto it = LookupAvailableSessionByKey(key);
  if (it == available_sessions_.end() ||
      (is_websocket && !it->second->support_websocket())) {
    return base::WeakPtr<SpdySession>();
  }

  if (key == it->second->spdy_session_key()) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet", FOUND_EXISTING,
                              SPDY_SESSION_GET_MAX);
    net_log.AddEventReferencingSource(
        NetLogEventType::HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION,
        it->second->net_log().source());
    return it->second;
  }

  if (enable_ip_based_pooling) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet", FOUND_EXISTING_FROM_IP_POOL,
                              SPDY_SESSION_GET_MAX);
    net_log.AddEventReferencingSource(
        NetLogEventType::HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL,
        it->second->net_log().source());
    return it->second;
  }

  return base::WeakPtr<SpdySession>();
}

base::WeakPtr<SpdySession>
SpdySessionPool::FindMatchingIpSessionForServiceEndpoint(
    const SpdySessionKey& key,
    const ServiceEndpoint& service_endpoint,
    const std::set<std::string>& dns_aliases) {
  CHECK(!HasAvailableSession(key, /*is_websocket=*/false));
  CHECK(key.socket_tag() == SocketTag());

  base::WeakPtr<SpdySession> session =
      FindMatchingIpSession(key, service_endpoint.ipv6_endpoints, dns_aliases);
  if (session) {
    return session;
  }
  return FindMatchingIpSession(key, service_endpoint.ipv4_endpoints,
                               dns_aliases);
}

bool SpdySessionPool::HasAvailableSession(const SpdySessionKey& key,
                                          bool is_websocket) const {
  const auto it = available_sessions_.find(key);
  return it != available_sessions_.end() &&
         (!is_websocket || it->second->support_websocket());
}

base::WeakPtr<SpdySession> SpdySessionPool::RequestSession(
    const SpdySessionKey& key,
    bool enable_ip_based_pooling,
    bool is_websocket,
    const NetLogWithSource& net_log,
    base::RepeatingClosure on_blocking_request_destroyed_callback,
    SpdySessionRequest::Delegate* delegate,
    std::unique_ptr<SpdySessionRequest>* spdy_session_request,
    bool* is_blocking_request_for_session) {
  DCHECK(delegate);

  base::WeakPtr<SpdySession> spdy_session =
      FindAvailableSession(key, enable_ip_based_pooling, is_websocket, net_log);
  if (spdy_session) {
    // This value doesn't really matter, but best to always populate it, for
    // consistency.
    *is_blocking_request_for_session = true;
    return spdy_session;
  }

  RequestInfoForKey* request_info = &spdy_session_request_map_[key];
  *is_blocking_request_for_session = !request_info->has_blocking_request;
  *spdy_session_request = std::make_unique<SpdySessionRequest>(
      key, enable_ip_based_pooling, is_websocket,
      *is_blocking_request_for_session, delegate, this);
  request_info->request_set.insert(spdy_session_request->get());

  if (*is_blocking_request_for_session) {
    request_info->has_blocking_request = true;
  } else if (on_blocking_request_destroyed_callback) {
    request_info->deferred_callbacks.push_back(
        on_blocking_request_destroyed_callback);
  }
  return nullptr;
}

OnHostResolutionCallbackResult SpdySessionPool::OnHostResolutionComplete(
    const SpdySessionKey& key,
    bool is_websocket,
    const std::vector<HostResolverEndpointResult>& endpoint_results,
    const std::set<std::string>& aliases) {
  // If there are no pending requests for that alias, nothing to do.
  if (spdy_session_request_map_.find(key) == spdy_session_request_map_.end())
    return OnHostResolutionCallbackResult::kContinue;

  // Check if there's already a matching session. If so, there may already
  // be a pending task to inform consumers of the alias. In this case, do
  // nothing, but inform the caller to wait for such a task to run.
  auto existing_session_it = LookupAvailableSessionByKey(key);
  if (existing_session_it != available_sessions_.end()) {
    if (is_websocket && !existing_session_it->second->support_websocket()) {
      // We don't look for aliased sessions because it would not be possible to
      // add them to the available_sessions_ map. See https://crbug.com/1220771.
      return OnHostResolutionCallbackResult::kContinue;
    }

    return OnHostResolutionCallbackResult::kMayBeDeletedAsync;
  }

  for (const auto& endpoint : endpoint_results) {
    // If `endpoint` has no associated ALPN protocols, it is TCP-based and thus
    // would have been eligible for connecting with HTTP/2.
    if (!endpoint.metadata.supported_protocol_alpns.empty() &&
        !base::Contains(endpoint.metadata.supported_protocol_alpns, "h2")) {
      continue;
    }
    for (const auto& address : endpoint.ip_endpoints) {
      auto range = aliases_.equal_range(address);
      for (auto alias_it = range.first; alias_it != range.second; ++alias_it) {
        // We found a potential alias.
        const SpdySessionKey& alias_key = alias_it->second;

        auto available_session_it = LookupAvailableSessionByKey(alias_key);
        // It shouldn't be in the aliases table if it doesn't exist!
        CHECK(available_session_it != available_sessions_.end(),
              base::NotFatalUntil::M130);

        SpdySessionKey::CompareForAliasingResult compare_result =
            alias_key.CompareForAliasing(key);
        // Keys must be aliasable.
        if (!compare_result.is_potentially_aliasable) {
          continue;
        }

        if (is_websocket &&
            !available_session_it->second->support_websocket()) {
          continue;
        }

        // Make copy of WeakPtr as call to UnmapKey() will delete original.
        const base::WeakPtr<SpdySession> available_session =
            available_session_it->second;

        // Need to verify that the server is authenticated to serve traffic for
        // |host_port_proxy_pair| too.
        if (!available_session->VerifyDomainAuthentication(
                key.host_port_pair().host())) {
          UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 0, 2);
          continue;
        }

        UMA_HISTOGRAM_ENUMERATION("Net.SpdyIPPoolDomainMatch", 1, 2);

        bool adding_pooled_alias = true;

        // If socket tags differ, see if session's socket tag can be changed.
        if (!compare_result.is_socket_tag_match) {
          SpdySessionKey old_key = available_session->spdy_session_key();
          SpdySessionKey new_key(
              old_key.host_port_pair(), old_key.privacy_mode(),
              old_key.proxy_chain(), old_key.session_usage(), key.socket_tag(),
              old_key.network_anonymization_key(), old_key.secure_dns_policy(),
              old_key.disable_cert_verification_network_fetches());

          // If there is already a session with |new_key|, skip this one.
          // It will be found in |aliases_| in a future iteration.
          if (available_sessions_.find(new_key) != available_sessions_.end()) {
            continue;
          }

          if (!available_session->ChangeSocketTag(key.socket_tag())) {
            continue;
          }

          DCHECK(available_session->spdy_session_key() == new_key);

          // If this isn't a pooled alias, but the actual session that needs to
          // have its socket tag change, there's no need to add an alias.
          if (new_key == key) {
            adding_pooled_alias = false;
          }

          // Remap main session key.
          std::set<std::string> main_session_old_dns_aliases =
              GetDnsAliasesForSessionKey(old_key);
          UnmapKey(old_key);
          MapKeyToAvailableSession(new_key, available_session,
                                   std::move(main_session_old_dns_aliases));

          // Remap alias. From this point on |alias_it| is invalid, so no more
          // iterations of the loop should be allowed.
          aliases_.insert(AliasMap::value_type(alias_it->first, new_key));
          aliases_.erase(alias_it);

          // Remap pooled session keys.
          const auto& pooled_aliases = available_session->pooled_aliases();
          for (auto it = pooled_aliases.begin(); it != pooled_aliases.end();) {
            // Ignore aliases this loop is inserting.
            if (it->socket_tag() == key.socket_tag()) {
              ++it;
              continue;
            }

            std::set<std::string> pooled_alias_old_dns_aliases =
                GetDnsAliasesForSessionKey(*it);
            UnmapKey(*it);
            SpdySessionKey new_pool_alias_key = SpdySessionKey(
                it->host_port_pair(), it->privacy_mode(), it->proxy_chain(),
                it->session_usage(), key.socket_tag(),
                it->network_anonymization_key(), it->secure_dns_policy(),
                it->disable_cert_verification_network_fetches());
            MapKeyToAvailableSession(new_pool_alias_key, available_session,
                                     std::move(pooled_alias_old_dns_aliases));
            auto old_it = it;
            ++it;
            available_session->RemovePooledAlias(*old_it);
            available_session->AddPooledAlias(new_pool_alias_key);

            // If this is desired key, no need to add an alias for the desired
            // key at the end of this method.
            if (new_pool_alias_key == key) {
              adding_pooled_alias = false;
            }
          }
        }

        if (adding_pooled_alias) {
          // Add this session to the map so that we can find it next time.
          MapKeyToAvailableSession(key, available_session, aliases);
          available_session->AddPooledAlias(key);
        }

        // Post task to inform pending requests for session for |key| that a
        // matching session is now available.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                                      weak_ptr_factory_.GetWeakPtr(), key));

        // Inform the caller that the Callback may be deleted if the consumer is
        // switched over to the newly aliased session. It's not guaranteed to be
        // deleted, as the session may be closed, or taken by yet another
        // pending request with a different SocketTag before the the request can
        // try and use the session.
        return OnHostResolutionCallbackResult::kMayBeDeletedAsync;
      }
    }
  }
  return OnHostResolutionCallbackResult::kContinue;
}

void SpdySessionPool::MakeSessionUnavailable(
    const base::WeakPtr<SpdySession>& available_session) {
  UnmapKey(available_session->spdy_session_key());
  RemoveAliases(available_session->spdy_session_key());
  const std::set<SpdySessionKey>& aliases = available_session->pooled_aliases();
  for (const auto& alias : aliases) {
    UnmapKey(alias);
    RemoveAliases(alias);
  }
  DCHECK(!IsSessionAvailable(available_session));
}

void SpdySessionPool::RemoveUnavailableSession(
    const base::WeakPtr<SpdySession>& unavailable_session) {
  DCHECK(!IsSessionAvailable(unavailable_session));

  unavailable_session->net_log().AddEvent(
      NetLogEventType::HTTP2_SESSION_POOL_REMOVE_SESSION);

  auto it = sessions_.find(unavailable_session.get());
  CHECK(it != sessions_.end());
  std::unique_ptr<SpdySession> owned_session(*it);
  sessions_.erase(it);
}

// Make a copy of |sessions_| in the Close* functions below to avoid
// reentrancy problems. Since arbitrary functions get called by close
// handlers, it doesn't suffice to simply increment the iterator
// before closing.

void SpdySessionPool::CloseCurrentSessions(Error error) {
  CloseCurrentSessionsHelper(error, "Closing current sessions.",
                             false /* idle_only */);
}

void SpdySessionPool::CloseCurrentIdleSessions(const std::string& description) {
  CloseCurrentSessionsHelper(ERR_ABORTED, description, true /* idle_only */);
}

void SpdySessionPool::CloseAllSessions() {
  auto is_draining = [](const SpdySession* s) { return s->IsDraining(); };
  // Repeat until every SpdySession owned by |this| is draining.
  while (!base::ranges::all_of(sessions_, is_draining)) {
    CloseCurrentSessionsHelper(ERR_ABORTED, "Closing all sessions.",
                               false /* idle_only */);
  }
}

void SpdySessionPool::MakeCurrentSessionsGoingAway(Error error) {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (base::WeakPtr<SpdySession>& session : current_sessions) {
    if (!session) {
      continue;
    }

    session->MakeUnavailable();
    session->StartGoingAway(kLastStreamId, error);
    session->MaybeFinishGoingAway();
    DCHECK(!IsSessionAvailable(session));
  }
}

std::unique_ptr<base::Value> SpdySessionPool::SpdySessionPoolInfoToValue()
    const {
  base::Value::List list;

  for (const auto& available_session : available_sessions_) {
    // Only add the session if the key in the map matches the main
    // host_port_proxy_pair (not an alias).
    const SpdySessionKey& key = available_session.first;
    const SpdySessionKey& session_key =
        available_session.second->spdy_session_key();
    if (key == session_key)
      list.Append(available_session.second->GetInfoAsValue());
  }
  return std::make_unique<base::Value>(std::move(list));
}

void SpdySessionPool::OnIPAddressChanged() {
  DCHECK(cleanup_sessions_on_ip_address_changed_);
  if (go_away_on_ip_change_) {
    MakeCurrentSessionsGoingAway(ERR_NETWORK_CHANGED);
  } else {
    CloseCurrentSessions(ERR_NETWORK_CHANGED);
  }
}

void SpdySessionPool::OnSSLConfigChanged(
    SSLClientContext::SSLConfigChangeType change_type) {
  switch (change_type) {
    case SSLClientContext::SSLConfigChangeType::kSSLConfigChanged:
      MakeCurrentSessionsGoingAway(ERR_NETWORK_CHANGED);
      break;
    case SSLClientContext::SSLConfigChangeType::kCertDatabaseChanged:
      MakeCurrentSessionsGoingAway(ERR_CERT_DATABASE_CHANGED);
      break;
    case SSLClientContext::SSLConfigChangeType::kCertVerifierChanged:
      MakeCurrentSessionsGoingAway(ERR_CERT_VERIFIER_CHANGED);
      break;
  };
}

void SpdySessionPool::OnSSLConfigForServersChanged(
    const base::flat_set<HostPortPair>& servers) {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (base::WeakPtr<SpdySession>& session : current_sessions) {
    bool session_matches = false;
    if (!session)
      continue;

    // If the destination for this session is invalidated, or any of the proxy
    // hops along the way, make the session go away.
    if (servers.contains(session->host_port_pair())) {
      session_matches = true;
    } else {
      const ProxyChain& proxy_chain = session->spdy_session_key().proxy_chain();

      for (const ProxyServer& proxy_server : proxy_chain.proxy_servers()) {
        if (proxy_server.is_http_like() && !proxy_server.is_http() &&
            servers.contains(proxy_server.host_port_pair())) {
          session_matches = true;
          break;
        }
      }
    }

    if (session_matches) {
      session->MakeUnavailable();
      // Note this call preserves active streams but fails any streams that are
      // waiting on a stream ID.
      // TODO(crbug.com/40768859): This is not ideal, but SpdySession
      // does not have a state that supports this.
      session->StartGoingAway(kLastStreamId, ERR_NETWORK_CHANGED);
      session->MaybeFinishGoingAway();
      DCHECK(!IsSessionAvailable(session));
    }
  }
}

std::set<std::string> SpdySessionPool::GetDnsAliasesForSessionKey(
    const SpdySessionKey& key) const {
  auto it = dns_aliases_by_session_key_.find(key);
  if (it == dns_aliases_by_session_key_.end())
    return {};

  return it->second;
}

void SpdySessionPool::RemoveRequestForSpdySession(SpdySessionRequest* request) {
  DCHECK_EQ(this, request->spdy_session_pool());

  auto iter = spdy_session_request_map_.find(request->key());
  CHECK(iter != spdy_session_request_map_.end(), base::NotFatalUntil::M130);

  // Resume all pending requests if it is the blocking request, which is either
  // being canceled, or has completed.
  if (request->is_blocking_request_for_session() &&
      !iter->second.deferred_callbacks.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                       weak_ptr_factory_.GetWeakPtr(), request->key()));
  }

  DCHECK(base::Contains(iter->second.request_set, request));
  RemoveRequestInternal(iter, iter->second.request_set.find(request));
}

SpdySessionPool::RequestInfoForKey::RequestInfoForKey() = default;
SpdySessionPool::RequestInfoForKey::~RequestInfoForKey() = default;

bool SpdySessionPool::IsSessionAvailable(
    const base::WeakPtr<SpdySession>& session) const {
  for (const auto& available_session : available_sessions_) {
    if (available_session.second.get() == session.get())
      return true;
  }
  return false;
}

void SpdySessionPool::MapKeyToAvailableSession(
    const SpdySessionKey& key,
    const base::WeakPtr<SpdySession>& session,
    std::set<std::string> dns_aliases) {
  DCHECK(base::Contains(sessions_, session.get()));
  std::pair<AvailableSessionMap::iterator, bool> result =
      available_sessions_.emplace(key, session);
  CHECK(result.second);

  dns_aliases_by_session_key_[key] = std::move(dns_aliases);
}

SpdySessionPool::AvailableSessionMap::iterator
SpdySessionPool::LookupAvailableSessionByKey(
    const SpdySessionKey& key) {
  return available_sessions_.find(key);
}

void SpdySessionPool::UnmapKey(const SpdySessionKey& key) {
  auto it = LookupAvailableSessionByKey(key);
  CHECK(it != available_sessions_.end());
  available_sessions_.erase(it);
  dns_aliases_by_session_key_.erase(key);
}

void SpdySessionPool::RemoveAliases(const SpdySessionKey& key) {
  // Walk the aliases map, find references to this pair.
  // TODO(mbelshe):  Figure out if this is too expensive.
  for (auto it = aliases_.begin(); it != aliases_.end();) {
    if (it->second == key) {
      auto old_it = it;
      ++it;
      aliases_.erase(old_it);
    } else {
      ++it;
    }
  }
}

SpdySessionPool::WeakSessionList SpdySessionPool::GetCurrentSessions() const {
  WeakSessionList current_sessions;
  for (SpdySession* session : sessions_) {
    current_sessions.push_back(session->GetWeakPtr());
  }
  return current_sessions;
}

void SpdySessionPool::CloseCurrentSessionsHelper(Error error,
                                                 const std::string& description,
                                                 bool idle_only) {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (base::WeakPtr<SpdySession>& session : current_sessions) {
    if (!session)
      continue;

    if (idle_only && session->is_active())
      continue;

    if (session->IsDraining())
      continue;

    session->CloseSessionOnError(error, description);

    DCHECK(!IsSessionAvailable(session));
    DCHECK(!session || session->IsDraining());
  }
}

std::unique_ptr<SpdySession> SpdySessionPool::CreateSession(
    const SpdySessionKey& key,
    NetLog* net_log) {
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionGet", IMPORTED_FROM_SOCKET,
                            SPDY_SESSION_GET_MAX);

  // If there's a pre-existing matching session, it has to be an alias. Remove
  // the alias.
  auto it = LookupAvailableSessionByKey(key);
  if (it != available_sessions_.end()) {
    DCHECK(key != it->second->spdy_session_key());

    // Remove session from available sessions and from aliases, and remove
    // key from the session's pooled alias set, so that a new session can be
    // created with this |key|.
    it->second->RemovePooledAlias(key);
    UnmapKey(key);
    RemoveAliases(key);
  }

  return std::make_unique<SpdySession>(
      key, http_server_properties_, transport_security_state_,
      ssl_client_context_ ? ssl_client_context_->ssl_config_service() : nullptr,
      quic_supported_versions_, enable_sending_initial_data_,
      enable_ping_based_connection_checking_, is_http2_enabled_,
      is_quic_enabled_, session_max_recv_window_size_,
      session_max_queued_capped_frames_, initial_settings_,
      enable_http2_settings_grease_, greased_http2_frame_,
      http2_end_stream_with_data_frame_, enable_priority_update_, time_func_,
      network_quality_estimator_, net_log);
}

base::expected<base::WeakPtr<SpdySession>, int> SpdySessionPool::InsertSession(
    const SpdySessionKey& key,
    std::unique_ptr<SpdySession> new_session,
    const NetLogWithSource& source_net_log,
    std::set<std::string> dns_aliases,
    bool perform_post_insertion_checks) {
  base::WeakPtr<SpdySession> available_session = new_session->GetWeakPtr();
  sessions_.insert(new_session.release());
  MapKeyToAvailableSession(key, available_session, std::move(dns_aliases));

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                                weak_ptr_factory_.GetWeakPtr(), key));

  source_net_log.AddEventReferencingSource(
      NetLogEventType::HTTP2_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET,
      available_session->net_log().source());

  // Look up the IP address for this session so that we can match
  // future sessions (potentially to different domains) which can
  // potentially be pooled with this one. Because GetPeerAddress()
  // reports the proxy's address instead of the origin server, check
  // to see if this is a direct connection.
  if (key.proxy_chain().is_direct()) {
    IPEndPoint address;
    if (available_session->GetPeerAddress(&address) == OK)
      aliases_.insert(AliasMap::value_type(address, key));
  }

  if (!perform_post_insertion_checks) {
    return available_session;
  }

  if (!available_session->HasAcceptableTransportSecurity()) {
    available_session->CloseSessionOnError(
        ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY, "");
    return base::unexpected(ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY);
  }

  int rv = available_session->ParseAlps();
  if (rv != OK) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    // ParseAlps() already closed the connection on error.
    return base::unexpected(rv);
  }

  return available_session;
}

void SpdySessionPool::UpdatePendingRequests(const SpdySessionKey& key) {
  auto it = LookupAvailableSessionByKey(key);
  if (it != available_sessions_.end()) {
    base::WeakPtr<SpdySession> new_session = it->second->GetWeakPtr();
    bool is_pooled = (key != new_session->spdy_session_key());
    while (new_session && new_session->IsAvailable()) {
      // Each iteration may empty out the RequestSet for |spdy_session_key| in
      // |spdy_session_request_map_|. So each time, check for RequestSet and use
      // the first one. Could just keep track if the last iteration removed the
      // final request, but it's possible that responding to one request will
      // result in cancelling another one.
      //
      // TODO(willchan): If it's important, switch RequestSet out for a FIFO
      // queue (Order by priority first, then FIFO within same priority).
      // Unclear that it matters here.
      auto iter = spdy_session_request_map_.find(key);
      if (iter == spdy_session_request_map_.end())
        break;
      RequestSet* request_set = &iter->second.request_set;
      // Find a request that can use the socket, if any.
      RequestSet::iterator request;
      for (request = request_set->begin(); request != request_set->end();
           ++request) {
        // If the request is for use with websockets, and the session doesn't
        // support websockets, skip over the request.
        if ((*request)->is_websocket() && !new_session->support_websocket())
          continue;
        // Don't use IP pooled session if not allowed.
        if (!(*request)->enable_ip_based_pooling() && is_pooled)
          continue;
        break;
      }
      if (request == request_set->end())
        break;

      SpdySessionRequest::Delegate* delegate = (*request)->delegate();
      RemoveRequestInternal(iter, request);
      delegate->OnSpdySessionAvailable(new_session);
    }
  }

  auto iter = spdy_session_request_map_.find(key);
  if (iter == spdy_session_request_map_.end())
    return;
  // Remove all pending requests, if there are any. As a result, if one of these
  // callbacks triggers a new RequestSession() call,
  // |is_blocking_request_for_session| will be true.
  std::list<base::RepeatingClosure> deferred_requests =
      std::move(iter->second.deferred_callbacks);

  // Delete the RequestMap if there are no SpdySessionRequests, and no deferred
  // requests.
  if (iter->second.request_set.empty())
    spdy_session_request_map_.erase(iter);

  // Resume any deferred requests. This needs to be after the
  // OnSpdySessionAvailable() calls, to prevent requests from calling into the
  // socket pools in cases where that's not necessary.
  for (auto callback : deferred_requests) {
    callback.Run();
  }
}

void SpdySessionPool::RemoveRequestInternal(
    SpdySessionRequestMap::iterator request_map_iterator,
    RequestSet::iterator request_set_iterator) {
  SpdySessionRequest* request = *request_set_iterator;
  request_map_iterator->second.request_set.erase(request_set_iterator);
  if (request->is_blocking_request_for_session()) {
    DCHECK(request_map_iterator->second.has_blocking_request);
    request_map_iterator->second.has_blocking_request = false;
  }

  // If both lists of requests are empty, can now remove the entry from the map.
  if (request_map_iterator->second.request_set.empty() &&
      request_map_iterator->second.deferred_callbacks.empty()) {
    spdy_session_request_map_.erase(request_map_iterator);
  }
  request->OnRemovedFromPool();
}

base::WeakPtr<SpdySession> SpdySessionPool::FindMatchingIpSession(
    const SpdySessionKey& key,
    const std::vector<IPEndPoint>& ip_endpoints,
    const std::set<std::string>& dns_aliases) {
  for (const auto& endpoint : ip_endpoints) {
    auto range = aliases_.equal_range(endpoint);
    for (auto alias_it = range.first; alias_it != range.second; ++alias_it) {
      // Found a potential alias.
      const SpdySessionKey& alias_key = alias_it->second;
      CHECK(alias_key.socket_tag() == SocketTag());

      auto available_session_it = LookupAvailableSessionByKey(alias_key);
      CHECK(available_session_it != available_sessions_.end());

      SpdySessionKey::CompareForAliasingResult compare_result =
          alias_key.CompareForAliasing(key);
      // Keys must be aliasable.
      if (!compare_result.is_potentially_aliasable) {
        continue;
      }

      base::WeakPtr<SpdySession> session = available_session_it->second;
      if (!session->VerifyDomainAuthentication(key.host_port_pair().host())) {
        continue;
      }

      // The found available session can be used for the IPEndpoint that was
      // resolved as an IP address to `key`.

      // Add the session to the available session map so that we can find it as
      // available for `key` next time.
      MapKeyToAvailableSession(key, session, dns_aliases);
      session->AddPooledAlias(key);

      return session;
    }
  }

  return nullptr;
}

}  // namespace net

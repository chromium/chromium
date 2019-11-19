// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_pool.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/trace_constants.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_source.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_huffman_table.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_static_table.h"

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
    const base::Optional<GreasedHttp2Frame>& greased_http2_frame,
    SpdySessionPool::TimeFunc time_func,
    NetworkQualityEstimator* network_quality_estimator)
    : http_server_properties_(http_server_properties),
      transport_security_state_(transport_security_state),
      ssl_client_context_(ssl_client_context),
      resolver_(resolver),
      quic_supported_versions_(quic_supported_versions),
      enable_sending_initial_data_(true),
      enable_ping_based_connection_checking_(
          enable_ping_based_connection_checking),
      is_http2_enabled_(is_http2_enabled),
      is_quic_enabled_(is_quic_enabled),
      session_max_recv_window_size_(session_max_recv_window_size),
      session_max_queued_capped_frames_(session_max_queued_capped_frames),
      initial_settings_(initial_settings),
      greased_http2_frame_(greased_http2_frame),
      time_func_(time_func),
      push_delegate_(nullptr),
      network_quality_estimator_(network_quality_estimator) {
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
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

base::WeakPtr<SpdySession>
SpdySessionPool::CreateAvailableSessionFromSocketHandle(
    const SpdySessionKey& key,
    bool is_trusted_proxy,
    std::unique_ptr<ClientSocketHandle> client_socket_handle,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(),
               "SpdySessionPool::CreateAvailableSessionFromSocketHandle");

  std::unique_ptr<SpdySession> new_session =
      CreateSession(key, is_trusted_proxy, net_log.net_log());
  new_session->InitializeWithSocketHandle(std::move(client_socket_handle),
                                          this);
  return InsertSession(key, std::move(new_session), net_log);
}

base::WeakPtr<SpdySession> SpdySessionPool::CreateAvailableSessionFromSocket(
    const SpdySessionKey& key,
    bool is_trusted_proxy,
    std::unique_ptr<StreamSocket> socket_stream,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    const NetLogWithSource& net_log) {
  TRACE_EVENT0(NetTracingCategory(),
               "SpdySessionPool::CreateAvailableSessionFromSocket");

  std::unique_ptr<SpdySession> new_session =
      CreateSession(key, is_trusted_proxy, net_log.net_log());

  new_session->InitializeWithSocket(std::move(socket_stream), connect_timing,
                                    this);

  return InsertSession(key, std::move(new_session), net_log);
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
    const AddressList& addresses) {
  // If there are no pending requests for that alias, nothing to do.
  if (spdy_session_request_map_.find(key) == spdy_session_request_map_.end())
    return OnHostResolutionCallbackResult::kContinue;

  // Check if there's already a matching session. If so, there may already
  // be a pending task to inform consumers of the alias. In this case, do
  // nothing, but inform the caller to wait for such a task to run.
  auto existing_session_it = LookupAvailableSessionByKey(key);
  if (existing_session_it != available_sessions_.end()) {
    // If this is an alias, the host resolution is for a websocket
    // connection, and the aliased session doesn't support websockets,
    // continue looking for an aliased session that does.  Unlikely there
    // is one, but can't hurt to check.
    bool continue_searching_for_websockets =
        is_websocket && !existing_session_it->second->support_websocket();

    if (!continue_searching_for_websockets)
      return OnHostResolutionCallbackResult::kMayBeDeletedAsync;
  }

  for (const auto& address : addresses) {
    auto range = aliases_.equal_range(address);
    for (auto alias_it = range.first; alias_it != range.second; ++alias_it) {
      // We found a potential alias.
      const SpdySessionKey& alias_key = alias_it->second;

      auto available_session_it = LookupAvailableSessionByKey(alias_key);
      // It shouldn't be in the aliases table if it doesn't exist!
      DCHECK(available_session_it != available_sessions_.end());

      // This session can be reused only if the proxy and privacy settings
      // match, as well as the NetworkIsolationKey.
      if (!(alias_key.proxy_server() == key.proxy_server()) ||
          !(alias_key.privacy_mode() == key.privacy_mode()) ||
          !(alias_key.is_proxy_session() == key.is_proxy_session()) ||
          !(alias_key.network_isolation_key() == key.network_isolation_key()) ||
          !(alias_key.disable_secure_dns() == key.disable_secure_dns())) {
        continue;
      }

      if (is_websocket && !available_session_it->second->support_websocket())
        continue;

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
      if (alias_key.socket_tag() != key.socket_tag()) {
        SpdySessionKey old_key = available_session->spdy_session_key();
        SpdySessionKey new_key(old_key.host_port_pair(), old_key.proxy_server(),
                               old_key.privacy_mode(),
                               old_key.is_proxy_session(), key.socket_tag(),
                               old_key.network_isolation_key(),
                               old_key.disable_secure_dns());

        // If there is already a session with |new_key|, skip this one.
        // It will be found in |aliases_| in a future iteration.
        if (available_sessions_.find(new_key) != available_sessions_.end())
          continue;

        if (!available_session->ChangeSocketTag(key.socket_tag()))
          continue;

        DCHECK(available_session->spdy_session_key() == new_key);

        // If this isn't a pooled alias, but the actual session that needs to
        // have its socket tag change, there's no need to add an alias.
        if (new_key == key)
          adding_pooled_alias = false;

        // Remap main session key.
        UnmapKey(old_key);
        MapKeyToAvailableSession(new_key, available_session);

        // Remap alias. From this point on |alias_it| is invalid, so no more
        // iterations of the loop should be allowed.
        aliases_.insert(AliasMap::value_type(alias_it->first, new_key));
        aliases_.erase(alias_it);

        // Remap pooled session keys.
        const auto& aliases = available_session->pooled_aliases();
        for (auto it = aliases.begin(); it != aliases.end();) {
          // Ignore aliases this loop is inserting.
          if (it->socket_tag() == key.socket_tag()) {
            ++it;
            continue;
          }

          UnmapKey(*it);
          SpdySessionKey new_pool_alias_key = SpdySessionKey(
              it->host_port_pair(), it->proxy_server(), it->privacy_mode(),
              it->is_proxy_session(), key.socket_tag(),
              it->network_isolation_key(), it->disable_secure_dns());
          MapKeyToAvailableSession(new_pool_alias_key, available_session);
          auto old_it = it;
          ++it;
          available_session->RemovePooledAlias(*old_it);
          available_session->AddPooledAlias(new_pool_alias_key);

          // If this is desired key, no need to add an alias for the desired key
          // at the end of this method.
          if (new_pool_alias_key == key)
            adding_pooled_alias = false;
        }
      }

      if (adding_pooled_alias) {
        // Add this session to the map so that we can find it next time.
        MapKeyToAvailableSession(key, available_session);
        available_session->AddPooledAlias(key);
      }

      // Post task to inform pending requests for session for |key| that a
      // matching session is now available.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                                    weak_ptr_factory_.GetWeakPtr(), key));

      // Inform the caller that the Callback may be deleted if the consumer is
      // switched over to the newly aliased session. It's not guaranteed to be
      // deleted, as the session may be closed, or taken by yet another pending
      // request with a different SocketTag before the the request can try and
      // use the session.
      return OnHostResolutionCallbackResult::kMayBeDeletedAsync;
    }
  }
  return OnHostResolutionCallbackResult::kContinue;
}

void SpdySessionPool::MakeSessionUnavailable(
    const base::WeakPtr<SpdySession>& available_session) {
  UnmapKey(available_session->spdy_session_key());
  RemoveAliases(available_session->spdy_session_key());
  const std::set<SpdySessionKey>& aliases = available_session->pooled_aliases();
  for (auto it = aliases.begin(); it != aliases.end(); ++it) {
    UnmapKey(*it);
    RemoveAliases(*it);
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

void SpdySessionPool::CloseCurrentIdleSessions() {
  CloseCurrentSessionsHelper(ERR_ABORTED, "Closing idle sessions.",
                             true /* idle_only */);
}

void SpdySessionPool::CloseAllSessions() {
  auto is_draining = [](const SpdySession* s) { return s->IsDraining(); };
  // Repeat until every SpdySession owned by |this| is draining.
  while (!std::all_of(sessions_.begin(), sessions_.end(), is_draining)) {
    CloseCurrentSessionsHelper(ERR_ABORTED, "Closing all sessions.",
                               false /* idle_only */);
  }
}

std::unique_ptr<base::Value> SpdySessionPool::SpdySessionPoolInfoToValue()
    const {
  auto list = std::make_unique<base::ListValue>();

  for (auto it = available_sessions_.begin(); it != available_sessions_.end();
       ++it) {
    // Only add the session if the key in the map matches the main
    // host_port_proxy_pair (not an alias).
    const SpdySessionKey& key = it->first;
    const SpdySessionKey& session_key = it->second->spdy_session_key();
    if (key == session_key)
      list->Append(it->second->GetInfoAsValue());
  }
  return std::move(list);
}

void SpdySessionPool::OnIPAddressChanged() {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (WeakSessionList::const_iterator it = current_sessions.begin();
       it != current_sessions.end(); ++it) {
    if (!*it)
      continue;

// For OSs that terminate TCP connections upon relevant network changes,
// attempt to preserve active streams by marking all sessions as going
// away, rather than explicitly closing them. Streams may still fail due
// to a generated TCP reset.
#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
    (*it)->MakeUnavailable();
    (*it)->StartGoingAway(kLastStreamId, ERR_NETWORK_CHANGED);
    (*it)->MaybeFinishGoingAway();
#else
    (*it)->CloseSessionOnError(ERR_NETWORK_CHANGED,
                               "Closing current sessions.");
    DCHECK((*it)->IsDraining());
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_IOS)
    DCHECK(!IsSessionAvailable(*it));
  }
}

void SpdySessionPool::OnSSLConfigChanged(bool is_cert_database_change) {
  CloseCurrentSessions(is_cert_database_change ? ERR_CERT_DATABASE_CHANGED
                                               : ERR_NETWORK_CHANGED);
}

void SpdySessionPool::OnSSLConfigForServerChanged(const HostPortPair& server) {
  WeakSessionList current_sessions = GetCurrentSessions();
  for (base::WeakPtr<SpdySession>& session : current_sessions) {
    if (!session)
      continue;

    const ProxyServer& proxy_server =
        session->spdy_session_key().proxy_server();
    if (session->host_port_pair() == server ||
        (proxy_server.is_http_like() && !proxy_server.is_http() &&
         proxy_server.host_port_pair() == server)) {
      session->MakeUnavailable();
      session->MaybeFinishGoingAway();
      DCHECK(!IsSessionAvailable(session));
    }
  }
}

void SpdySessionPool::RemoveRequestForSpdySession(SpdySessionRequest* request) {
  DCHECK_EQ(this, request->spdy_session_pool());

  auto iter = spdy_session_request_map_.find(request->key());
  DCHECK(iter != spdy_session_request_map_.end());

  // Resume all pending requests if it is the blocking request, which is either
  // being canceled, or has completed.
  if (request->is_blocking_request_for_session() &&
      !iter->second.deferred_callbacks.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpdySessionPool::UpdatePendingRequests,
                       weak_ptr_factory_.GetWeakPtr(), request->key()));
  }

  DCHECK(base::Contains(iter->second.request_set, request));
  RemoveRequestInternal(iter, iter->second.request_set.find(request));
}

void SpdySessionPool::DumpMemoryStats(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& parent_dump_absolute_name) const {
  if (sessions_.empty())
    return;
  size_t total_size = 0;
  size_t buffer_size = 0;
  size_t cert_count = 0;
  size_t cert_size = 0;
  size_t num_active_sessions = 0;
  for (auto* session : sessions_) {
    StreamSocket::SocketMemoryStats stats;
    bool is_session_active = false;
    total_size += session->DumpMemoryStats(&stats, &is_session_active);
    buffer_size += stats.buffer_size;
    cert_count += stats.cert_count;
    cert_size += stats.cert_size;
    if (is_session_active)
      num_active_sessions++;
  }
  total_size +=
      base::trace_event::EstimateMemoryUsage(spdy::ObtainHpackHuffmanTable()) +
      base::trace_event::EstimateMemoryUsage(spdy::ObtainHpackStaticTable()) +
      base::trace_event::EstimateMemoryUsage(push_promise_index_);
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(base::StringPrintf(
          "%s/spdy_session_pool", parent_dump_absolute_name.c_str()));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_size);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  sessions_.size());
  dump->AddScalar("active_session_count",
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  num_active_sessions);
  dump->AddScalar("buffer_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  buffer_size);
  dump->AddScalar("cert_count",
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  cert_count);
  dump->AddScalar("cert_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  cert_size);
}

SpdySessionPool::RequestInfoForKey::RequestInfoForKey() = default;
SpdySessionPool::RequestInfoForKey::~RequestInfoForKey() = default;

bool SpdySessionPool::IsSessionAvailable(
    const base::WeakPtr<SpdySession>& session) const {
  for (auto it = available_sessions_.begin(); it != available_sessions_.end();
       ++it) {
    if (it->second.get() == session.get())
      return true;
  }
  return false;
}

void SpdySessionPool::MapKeyToAvailableSession(
    const SpdySessionKey& key,
    const base::WeakPtr<SpdySession>& session) {
  DCHECK(base::Contains(sessions_, session.get()));
  std::pair<AvailableSessionMap::iterator, bool> result =
      available_sessions_.insert(std::make_pair(key, session));
  CHECK(result.second);
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
  for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
    current_sessions.push_back((*it)->GetWeakPtr());
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
    bool is_trusted_proxy,
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
      is_quic_enabled_, is_trusted_proxy, session_max_recv_window_size_,
      session_max_queued_capped_frames_, initial_settings_,
      greased_http2_frame_, time_func_, push_delegate_,
      network_quality_estimator_, net_log);
}

base::WeakPtr<SpdySession> SpdySessionPool::InsertSession(
    const SpdySessionKey& key,
    std::unique_ptr<SpdySession> new_session,
    const NetLogWithSource& source_net_log) {
  base::WeakPtr<SpdySession> available_session = new_session->GetWeakPtr();
  sessions_.insert(new_session.release());
  MapKeyToAvailableSession(key, available_session);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  if (key.proxy_server().is_direct()) {
    IPEndPoint address;
    if (available_session->GetPeerAddress(&address) == OK)
      aliases_.insert(AliasMap::value_type(address, key));
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

}  // namespace net

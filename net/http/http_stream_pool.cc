// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "url/gurl.h"

namespace net {

HttpStreamPool::SucceededStream::SucceededStream(
    std::unique_ptr<HttpStream> stream,
    NextProto negotiated_protocol)
    : stream(std::move(stream)), negotiated_protocol(negotiated_protocol) {}

HttpStreamPool::SucceededStream::SucceededStream(SucceededStream&&) = default;
HttpStreamPool::SucceededStream& HttpStreamPool::SucceededStream::operator=(
    SucceededStream&&) = default;

HttpStreamPool::SucceededStream::~SucceededStream() = default;

HttpStreamPool::HttpStreamPool(HttpNetworkSession* http_network_session,
                               bool cleanup_on_ip_address_change)
    : http_network_session_(http_network_session),
      cleanup_on_ip_address_change_(cleanup_on_ip_address_change) {
  CHECK(http_network_session_);
  if (cleanup_on_ip_address_change) {
    NetworkChangeNotifier::AddIPAddressObserver(this);
  }

  http_network_session_->ssl_client_context()->AddObserver(this);
}

HttpStreamPool::~HttpStreamPool() {
  http_network_session_->ssl_client_context()->RemoveObserver(this);

  if (cleanup_on_ip_address_change_) {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }
}

HttpStreamPool::StreamResult HttpStreamPool::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    const HttpStreamKey& stream_key,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    bool enable_ip_based_pooling,
    const NetLogWithSource& net_log) {
  SpdySessionKey spdy_session_key(
      HostPortPair::FromSchemeHostPort(stream_key.destination()),
      stream_key.privacy_mode(), ProxyChain(), SessionUsage::kDestination,
      stream_key.socket_tag(), stream_key.network_anonymization_key(),
      stream_key.secure_dns_policy(),
      stream_key.disable_cert_network_fetches());

  base::WeakPtr<SpdySession> spdy_session =
      http_network_session_->spdy_session_pool()->FindAvailableSession(
          spdy_session_key, enable_ip_based_pooling, /*is_websocket=*/false,
          net_log);
  if (spdy_session) {
    std::set<std::string> dns_aliases =
        http_network_session_->spdy_session_pool()->GetDnsAliasesForSessionKey(
            spdy_session_key);
    auto stream = std::make_unique<SpdyHttpStream>(
        spdy_session, net_log.source(), std::move(dns_aliases));
    return SucceededStream(std::move(stream), kProtoHTTP2);
  }

  Group& group = GetOrCreateGroup(stream_key, std::move(spdy_session_key));
  return group.RequestStream(delegate, priority, allowed_bad_certs,
                             enable_ip_based_pooling, net_log);
}

void HttpStreamPool::IncrementTotalIdleStreamCount() {
  CHECK_LT(TotalActiveStreamCount(), kMaxStreamSocketsPerPool);
  ++total_idle_stream_count_;
}

void HttpStreamPool::DecrementTotalIdleStreamCount() {
  CHECK_GT(total_idle_stream_count_, 0u);
  --total_idle_stream_count_;
}

void HttpStreamPool::IncrementTotalHandedOutStreamCount() {
  CHECK_LT(TotalActiveStreamCount(), kMaxStreamSocketsPerPool);
  ++total_handed_out_stream_count_;
}

void HttpStreamPool::DecrementTotalHandedOutStreamCount() {
  CHECK_GT(total_handed_out_stream_count_, 0u);
  --total_handed_out_stream_count_;
}

void HttpStreamPool::IncrementTotalConnectingStreamCount() {
  CHECK_LT(TotalActiveStreamCount(), kMaxStreamSocketsPerPool);
  ++total_connecting_stream_count_;
}

void HttpStreamPool::DecrementTotalConnectingStreamCount() {
  CHECK_GT(total_connecting_stream_count_, 0u);
  --total_connecting_stream_count_;
}

void HttpStreamPool::OnIPAddressChanged() {
  CHECK(cleanup_on_ip_address_change_);
  for (const auto& group : groups_) {
    group.second->Refresh();
    group.second->CancelRequests(ERR_NETWORK_CHANGED);
  }
}

void HttpStreamPool::OnSSLConfigChanged(
    SSLClientContext::SSLConfigChangeType change_type) {
  for (const auto& group : groups_) {
    group.second->Refresh();
  }
  ProcessPendingRequestsInGroups();
}

void HttpStreamPool::OnSSLConfigForServersChanged(
    const base::flat_set<HostPortPair>& servers) {
  for (const auto& group : groups_) {
    if (GURL::SchemeIsCryptographic(group.first.destination().scheme()) &&
        servers.contains(
            HostPortPair::FromSchemeHostPort(group.first.destination()))) {
      group.second->Refresh();
    }
  }
  ProcessPendingRequestsInGroups();
}

bool HttpStreamPool::IsPoolStalled() {
  if (!ReachedMaxStreamLimit()) {
    return false;
  }
  return FindHighestStalledGroup() != nullptr;
}

void HttpStreamPool::ProcessPendingRequestsInGroups() {
  // Loop until there is nothing more to do.
  while (true) {
    Group* group = FindHighestStalledGroup();
    if (!group) {
      return;
    }

    if (ReachedMaxStreamLimit()) {
      if (!CloseOneIdleStreamSocket()) {
        return;
      }
    }

    group->ProcessPendingRequest();
  }
}

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroupForTesting(
    const HttpStreamKey& stream_key) {
  SpdySessionKey spdy_session_key(
      HostPortPair::FromSchemeHostPort(stream_key.destination()),
      stream_key.privacy_mode(), ProxyChain(), SessionUsage::kDestination,
      stream_key.socket_tag(), stream_key.network_anonymization_key(),
      stream_key.secure_dns_policy(),
      stream_key.disable_cert_network_fetches());
  return GetOrCreateGroup(stream_key, std::move(spdy_session_key));
}

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroup(
    const HttpStreamKey& stream_key,
    SpdySessionKey spdy_session_key) {
  auto it = groups_.find(stream_key);
  if (it == groups_.end()) {
    it = groups_.try_emplace(
        it, stream_key,
        std::make_unique<Group>(this, stream_key, std::move(spdy_session_key)));
  }
  return *it->second;
}

HttpStreamPool::Group* HttpStreamPool::FindHighestStalledGroup() {
  Group* highest_stalled_group = nullptr;
  std::optional<RequestPriority> highest_priority;

  for (const auto& group : groups_) {
    std::optional<RequestPriority> priority =
        group.second->GetPriorityIfStalledByPoolLimit();
    if (!priority) {
      continue;
    }
    if (!highest_priority || *priority > *highest_priority) {
      highest_priority = priority;
      highest_stalled_group = group.second.get();
    }
  }

  return highest_stalled_group;
}

bool HttpStreamPool::CloseOneIdleStreamSocket() {
  if (total_idle_stream_count_ == 0) {
    return false;
  }

  for (auto& group : groups_) {
    if (group.second->CloseOneIdleStreamSocket()) {
      return true;
    }
  }
  NOTREACHED_NORETURN();
}

}  // namespace net

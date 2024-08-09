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
#include "net/base/completion_once_callback.h"
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

HttpStreamPool::HttpStreamPool(HttpNetworkSession* http_network_session,
                               bool cleanup_on_ip_address_change)
    : http_network_session_(http_network_session),
      stream_attempt_params_(
          StreamAttemptParams::FromHttpNetworkSession(http_network_session_)),
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

std::unique_ptr<HttpStreamRequest> HttpStreamPool::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    const HttpStreamKey& stream_key,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    quic::ParsedQuicVersion quic_version,
    const NetLogWithSource& net_log) {
  return GetOrCreateGroup(stream_key)
      .RequestStream(delegate, priority, allowed_bad_certs,
                     enable_ip_based_pooling, enable_alternative_services,
                     quic_version, net_log);
}

int HttpStreamPool::Preconnect(const HttpStreamKey& stream_key,
                               size_t num_streams,
                               quic::ParsedQuicVersion quic_version,
                               CompletionOnceCallback callback) {
  CHECK_GE(kMaxStreamSocketsPerGroup, num_streams);
  return GetOrCreateGroup(stream_key)
      .Preconnect(num_streams, quic_version, std::move(callback));
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

void HttpStreamPool::DecrementTotalConnectingStreamCount(size_t amount) {
  CHECK_GE(total_connecting_stream_count_, amount);
  total_connecting_stream_count_ -= amount;
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

void HttpStreamPool::OnGroupComplete(Group* group) {
  auto it = groups_.find(group->stream_key());
  CHECK(it != groups_.end());
  groups_.erase(it);
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
  return GetOrCreateGroup(stream_key);
}

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroup(
    const HttpStreamKey& stream_key) {
  auto it = groups_.find(stream_key);
  if (it == groups_.end()) {
    SpdySessionKey spdy_session_key = stream_key.ToSpdySessionKey();
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

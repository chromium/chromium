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
#include "base/task/sequenced_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "url/gurl.h"

namespace net {

// An implementation of HttpStreamRequest::Helper that is used to create a
// request when the pool can immediately provide an HttpStream from existing
// QUIC/SPDY sessions. This eliminates unnecessary creation/destruction of
// Group/Job when QUIC/SPDY sessions are already available.
class HttpStreamPool::PooledStreamRequestHelper
    : public HttpStreamRequest::Helper {
 public:
  PooledStreamRequestHelper(HttpStreamPool* pool,
                            std::unique_ptr<HttpStream> stream,
                            NextProto negotiated_protocol)
      : pool_(pool),
        stream_(std::move(stream)),
        negotiated_protocol_(negotiated_protocol) {}

  PooledStreamRequestHelper(const PooledStreamRequestHelper&) = delete;
  PooledStreamRequestHelper& operator=(const PooledStreamRequestHelper&) =
      delete;

  ~PooledStreamRequestHelper() override = default;

  std::unique_ptr<HttpStreamRequest> CreateRequest(
      HttpStreamRequest::Delegate* delegate,
      const NetLogWithSource& net_log) {
    CHECK(!delegate_);
    CHECK(delegate);
    delegate_ = delegate;

    auto request = std::make_unique<HttpStreamRequest>(
        this, /*websocket_handshake_stream_create_helper=*/nullptr, net_log,
        HttpStreamRequest::StreamType::HTTP_STREAM);

    request_ = request.get();

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PooledStreamRequestHelper::CallRequestComplete,
                       weak_ptr_factory_.GetWeakPtr()));
    return request;
  }

  // HttpStreamRequest::Helper methods:
  LoadState GetLoadState() const override { return LOAD_STATE_IDLE; }

  void OnRequestComplete() override {
    CHECK(request_);
    CHECK(delegate_);
    request_ = nullptr;
    delegate_ = nullptr;
    pool_->OnPooledStreamRequestComplete(this);
    // `this` is deleted.
  }

  int RestartTunnelWithProxyAuth() override { NOTREACHED_NORETURN(); }

  void SetPriority(RequestPriority priority) override { NOTREACHED_NORETURN(); }

 private:
  void CallRequestComplete() {
    CHECK(request_);
    CHECK(delegate_);
    request_->Complete(negotiated_protocol_,
                       ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON);
    ProxyInfo proxy_info;
    proxy_info.UseDirect();
    delegate_->OnStreamReady(proxy_info, std::move(stream_));
  }

  const raw_ptr<HttpStreamPool> pool_;

  std::unique_ptr<HttpStream> stream_;
  NextProto negotiated_protocol_;

  raw_ptr<HttpStreamRequest> request_;
  raw_ptr<HttpStreamRequest::Delegate> delegate_;

  base::WeakPtrFactory<PooledStreamRequestHelper> weak_ptr_factory_{this};
};

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
  QuicSessionKey quic_session_key = stream_key.ToQuicSessionKey();
  if (CanUseExistingQuicSession(stream_key, quic_session_key,
                                enable_ip_based_pooling,
                                enable_alternative_services)) {
    QuicChromiumClientSession* quic_session =
        http_network_session()->quic_session_pool()->FindExistingSession(
            quic_session_key, stream_key.destination());
    auto http_stream = std::make_unique<QuicHttpStream>(
        quic_session->CreateHandle(stream_key.destination()),
        quic_session->GetDnsAliasesForSessionKey(quic_session_key));
    return CreatePooledStreamRequest(delegate, std::move(http_stream),
                                     NextProto::kProtoQUIC, net_log);
  }

  SpdySessionKey spdy_session_key = stream_key.ToSpdySessionKey();
  base::WeakPtr<SpdySession> spdy_session =
      http_network_session()->spdy_session_pool()->FindAvailableSession(
          spdy_session_key, enable_ip_based_pooling, /*is_websocket=*/false,
          net_log);
  if (spdy_session) {
    auto http_stream = std::make_unique<SpdyHttpStream>(
        spdy_session, net_log.source(),
        http_network_session()->spdy_session_pool()->GetDnsAliasesForSessionKey(
            spdy_session_key));
    return CreatePooledStreamRequest(delegate, std::move(http_stream),
                                     NextProto::kProtoHTTP2, net_log);
  }

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
  QuicSessionKey quic_session_key = stream_key.ToQuicSessionKey();
  if (CanUseExistingQuicSession(stream_key, quic_session_key,
                                /*enable_ip_based_pooling=*/true,
                                /*enable_alternative_services=*/true)) {
    return OK;
  }
  SpdySessionKey spdy_session_key = stream_key.ToSpdySessionKey();
  if (http_network_session()->spdy_session_pool()->HasAvailableSession(
          spdy_session_key, /*is_websocket=*/false)) {
    return OK;
  }

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

bool HttpStreamPool::RequiresHTTP11(const HttpStreamKey& stream_key) {
  return http_network_session()->http_server_properties()->RequiresHTTP11(
      stream_key.destination(), stream_key.network_anonymization_key());
}

bool HttpStreamPool::CanUseQuic(const HttpStreamKey& stream_key,
                                bool enable_ip_based_pooling,
                                bool enable_alternative_services) {
  return enable_ip_based_pooling && enable_alternative_services &&
         GURL::SchemeIsCryptographic(stream_key.destination().scheme()) &&
         !RequiresHTTP11(stream_key);
}

bool HttpStreamPool::CanUseExistingQuicSession(
    const HttpStreamKey& stream_key,
    const QuicSessionKey& quic_session_key,
    bool enable_ip_based_pooling,
    bool enable_alternative_services) {
  return CanUseQuic(stream_key, enable_ip_based_pooling,
                    enable_alternative_services) &&
         http_network_session()->quic_session_pool()->CanUseExistingSession(
             quic_session_key, stream_key.destination());
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

std::unique_ptr<HttpStreamRequest> HttpStreamPool::CreatePooledStreamRequest(
    HttpStreamRequest::Delegate* delegate,
    std::unique_ptr<HttpStream> http_stream,
    NextProto negotiated_protocol,
    const NetLogWithSource& net_log) {
  auto helper = std::make_unique<PooledStreamRequestHelper>(
      this, std::move(http_stream), negotiated_protocol);
  PooledStreamRequestHelper* raw_helper = helper.get();
  pooled_stream_request_helpers_.emplace(std::move(helper));

  return raw_helper->CreateRequest(delegate, net_log);
}

void HttpStreamPool::OnPooledStreamRequestComplete(
    PooledStreamRequestHelper* helper) {
  auto it = pooled_stream_request_helpers_.find(helper);
  CHECK(it != pooled_stream_request_helpers_.end());
  pooled_stream_request_helpers_.erase(it);
}

}  // namespace net

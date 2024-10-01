// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/port_util.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/http/alternative_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_job_controller.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"
#include "url/gurl.h"

namespace net {

namespace {

constexpr base::FeatureParam<size_t> kHttpStreamPoolMaxStreamPerPool{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kMaxStreamSocketsPerPoolParamName.data(),
    HttpStreamPool::kDefaultMaxStreamSocketsPerPool};

constexpr base::FeatureParam<size_t> kHttpStreamPoolMaxStreamPerGroup{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kMaxStreamSocketsPerGroupParamName.data(),
    HttpStreamPool::kDefaultMaxStreamSocketsPerGroup};

}  // namespace

// An implementation of HttpStreamRequest::Helper that is used to create a
// request when the pool can immediately provide an HttpStream from existing
// QUIC/SPDY sessions. This eliminates unnecessary creation/destruction of
// Group/AttemptManager when QUIC/SPDY sessions are already available.
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

  int RestartTunnelWithProxyAuth() override { NOTREACHED(); }

  void SetPriority(RequestPriority priority) override {
    if (stream_) {
      stream_->SetPriority(priority);
    }
  }

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
      cleanup_on_ip_address_change_(cleanup_on_ip_address_change),
      max_stream_sockets_per_pool_(kHttpStreamPoolMaxStreamPerPool.Get()),
      // Ensure that the per-group limit is less than or equals to the per-pool
      // limit.
      max_stream_sockets_per_group_(
          std::min(kHttpStreamPoolMaxStreamPerPool.Get(),
                   kHttpStreamPoolMaxStreamPerGroup.Get())) {
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

void HttpStreamPool::OnShuttingDown() {
  is_shutting_down_ = true;
}

std::unique_ptr<HttpStreamRequest> HttpStreamPool::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    HttpStreamPoolSwitchingInfo switching_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    const NetLogWithSource& net_log) {
  CHECK(switching_info.proxy_info.is_direct());

  const HttpStreamKey& stream_key = switching_info.stream_key;
  if (delegate_for_testing_) {
    delegate_for_testing_->OnRequestStream(stream_key);
  }

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
  base::WeakPtr<SpdySession> spdy_session = FindAvailableSpdySession(
      stream_key, spdy_session_key, enable_ip_based_pooling, net_log);
  if (spdy_session) {
    auto http_stream = std::make_unique<SpdyHttpStream>(
        spdy_session, net_log.source(),
        http_network_session()->spdy_session_pool()->GetDnsAliasesForSessionKey(
            spdy_session_key));
    return CreatePooledStreamRequest(delegate, std::move(http_stream),
                                     NextProto::kProtoHTTP2, net_log);
  }

  auto controller = std::make_unique<JobController>(this);
  JobController* controller_raw_ptr = controller.get();
  // Put `controller` into `job_controllers_` before calling RequestStream() to
  // make sure `job_controllers_` always contains `controller` when
  // OnJobControllerComplete() is called.
  job_controllers_.emplace(std::move(controller));

  return controller_raw_ptr->RequestStream(
      delegate, std::move(switching_info), priority, allowed_bad_certs,
      enable_ip_based_pooling, enable_alternative_services, net_log);
}

int HttpStreamPool::Preconnect(HttpStreamPoolSwitchingInfo switching_info,
                               size_t num_streams,
                               CompletionOnceCallback callback) {
  num_streams = std::min(kDefaultMaxStreamSocketsPerGroup, num_streams);

  const HttpStreamKey& stream_key = switching_info.stream_key;
  if (!IsPortAllowedForScheme(stream_key.destination().port(),
                              stream_key.destination().scheme())) {
    return ERR_UNSAFE_PORT;
  }

  QuicSessionKey quic_session_key = stream_key.ToQuicSessionKey();
  if (CanUseExistingQuicSession(stream_key, quic_session_key,
                                /*enable_ip_based_pooling=*/true,
                                /*enable_alternative_services=*/true)) {
    return OK;
  }

  SpdySessionKey spdy_session_key = stream_key.ToSpdySessionKey();
  bool had_spdy_session =
      http_network_session()->spdy_session_pool()->HasAvailableSession(
          spdy_session_key, /*is_websocket=*/false);
  if (FindAvailableSpdySession(stream_key, spdy_session_key,
                               /*enable_ip_based_pooling=*/true)) {
    return OK;
  }
  if (had_spdy_session) {
    // We had a SPDY session but the server required HTTP/1.1. The session is
    // going away right now.
    return ERR_HTTP_1_1_REQUIRED;
  }

  if (delegate_for_testing_) {
    // Some tests expect OnPreconnect() is called after checking existing
    // sessions.
    std::optional<int> result =
        delegate_for_testing_->OnPreconnect(stream_key, num_streams);
    if (result.has_value()) {
      return *result;
    }
  }

  return GetOrCreateGroup(stream_key)
      .Preconnect(num_streams, switching_info.quic_version,
                  std::move(callback));
}

void HttpStreamPool::IncrementTotalIdleStreamCount() {
  CHECK_LT(TotalActiveStreamCount(), kDefaultMaxStreamSocketsPerPool);
  ++total_idle_stream_count_;
}

void HttpStreamPool::DecrementTotalIdleStreamCount() {
  CHECK_GT(total_idle_stream_count_, 0u);
  --total_idle_stream_count_;
}

void HttpStreamPool::IncrementTotalHandedOutStreamCount() {
  CHECK_LT(TotalActiveStreamCount(), kDefaultMaxStreamSocketsPerPool);
  ++total_handed_out_stream_count_;
}

void HttpStreamPool::DecrementTotalHandedOutStreamCount() {
  CHECK_GT(total_handed_out_stream_count_, 0u);
  --total_handed_out_stream_count_;
}

void HttpStreamPool::IncrementTotalConnectingStreamCount() {
  CHECK_LT(TotalActiveStreamCount(), kDefaultMaxStreamSocketsPerPool);
  ++total_connecting_stream_count_;
}

void HttpStreamPool::DecrementTotalConnectingStreamCount(size_t amount) {
  CHECK_GE(total_connecting_stream_count_, amount);
  total_connecting_stream_count_ -= amount;
}

void HttpStreamPool::OnIPAddressChanged() {
  CHECK(cleanup_on_ip_address_change_);
  for (const auto& group : groups_) {
    group.second->FlushWithError(ERR_NETWORK_CHANGED, kIpAddressChanged);
  }
}

void HttpStreamPool::OnSSLConfigChanged(
    SSLClientContext::SSLConfigChangeType change_type) {
  for (const auto& group : groups_) {
    group.second->Refresh(kSslConfigChanged);
  }
  ProcessPendingRequestsInGroups();
}

void HttpStreamPool::OnSSLConfigForServersChanged(
    const base::flat_set<HostPortPair>& servers) {
  for (const auto& group : groups_) {
    if (GURL::SchemeIsCryptographic(group.first.destination().scheme()) &&
        servers.contains(
            HostPortPair::FromSchemeHostPort(group.first.destination()))) {
      group.second->Refresh(kSslConfigChanged);
    }
  }
  ProcessPendingRequestsInGroups();
}

void HttpStreamPool::OnGroupComplete(Group* group) {
  auto it = groups_.find(group->stream_key());
  CHECK(it != groups_.end());
  groups_.erase(it);
}

void HttpStreamPool::OnJobControllerComplete(JobController* job_controller) {
  auto it = job_controllers_.find(job_controller);
  CHECK(it != job_controllers_.end());
  job_controllers_.erase(it);
}

void HttpStreamPool::FlushWithError(
    int error,
    std::string_view net_log_close_reason_utf8) {
  for (auto& group : groups_) {
    group.second->FlushWithError(error, net_log_close_reason_utf8);
  }
}

void HttpStreamPool::CloseIdleStreams(
    std::string_view net_log_close_reason_utf8) {
  for (auto& group : groups_) {
    group.second->CloseIdleStreams(net_log_close_reason_utf8);
  }
}

bool HttpStreamPool::IsPoolStalled() {
  if (!ReachedMaxStreamLimit()) {
    return false;
  }
  return FindHighestStalledGroup() != nullptr;
}

void HttpStreamPool::ProcessPendingRequestsInGroups() {
  if (is_shutting_down_) {
    return;
  }

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

bool HttpStreamPool::IsQuicBroken(const HttpStreamKey& stream_key) {
  return http_network_session()
      ->http_server_properties()
      ->IsAlternativeServiceBroken(
          AlternativeService(
              NextProto::kProtoQUIC,
              HostPortPair::FromSchemeHostPort(stream_key.destination())),
          stream_key.network_anonymization_key());
}

bool HttpStreamPool::CanUseQuic(const HttpStreamKey& stream_key,
                                bool enable_ip_based_pooling,
                                bool enable_alternative_services) {
  if (http_network_session()->ShouldForceQuic(stream_key.destination(),
                                              ProxyInfo::Direct(),
                                              /*is_websocket=*/false)) {
    return true;
  }
  return enable_ip_based_pooling && enable_alternative_services &&
         GURL::SchemeIsCryptographic(stream_key.destination().scheme()) &&
         !RequiresHTTP11(stream_key) && !IsQuicBroken(stream_key);
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

void HttpStreamPool::SetDelegateForTesting(
    std::unique_ptr<TestDelegate> delegate) {
  delegate_for_testing_ = std::move(delegate);
}

base::Value::Dict HttpStreamPool::GetInfoAsValue() const {
  // Using "socket" instead of "stream" for compatibility with ClientSocketPool.
  base::Value::Dict dict;
  dict.Set("handed_out_socket_count",
           static_cast<int>(total_handed_out_stream_count_));
  dict.Set("connecting_socket_count",
           static_cast<int>(total_connecting_stream_count_));
  dict.Set("idle_socket_count", static_cast<int>(total_idle_stream_count_));
  dict.Set("max_socket_count", static_cast<int>(max_stream_sockets_per_pool_));
  dict.Set("max_sockets_per_group",
           static_cast<int>(max_stream_sockets_per_group_));

  base::Value::Dict group_dicts;
  for (const auto& [key, group] : groups_) {
    group_dicts.Set(key.ToString(), group->GetInfoAsValue());
  }

  if (!group_dicts.empty()) {
    dict.Set("groups", std::move(group_dicts));
  }
  return dict;
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

HttpStreamPool::Group* HttpStreamPool::GetGroup(
    const HttpStreamKey& stream_key) {
  auto it = groups_.find(stream_key);
  return it == groups_.end() ? nullptr : it->second.get();
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
  NOTREACHED();
}

base::WeakPtr<SpdySession> HttpStreamPool::FindAvailableSpdySession(
    const HttpStreamKey& stream_key,
    const SpdySessionKey& spdy_session_key,
    bool enable_ip_based_pooling,
    const NetLogWithSource& net_log) {
  if (!GURL::SchemeIsCryptographic(stream_key.destination().scheme())) {
    return nullptr;
  }

  base::WeakPtr<SpdySession> spdy_session =
      http_network_session()->spdy_session_pool()->FindAvailableSession(
          spdy_session_key, enable_ip_based_pooling, /*is_websocket=*/false,
          net_log);
  if (spdy_session) {
    if (RequiresHTTP11(stream_key)) {
      spdy_session->MakeUnavailable();
      Group* group = GetGroup(stream_key);
      if (group) {
        group->OnRequiredHttp11();
      }
      return nullptr;
    }
  }
  return spdy_session;
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

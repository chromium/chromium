// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool.h"

#include <algorithm>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/session_usage.h"
#include "net/http/alternative_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_job_controller.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

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

constexpr base::FeatureParam<bool> kEnableConsistencyCheck{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kEnableConsistencyCheckParamName.data(), false};

// Represents total stream counts in the pool. Only used for consistency check.
struct StreamCounts {
  size_t handed_out = 0;
  size_t idle = 0;
  size_t connecting = 0;

  auto operator<=>(const StreamCounts&) const = default;

  base::Value::Dict ToValue() const {
    base::Value::Dict dict;
    dict.Set("handed_out", static_cast<int>(handed_out));
    dict.Set("idle", static_cast<int>(idle));
    dict.Set("connecting", static_cast<int>(connecting));
    return dict;
  }
};

std::ostream& operator<<(std::ostream& os, const StreamCounts& counts) {
  return os << "{ handed_out: " << counts.handed_out
            << ", idle: " << counts.idle
            << ", connecting: " << counts.connecting << " }";
}

}  // namespace

HttpStreamPool::HttpStreamPool(HttpNetworkSession* http_network_session,
                               bool cleanup_on_ip_address_change)
    : http_network_session_(http_network_session),
      stream_attempt_params_(
          StreamAttemptParams::FromHttpNetworkSession(http_network_session_)),
      cleanup_on_ip_address_change_(cleanup_on_ip_address_change),
      net_log_(NetLogWithSource::Make(http_network_session_->net_log(),
                                      NetLogSourceType::HTTP_STREAM_POOL)),
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

  if (kEnableConsistencyCheck.Get()) {
    CheckConsistency();
  }
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
  auto controller = std::make_unique<JobController>(this);
  JobController* controller_raw_ptr = controller.get();
  // SAFETY: Using base::Unretained() is safe because `this` will own
  // `controller` when Preconnect() return ERR_IO_PENDING.
  int rv = controller_raw_ptr->Preconnect(
      std::move(switching_info), num_streams,
      base::BindOnce(&HttpStreamPool::OnPreconnectComplete,
                     base::Unretained(this), controller_raw_ptr,
                     std::move(callback)));
  if (rv == ERR_IO_PENDING) {
    job_controllers_.emplace(std::move(controller));
  }
  return rv;
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

bool HttpStreamPool::RequiresHTTP11(
    const url::SchemeHostPort& destination,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return http_network_session()->http_server_properties()->RequiresHTTP11(
      destination, network_anonymization_key);
}

bool HttpStreamPool::IsQuicBroken(
    const url::SchemeHostPort& destination,
    const NetworkAnonymizationKey& network_anonymization_key) {
  return http_network_session()
      ->http_server_properties()
      ->IsAlternativeServiceBroken(
          AlternativeService(NextProto::kProtoQUIC,
                             HostPortPair::FromSchemeHostPort(destination)),
          network_anonymization_key);
}

bool HttpStreamPool::CanUseQuic(
    const url::SchemeHostPort& destination,
    const NetworkAnonymizationKey& network_anonymization_key,
    bool enable_ip_based_pooling,
    bool enable_alternative_services) {
  if (http_network_session()->ShouldForceQuic(destination, ProxyInfo::Direct(),
                                              /*is_websocket=*/false)) {
    return true;
  }
  return enable_ip_based_pooling && enable_alternative_services &&
         GURL::SchemeIsCryptographic(destination.scheme()) &&
         !RequiresHTTP11(destination, network_anonymization_key) &&
         !IsQuicBroken(destination, network_anonymization_key);
}

quic::ParsedQuicVersion HttpStreamPool::SelectQuicVersion(
    const AlternativeServiceInfo& alternative_service_info) {
  if (alternative_service_info.protocol() != NextProto::kProtoQUIC) {
    return quic::ParsedQuicVersion::Unsupported();
  }
  return http_network_session()->context().quic_context->SelectQuicVersion(
      alternative_service_info.advertised_versions());
}

bool HttpStreamPool::CanUseExistingQuicSession(
    const QuicSessionAliasKey& quic_session_alias_key,
    bool enable_ip_based_pooling,
    bool enable_alternative_services) {
  const url::SchemeHostPort& destination = quic_session_alias_key.destination();
  return destination.IsValid() &&
         CanUseQuic(
             destination,
             quic_session_alias_key.session_key().network_anonymization_key(),
             enable_ip_based_pooling, enable_alternative_services) &&
         http_network_session()->quic_session_pool()->CanUseExistingSession(
             quic_session_alias_key.session_key(), destination);
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
    const HttpStreamKey& stream_key,
    std::optional<QuicSessionAliasKey> quic_session_alias_key) {
  auto it = groups_.find(stream_key);
  if (it == groups_.end()) {
    it = groups_.try_emplace(
        it, stream_key,
        std::make_unique<Group>(this, stream_key, quic_session_alias_key));
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
    if (RequiresHTTP11(stream_key.destination(),
                       stream_key.network_anonymization_key())) {
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

void HttpStreamPool::OnPreconnectComplete(JobController* job_controller,
                                          CompletionOnceCallback callback,
                                          int rv) {
  OnJobControllerComplete(job_controller);
  std::move(callback).Run(rv);
}

void HttpStreamPool::CheckConsistency() {
  CHECK(kEnableConsistencyCheck.Get());

  const StreamCounts pool_total_counts = {
      .handed_out = total_handed_out_stream_count_,
      .idle = total_idle_stream_count_,
      .connecting = total_connecting_stream_count_};

  if (groups_.empty()) {
    VLOG_IF(1, pool_total_counts == StreamCounts())
        << "Total stream counts are not zero: " << pool_total_counts;
  } else {
    StreamCounts groups_total_counts;
    base::Value::Dict groups;
    for (const auto& [key, group] : groups_) {
      groups_total_counts.handed_out += group->HandedOutStreamSocketCount();
      groups_total_counts.idle += group->IdleStreamSocketCount();
      groups_total_counts.connecting += group->ConnectingStreamSocketCount();
      groups.Set(key.ToString(), group->GetInfoAsValue());
    }

    const bool ok = pool_total_counts == groups_total_counts;
    NetLogEventType event_type =
        ok ? NetLogEventType::HTTP_STREAM_POOL_CONSISTENCY_CHECK_OK
           : NetLogEventType::HTTP_STREAM_POOL_CONSISTENCY_CHECK_FAIL;
    net_log_.AddEvent(event_type, [&] {
      base::Value::Dict dict;
      dict.Set("pool_total_counts", pool_total_counts.ToValue());
      dict.Set("groups_total_counts", groups_total_counts.ToValue());
      dict.Set("groups", std::move(groups));
      return dict;
    });
    VLOG_IF(1, !ok) << "Stream counts mismatch: pool=" << pool_total_counts
                    << ", groups=" << groups_total_counts;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HttpStreamPool::CheckConsistency,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(3));
}

}  // namespace net

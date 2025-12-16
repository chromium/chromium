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
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/base/session_usage.h"
#include "net/base/task/task_runner.h"
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
#include "net/socket/stream_socket_close_reason.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// Specifies how to handle unexpected states.
// TODO(crbug.com/346835898): Remove this when we stabilize the implementation.
enum class CheckConsistencyMode {
  // Disable consistency checks.
  kDisabled = 0,
  // Logging only.
  kLogging = 1,
  // Use (D)CHECKs in addition to logging.
  kStrict = 2,
};

constexpr base::FeatureParam<size_t> kHttpStreamPoolMaxStreamPerPool{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kMaxStreamSocketsPerPoolParamName.data(),
    HttpStreamPool::kDefaultMaxStreamSocketsPerPool};

constexpr base::FeatureParam<size_t> kHttpStreamPoolMaxStreamPerGroup{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kMaxStreamSocketsPerGroupParamName.data(),
    HttpStreamPool::kDefaultMaxStreamSocketsPerGroup};

constexpr base::FeatureParam<base::TimeDelta>
    kHttpStreamPoolConnectionAttemptDelay{
        &features::kHappyEyeballsV3,
        HttpStreamPool::kConnectionAttemptDelayParamName.data(),
        HttpStreamPool::kDefaultConnectionAttemptDelay};

constexpr base::FeatureParam<bool> kEnablePriorityTaskRunner{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kEnablePriorityTaskRunnerParamName.data(), true};

constexpr base::FeatureParam<HttpStreamPool::TcpBasedAttemptDelayBehavior>
    kTcpBasedAttemptDelayBehavior{
        &features::kHappyEyeballsV3,
        HttpStreamPool::kTcpBasedAttemptDelayBehaviorParamName.data(),
        HttpStreamPool::TcpBasedAttemptDelayBehavior::
            kStartTimerOnFirstQuicAttempt,
        HttpStreamPool::kTcpBasedAttemptDelayBehaviorOptions};

constexpr base::FeatureParam<bool> kVerboseNetLog{
    &features::kHappyEyeballsV3, HttpStreamPool::kVerboseNetLogParamName.data(),
    false};

constexpr base::FeatureParam<CheckConsistencyMode>::Option
    kCheckConsistencyModeOptions[] = {
        {CheckConsistencyMode::kDisabled, "disabled"},
        {CheckConsistencyMode::kLogging, "logging"},
        {CheckConsistencyMode::kStrict, "strict"}};

constexpr base::FeatureParam<CheckConsistencyMode> kConsistencyCheck{
    &features::kHappyEyeballsV3,
    HttpStreamPool::kConsistencyCheckParamName.data(),
    CheckConsistencyMode::kDisabled, &kCheckConsistencyModeOptions};

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

// static
const scoped_refptr<base::SequencedTaskRunner> HttpStreamPool::TaskRunner(
    RequestPriority priority) {
  if (kEnablePriorityTaskRunner.Get()) {
    return GetTaskRunner(priority);
  }
  return base::SequencedTaskRunner::GetCurrentDefault();
}

// static
base::TimeDelta HttpStreamPool::GetConnectionAttemptDelay() {
  return kHttpStreamPoolConnectionAttemptDelay.Get();
}

// static
HttpStreamPool::TcpBasedAttemptDelayBehavior
HttpStreamPool::GetTcpBasedAttemptDelayBehavior() {
  return kTcpBasedAttemptDelayBehavior.Get();
}

// static
bool HttpStreamPool::VerboseNetLog() {
  return kVerboseNetLog.Get();
}

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

  if (kConsistencyCheck.Get() != CheckConsistencyMode::kDisabled) {
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

void HttpStreamPool::HandleStreamRequest(
    HttpStreamRequest* request,
    HttpStreamRequest::Delegate* delegate,
    HttpStreamPoolRequestInfo request_info,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    bool enable_ip_based_pooling_for_h2,
    bool enable_alternative_services) {
  auto controller = std::make_unique<JobController>(
      this, std::move(request_info), priority, allowed_bad_certs,
      enable_ip_based_pooling_for_h2, enable_alternative_services);
  JobController* controller_raw_ptr = controller.get();
  // Put `controller` into `job_controllers_` before calling HandleRequest() to
  // make sure `job_controllers_` always contains `controller` when
  // OnJobControllerComplete() is called.
  job_controllers_.emplace(std::move(controller));
  if (controller_raw_ptr->respect_limits() == RespectLimits::kIgnore) {
    ++limit_ignoring_job_controller_counts_;
  }

  controller_raw_ptr->HandleStreamRequest(request, delegate);
}

int HttpStreamPool::Preconnect(HttpStreamPoolRequestInfo request_info,
                               size_t num_streams,
                               CompletionOnceCallback callback) {
  auto controller = std::make_unique<JobController>(
      this, std::move(request_info), /*priority=*/RequestPriority::IDLE,
      /*allowed_bad_certs=*/std::vector<SSLConfig::CertAndStatus>(),
      /*enable_ip_based_pooling_for_h2=*/true,
      /*enable_alternative_services=*/true);
  JobController* controller_raw_ptr = controller.get();
  CHECK_EQ(controller_raw_ptr->respect_limits(), RespectLimits::kRespect);
  // SAFETY: Using base::Unretained() is safe because `this` owns `controller`.
  int rv = controller_raw_ptr->Preconnect(
      num_streams, base::BindOnce(&HttpStreamPool::OnPreconnectComplete,
                                  base::Unretained(this), controller_raw_ptr,
                                  std::move(callback)));
  // Preconnect() doesn't invoke the callback when it completes synchronously.
  // Put `controller` into `job_controllers_` only when the method doesn't
  // complete synchronously.
  if (rv == ERR_IO_PENDING) {
    job_controllers_.emplace(std::move(controller));
  }
  return rv;
}

bool HttpStreamPool::EnsureTotalActiveStreamCountBelowLimit() const {
  if (limit_ignoring_job_controller_counts_ > 0) {
    return true;
  }
  return TotalActiveStreamCount() < max_stream_sockets_per_pool_;
}

void HttpStreamPool::IncrementTotalIdleStreamCount() {
  CHECK(EnsureTotalActiveStreamCountBelowLimit());
  ++total_idle_stream_count_;
  TRACE_COUNTER("net.stream", "HttpStreamPoolTotalIdleStreams",
                total_idle_stream_count_);
}

void HttpStreamPool::DecrementTotalIdleStreamCount() {
  CHECK_GT(total_idle_stream_count_, 0u);
  --total_idle_stream_count_;
  TRACE_COUNTER("net.stream", "HttpStreamPoolTotalIdleStreams",
                total_idle_stream_count_);
}

void HttpStreamPool::IncrementTotalHandedOutStreamCount() {
  CHECK(EnsureTotalActiveStreamCountBelowLimit());
  ++total_handed_out_stream_count_;
  TRACE_COUNTER("net.stream", "HttpStreamPoolTotalHandedOutStreams",
                total_handed_out_stream_count_);
}

void HttpStreamPool::DecrementTotalHandedOutStreamCount() {
  CHECK_GT(total_handed_out_stream_count_, 0u);
  --total_handed_out_stream_count_;
  TRACE_COUNTER("net.stream", "HttpStreamPoolTotalHandedOutStreams",
                total_handed_out_stream_count_);
}

void HttpStreamPool::IncrementTotalConnectingStreamCount() {
  // TODO(crbug.com/383606724): Change this `if` to CHECK() once we stabilize
  // the implementation.
  if (!EnsureTotalActiveStreamCountBelowLimit()) {
    base::debug::Alias(&total_handed_out_stream_count_);
    base::debug::Alias(&total_idle_stream_count_);
    base::debug::Alias(&total_connecting_stream_count_);
    NOTREACHED() << "handed_out=" << total_handed_out_stream_count_
                 << ", idle=" << total_idle_stream_count_
                 << ", connecting=" << total_connecting_stream_count_
                 << ", limit=" << max_stream_sockets_per_pool_;
  }
  ++total_connecting_stream_count_;
  TRACE_COUNTER("net.stream", "HttpStreamPoolTotalConnectingStreams",
                total_connecting_stream_count_);
}

void HttpStreamPool::DecrementTotalConnectingStreamCount(size_t amount) {
  CHECK_GE(total_connecting_stream_count_, amount);
  total_connecting_stream_count_ -= amount;
  TRACE_COUNTER("net.stream", "HttpStreamPoolTotalConnectingStreams",
                total_connecting_stream_count_);
}

void HttpStreamPool::OnIPAddressChanged(
    NetworkChangeNotifier::IPAddressChangeType change_type) {
  CHECK(cleanup_on_ip_address_change_);

  // Ignore changes to randomly generated IPv6 temporary addresses.
  if (base::FeatureList::IsEnabled(
          net::features::kMaintainConnectionsOnIpv6TempAddrChange) &&
      change_type == NetworkChangeNotifier::IP_ADDRESS_CHANGE_IPV6_TEMPADDR) {
    return;
  }

  for (auto& group : groups_) {
    group.second.FlushWithError(ERR_NETWORK_CHANGED,
                                StreamSocketCloseReason::kIpAddressChanged,
                                kIpAddressChanged);
  }
}

void HttpStreamPool::OnSSLConfigChanged(
    SSLClientContext::SSLConfigChangeType change_type) {
  for (auto& group : groups_) {
    group.second.Refresh(kSslConfigChanged,
                         StreamSocketCloseReason::kSslConfigChanged);
  }
  ProcessPendingRequestsInGroups();
}

void HttpStreamPool::OnSSLConfigForServersChanged(
    const base::flat_set<HostPortPair>& servers) {
  for (auto& group : groups_) {
    if (GURL::SchemeIsCryptographic(group.first.destination().scheme()) &&
        servers.contains(
            HostPortPair::FromSchemeHostPort(group.first.destination()))) {
      group.second.Refresh(kSslConfigChanged,
                           StreamSocketCloseReason::kSslConfigChanged);
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
  if (job_controller->respect_limits() == RespectLimits::kIgnore) {
    CHECK_GT(limit_ignoring_job_controller_counts_, 0u);
    --limit_ignoring_job_controller_counts_;
  }
  auto it = job_controllers_.find(job_controller);
  CHECK(it != job_controllers_.end());
  job_controllers_.erase(it);
  CHECK_GE(job_controllers_.size(), limit_ignoring_job_controller_counts_);
}

void HttpStreamPool::FlushWithError(
    int error,
    StreamSocketCloseReason attempt_cancel_reason,
    std::string_view net_log_close_reason_utf8) {
  for (auto& group : groups_) {
    group.second.FlushWithError(error, attempt_cancel_reason,
                                net_log_close_reason_utf8);
  }
}

void HttpStreamPool::CloseIdleStreams(
    std::string_view net_log_close_reason_utf8) {
  for (auto& group : groups_) {
    group.second.CloseIdleStreams(net_log_close_reason_utf8);
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
    const NetworkAnonymizationKey& network_anonymization_key) const {
  return http_network_session()->http_server_properties()->RequiresHTTP11(
      destination, network_anonymization_key);
}

bool HttpStreamPool::IsQuicBroken(
    const url::SchemeHostPort& destination,
    const NetworkAnonymizationKey& network_anonymization_key) const {
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
    bool enable_alternative_services) const {
  if (http_network_session()->ShouldForceQuic(destination, ProxyInfo::Direct(),
                                              /*is_websocket=*/false)) {
    return true;
  }

  // Note that this does not check RequiresHTTP11(), as despite its name, it
  // only means H2 is not allowed.
  return http_network_session()->IsQuicEnabled() &&
         enable_alternative_services &&
         GURL::SchemeIsCryptographic(destination.scheme()) &&
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
    bool enable_alternative_services) {
  const url::SchemeHostPort& destination = quic_session_alias_key.destination();
  return destination.IsValid() &&
         CanUseQuic(
             destination,
             quic_session_alias_key.session_key().network_anonymization_key(),
             enable_alternative_services) &&
         http_network_session()->quic_session_pool()->CanUseExistingSession(
             quic_session_alias_key.session_key(), destination);
}

CompletionOnceCallback HttpStreamPool::GetAltSvcQuicPreconnectCallback() {
  if (alt_svc_quic_preconnect_callback_for_testing_) {
    return std::move(alt_svc_quic_preconnect_callback_for_testing_);
  }
  return base::DoNothing();
}

void HttpStreamPool::SetDelegateForTesting(
    std::unique_ptr<TestDelegate> delegate) {
  delegate_for_testing_ = std::move(delegate);
}

base::Value::Dict HttpStreamPool::GetInfoAsValue() const {
  // Using "socket" instead of "stream" for compatibility with ClientSocketPool.
  // These fields are used by some tests.
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
    group_dicts.Set(key.ToString(), group.GetInfoAsValue());
  }
  if (!group_dicts.empty()) {
    dict.Set("groups", std::move(group_dicts));
  }

  base::Value::List job_controller_list;
  for (const auto& job_controller : job_controllers_) {
    job_controller_list.Append(job_controller->GetInfoAsValue());
  }
  if (!job_controller_list.empty()) {
    dict.Set("job_controllers", std::move(job_controller_list));
  }

  return dict;
}

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroupForTesting(
    const HttpStreamKey& stream_key) {
  return GetOrCreateGroup(stream_key);
}

HttpStreamPool::Group* HttpStreamPool::GetGroupForTesting(
    const HttpStreamKey& stream_key) {
  return GetGroup(stream_key);
}

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroup(
    const HttpStreamKey& stream_key,
    const std::optional<QuicSessionAliasKey>& quic_session_alias_key) {
  auto [result, inserted] =
      groups_.try_emplace(stream_key, this, stream_key, quic_session_alias_key);
  return result->second;
}

HttpStreamPool::Group* HttpStreamPool::GetGroup(
    const HttpStreamKey& stream_key) {
  auto it = groups_.find(stream_key);
  return it == groups_.end() ? nullptr : &it->second;
}

HttpStreamPool::Group* HttpStreamPool::FindHighestStalledGroup() {
  Group* highest_stalled_group = nullptr;
  std::optional<RequestPriority> highest_priority;

  for (auto& group : groups_) {
    std::optional<RequestPriority> priority =
        group.second.GetPriorityIfStalledByPoolLimit();
    if (!priority) {
      continue;
    }
    if (!highest_priority || *priority > *highest_priority) {
      highest_priority = priority;
      highest_stalled_group = &group.second;
    }
  }

  return highest_stalled_group;
}

bool HttpStreamPool::CloseOneIdleStreamSocket() {
  if (total_idle_stream_count_ == 0) {
    return false;
  }

  for (auto& group : groups_) {
    if (group.second.CloseOneIdleStreamSocket()) {
      return true;
    }
  }
  NOTREACHED();
}

base::WeakPtr<SpdySession> HttpStreamPool::FindAvailableSpdySession(
    const HttpStreamKey& stream_key,
    const SpdySessionKey& spdy_session_key,
    bool enable_ip_based_pooling_for_h2,
    const NetLogWithSource& net_log) {
  // Only SSL origins may have H2 sessions.
  //
  // Also ignore any live H2 sessions for origins marked as requiring HTTP/1.1.
  // Ideally such sessions would not exist, but that is a difficult invariant to
  // enforce globally.
  if (!GURL::SchemeIsCryptographic(stream_key.destination().scheme()) ||
      RequiresHTTP11(stream_key.destination(),
                     stream_key.network_anonymization_key())) {
    return nullptr;
  }

  return http_network_session()->spdy_session_pool()->FindAvailableSession(
      spdy_session_key, enable_ip_based_pooling_for_h2,
      /*is_websocket=*/false, net_log);
}

void HttpStreamPool::OnPreconnectComplete(JobController* job_controller,
                                          CompletionOnceCallback callback,
                                          int rv) {
  OnJobControllerComplete(job_controller);
  if (callback) {
    std::move(callback).Run(rv);
  }
}

void HttpStreamPool::CheckConsistency() {
  CHECK(kConsistencyCheck.Get() != CheckConsistencyMode::kDisabled);
  const bool is_strict =
      kConsistencyCheck.Get() == CheckConsistencyMode::kStrict;

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
      groups_total_counts.handed_out += group.HandedOutStreamSocketCount();
      groups_total_counts.idle += group.IdleStreamSocketCount();
      groups_total_counts.connecting += group.ConnectingStreamSocketCount();
      groups.Set(key.ToString(), group.GetInfoAsValue());

      if (is_strict) {
        CHECK(!group.CanComplete()) << key.ToString();
      }
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

    if (is_strict) {
      CHECK(ok) << "Stream counts mismatch: pool=" << pool_total_counts
                << ", groups=" << groups_total_counts;
    } else {
      VLOG_IF(1, !ok) << "Stream counts mismatch: pool=" << pool_total_counts
                      << ", groups=" << groups_total_counts;
    }
  }

  TaskRunner(IDLE)->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HttpStreamPool::CheckConsistency,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(3));
}

}  // namespace net

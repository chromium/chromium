// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

bool IsNegotiatedProtocolTextBased(NextProto next_proto) {
  return next_proto == kProtoUnknown || next_proto == kProtoHTTP11;
}

void RecordNetLogClosingSocket(const StreamSocket& stream_socket,
                               std::string_view reason) {
  stream_socket.NetLog().AddEventWithStringParams(
      NetLogEventType::HTTP_STREAM_POOL_CLOSING_SOCKET, "reason", reason);
}

}  // namespace

// static
base::expected<void, std::string_view>
HttpStreamPool::Group::IsIdleStreamSocketUsable(const IdleStreamSocket& idle) {
  base::TimeDelta timeout = idle.stream_socket->WasEverUsed()
                                ? kUsedIdleStreamSocketTimeout
                                : kUnusedIdleStreamSocketTimeout;
  if (base::TimeTicks::Now() - idle.time_became_idle >= timeout) {
    return base::unexpected(kIdleTimeLimitExpired);
  }

  if (idle.stream_socket->WasEverUsed()) {
    if (idle.stream_socket->IsConnectedAndIdle()) {
      return base::ok();
    }
    if (idle.stream_socket->IsConnected()) {
      return base::unexpected(kDataReceivedUnexpectedly);
    } else {
      return base::unexpected(kRemoteSideClosedConnection);
    }
  }

  if (idle.stream_socket->IsConnected()) {
    return base::ok();
  }

  return base::unexpected(kRemoteSideClosedConnection);
}

HttpStreamPool::Group::IdleStreamSocket::IdleStreamSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    base::TimeTicks time_became_idle)
    : stream_socket(std::move(stream_socket)),
      time_became_idle(time_became_idle) {}

HttpStreamPool::Group::IdleStreamSocket::~IdleStreamSocket() = default;

HttpStreamPool::Group::Group(HttpStreamPool* pool,
                             HttpStreamKey stream_key,
                             SpdySessionKey spdy_session_key)
    : pool_(pool),
      stream_key_(std::move(stream_key)),
      spdy_session_key_(std::move(spdy_session_key)),
      quic_session_key_(stream_key_.ToQuicSessionKey()),
      net_log_(
          NetLogWithSource::Make(http_network_session()->net_log(),
                                 NetLogSourceType::HTTP_STREAM_POOL_GROUP)),
      force_quic_(
          http_network_session()->ShouldForceQuic(stream_key_.destination(),
                                                  ProxyInfo::Direct(),
                                                  /*is_websocket=*/false)) {
  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_ALIVE, [&] {
    base::Value::Dict dict;
    dict.Set("stream_key", stream_key_.ToValue());
    dict.Set("force_quic", force_quic_);
    return dict;
  });
}

HttpStreamPool::Group::~Group() {
  // TODO(crbug.com/346835898): Ensure `pool_`'s total active stream counts
  // are consistent.
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_ALIVE);
}

std::unique_ptr<HttpStreamPool::Job> HttpStreamPool::Group::CreateJob(
    Job::Delegate* delegate,
    NextProto expected_protocol,
    bool is_http1_allowed,
    ProxyInfo proxy_info) {
  EnsureAttemptManager();
  return std::make_unique<Job>(delegate, attempt_manager_.get(),
                               expected_protocol, is_http1_allowed,
                               std::move(proxy_info));
}

void HttpStreamPool::Group::StartJob(
    Job* job,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    RespectLimits respect_limits,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    quic::ParsedQuicVersion quic_version,
    const NetLogWithSource& net_log) {
  MaybeUpdateQuicVersionWhenForced(quic_version);
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_START_JOB, [&] {
        base::Value::Dict dict;
        dict.Set("priority", priority);
        base::Value::List allowed_bad_certs_list;
        for (const auto& cert_and_status : allowed_bad_certs) {
          allowed_bad_certs_list.Append(
              cert_and_status.cert->subject().GetDisplayName());
        }
        dict.Set("allowed_bad_certs", std::move(allowed_bad_certs_list));
        dict.Set("enable_ip_based_pooling", enable_ip_based_pooling);
        dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version));
        net_log.source().AddToEventParameters(dict);
        return dict;
      });
  net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_JOB_BOUND, net_log_.source());
  EnsureAttemptManager();
  attempt_manager_->StartJob(
      job, priority, allowed_bad_certs, respect_limits, enable_ip_based_pooling,
      enable_alternative_services, quic_version, net_log);
}

int HttpStreamPool::Group::Preconnect(size_t num_streams,
                                      quic::ParsedQuicVersion quic_version,
                                      CompletionOnceCallback callback) {
  MaybeUpdateQuicVersionWhenForced(quic_version);
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_PRECONNECT, [&] {
    base::Value::Dict dict;
    dict.Set("num_streams", static_cast<int>(num_streams));
    dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version));
    return dict;
  });

  if (ActiveStreamSocketCount() >= num_streams) {
    return OK;
  }

  EnsureAttemptManager();
  return attempt_manager_->Preconnect(num_streams, quic_version,
                                      std::move(callback));
}

std::unique_ptr<HttpStreamPoolHandle> HttpStreamPool::Group::CreateHandle(
    std::unique_ptr<StreamSocket> socket,
    StreamSocketHandle::SocketReuseType reuse_type,
    LoadTimingInfo::ConnectTiming connect_timing) {
  ++handed_out_stream_count_;
  pool_->IncrementTotalHandedOutStreamCount();

  auto handle = std::make_unique<HttpStreamPoolHandle>(
      weak_ptr_factory_.GetWeakPtr(), std::move(socket), generation_);
  handle->set_connect_timing(connect_timing);
  handle->set_reuse_type(reuse_type);
  return handle;
}

std::unique_ptr<HttpStream> HttpStreamPool::Group::CreateTextBasedStream(
    std::unique_ptr<StreamSocket> socket,
    StreamSocketHandle::SocketReuseType reuse_type,
    LoadTimingInfo::ConnectTiming connect_timing) {
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  return std::make_unique<HttpBasicStream>(
      CreateHandle(std::move(socket), reuse_type, std::move(connect_timing)),
      /*is_for_get_to_http_proxy=*/false);
}

void HttpStreamPool::Group::ReleaseStreamSocket(
    std::unique_ptr<StreamSocket> socket,
    int64_t generation) {
  CHECK_GT(handed_out_stream_count_, 0u);
  --handed_out_stream_count_;
  pool_->DecrementTotalHandedOutStreamCount();

  bool reusable = false;
  std::string_view not_reusable_reason;
  if (!socket->IsConnectedAndIdle()) {
    not_reusable_reason = socket->IsConnected()
                              ? kDataReceivedUnexpectedly
                              : kClosedConnectionReturnedToPool;
  } else if (generation != generation_) {
    not_reusable_reason = kSocketGenerationOutOfDate;
  } else {
    reusable = true;
  }

  if (reusable) {
    AddIdleStreamSocket(std::move(socket));
    ProcessPendingRequest();
  } else {
    RecordNetLogClosingSocket(*socket, not_reusable_reason);
    socket.reset();
  }

  pool_->ProcessPendingRequestsInGroups();
}

void HttpStreamPool::Group::AddIdleStreamSocket(
    std::unique_ptr<StreamSocket> socket) {
  CHECK(socket->IsConnectedAndIdle());
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_LE(ActiveStreamSocketCount(), pool_->max_stream_sockets_per_group());

  idle_stream_sockets_.emplace_back(std::move(socket), base::TimeTicks::Now());
  pool_->IncrementTotalIdleStreamCount();
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly, kIdleTimeLimitExpired);
}

std::unique_ptr<StreamSocket> HttpStreamPool::Group::GetIdleStreamSocket() {
  // Iterate through the idle streams from oldtest to newest and try to find a
  // used idle stream. Prefer the newest used idle stream.
  auto idle_it = idle_stream_sockets_.end();
  for (auto it = idle_stream_sockets_.begin();
       it != idle_stream_sockets_.end();) {
    const base::expected<void, std::string_view> usable_result =
        IsIdleStreamSocketUsable(*it);
    if (!usable_result.has_value()) {
      RecordNetLogClosingSocket(*it->stream_socket, usable_result.error());
      it = idle_stream_sockets_.erase(it);
      pool_->DecrementTotalIdleStreamCount();
      continue;
    }
    if (it->stream_socket->WasEverUsed()) {
      idle_it = it;
    }
    ++it;
  }

  if (idle_stream_sockets_.empty()) {
    return nullptr;
  }

  if (idle_it == idle_stream_sockets_.end()) {
    // There are no used idle streams. Pick the oldest (first) idle streams
    // (FIFO).
    idle_it = idle_stream_sockets_.begin();
  }

  CHECK(idle_it != idle_stream_sockets_.end());

  std::unique_ptr<StreamSocket> stream_socket =
      std::move(idle_it->stream_socket);
  idle_stream_sockets_.erase(idle_it);
  pool_->DecrementTotalIdleStreamCount();

  return stream_socket;
}

void HttpStreamPool::Group::ProcessPendingRequest() {
  if (!attempt_manager_) {
    return;
  }
  attempt_manager_->ProcessPendingJob();
}

bool HttpStreamPool::Group::CloseOneIdleStreamSocket() {
  if (idle_stream_sockets_.empty()) {
    return false;
  }

  idle_stream_sockets_.pop_front();
  pool_->DecrementTotalIdleStreamCount();
  return true;
}

size_t HttpStreamPool::Group::ActiveStreamSocketCount() const {
  return handed_out_stream_count_ + idle_stream_sockets_.size() +
         (attempt_manager_ ? attempt_manager_->InFlightAttemptCount() : 0);
}

bool HttpStreamPool::Group::ReachedMaxStreamLimit() const {
  return ActiveStreamSocketCount() >= pool_->max_stream_sockets_per_group();
}

std::optional<RequestPriority>
HttpStreamPool::Group::GetPriorityIfStalledByPoolLimit() const {
  if (!attempt_manager_) {
    return std::nullopt;
  }

  return attempt_manager_->IsStalledByPoolLimit()
             ? std::make_optional(attempt_manager_->GetPriority())
             : std::nullopt;
}

void HttpStreamPool::Group::FlushWithError(
    int error,
    std::string_view net_log_close_reason_utf8) {
  Refresh(net_log_close_reason_utf8);
  CancelJobs(error);
}

void HttpStreamPool::Group::Refresh(
    std::string_view net_log_close_reason_utf8) {
  ++generation_;
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
  if (attempt_manager_) {
    attempt_manager_->CancelInFlightAttempts();
  }
}

void HttpStreamPool::Group::CloseIdleStreams(
    std::string_view net_log_close_reason_utf8) {
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
}

void HttpStreamPool::Group::CancelJobs(int error) {
  if (attempt_manager_) {
    attempt_manager_->CancelJobs(error);
  }
}

void HttpStreamPool::Group::OnRequiredHttp11() {
  if (attempt_manager_) {
    attempt_manager_->OnRequiredHttp11();
  }
}

void HttpStreamPool::Group::OnAttemptManagerComplete() {
  CHECK(attempt_manager_);
  attempt_manager_.reset();
  MaybeComplete();
}

base::Value::Dict HttpStreamPool::Group::GetInfoAsValue() const {
  base::Value::Dict dict;
  dict.Set("active_socket_count", static_cast<int>(ActiveStreamSocketCount()));
  dict.Set("idle_socket_count", static_cast<int>(IdleStreamSocketCount()));
  if (attempt_manager_) {
    dict.Merge(attempt_manager_->GetInfoAsValue());
  }
  return dict;
}

void HttpStreamPool::Group::CleanupTimedoutIdleStreamSocketsForTesting() {
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly, "For testing");
}

void HttpStreamPool::Group::MaybeUpdateQuicVersionWhenForced(
    quic::ParsedQuicVersion& quic_version) {
  if (!quic_version.IsKnown() && force_quic_) {
    quic_version = http_network_session()
                       ->context()
                       .quic_context->params()
                       ->supported_versions[0];
  }
}

void HttpStreamPool::Group::CleanupIdleStreamSockets(
    CleanupMode mode,
    std::string_view net_log_close_reason_utf8) {
  // Iterate though the idle sockets to delete any disconnected ones.
  for (auto it = idle_stream_sockets_.begin();
       it != idle_stream_sockets_.end();) {
    bool should_delete = mode == CleanupMode::kForce;
    const base::expected<void, std::string_view> usable_result =
        IsIdleStreamSocketUsable(*it);
    if (!usable_result.has_value()) {
      should_delete = true;
    }

    if (should_delete) {
      RecordNetLogClosingSocket(*it->stream_socket, net_log_close_reason_utf8);
      it = idle_stream_sockets_.erase(it);
      pool_->DecrementTotalIdleStreamCount();
    } else {
      ++it;
    }
  }
}

void HttpStreamPool::Group::EnsureAttemptManager() {
  if (attempt_manager_) {
    return;
  }
  attempt_manager_ =
      std::make_unique<AttemptManager>(this, http_network_session()->net_log());
}

void HttpStreamPool::Group::MaybeComplete() {
  if (ActiveStreamSocketCount() > 0) {
    return;
  }

  pool_->OnGroupComplete(this);
  // `this` is deleted.
}

}  // namespace net

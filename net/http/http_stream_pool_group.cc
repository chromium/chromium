// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_id_helper.h"
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
#include "net/log/net_log_with_source.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

bool IsNegotiatedProtocolTextBased(NextProto next_proto) {
  return next_proto == NextProto::kProtoUnknown ||
         next_proto == NextProto::kProtoHTTP11;
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

HttpStreamPool::Group::Group(
    HttpStreamPool* pool,
    HttpStreamKey stream_key,
    std::optional<QuicSessionAliasKey> quic_session_alias_key)
    : pool_(pool),
      stream_key_(std::move(stream_key)),
      spdy_session_key_(stream_key_.CalculateSpdySessionKey()),
      quic_session_alias_key_(quic_session_alias_key.has_value()
                                  ? std::move(*quic_session_alias_key)
                                  : stream_key_.CalculateQuicSessionAliasKey()),
      net_log_(
          NetLogWithSource::Make(http_network_session()->net_log(),
                                 NetLogSourceType::HTTP_STREAM_POOL_GROUP)),
      force_quic_(
          http_network_session()->ShouldForceQuic(stream_key_.destination(),
                                                  ProxyInfo::Direct(),
                                                  /*is_websocket=*/false)),
      track_("HttpStreamPool::Group"),
      flow_(perfetto::Flow::ProcessScoped(
          base::trace_event::GetNextGlobalTraceId())) {
  TRACE_EVENT_INSTANT("net.stream", "Group::Group", track_, flow_,
                      "destination", stream_key_.destination().Serialize());
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
  TRACE_EVENT_INSTANT("net.stream", "Group::~Group", track_, flow_);
}

std::unique_ptr<HttpStreamPool::Job> HttpStreamPool::Group::CreateJob(
    Job::Delegate* delegate,
    quic::ParsedQuicVersion quic_version,
    NextProto expected_protocol,
    const NetLogWithSource& request_net_log) {
  return std::make_unique<Job>(delegate, JobType::kRequest, this, quic_version,
                               expected_protocol, request_net_log);
}

std::unique_ptr<HttpStreamPoolHandle> HttpStreamPool::Group::CreateHandle(
    std::unique_ptr<StreamSocket> socket,
    StreamSocketHandle::SocketReuseType reuse_type,
    LoadTimingInfo::ConnectTiming connect_timing) {
  ++handed_out_stream_count_;
  pool_->IncrementTotalHandedOutStreamCount();

  TRACE_EVENT_INSTANT("net.stream", "Group::CreateHandle", track_, flow_,
                      "negotiated_protocol", socket->GetNegotiatedProtocol(),
                      "handed_out_stream_count", handed_out_stream_count_);
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_HANDLE_CREATED,
                    [&] {
                      base::Value::Dict dict;
                      socket->NetLog().source().AddToEventParameters(dict);
                      dict.Set("reuse_type", static_cast<int>(reuse_type));
                      return dict;
                    });

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
  } else if (ReachedMaxStreamLimit() || pool_->ReachedMaxStreamLimit()) {
    not_reusable_reason = kExceededSocketLimits;
  } else {
    reusable = true;
  }

  TRACE_EVENT_INSTANT("net.stream", "Group::ReleaseStreamSocket", track_, flow_,
                      "reusable", reusable, "handed_out_stream_count",
                      handed_out_stream_count_);

  if (reusable) {
    AddIdleStreamSocket(std::move(socket));
  } else {
    RecordNetLogClosingSocket(*socket, not_reusable_reason);
    socket.reset();
  }

  pool_->ProcessPendingRequestsInGroups();
  MaybeComplete();
}

void HttpStreamPool::Group::AddIdleStreamSocket(
    std::unique_ptr<StreamSocket> socket) {
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_LE(ActiveStreamSocketCount(), pool_->max_stream_sockets_per_group());

  idle_stream_sockets_.emplace_back(std::move(socket), base::TimeTicks::Now());
  pool_->IncrementTotalIdleStreamCount();
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly, kIdleTimeLimitExpired);

  TRACE_EVENT_INSTANT("net.stream", "Group::AddIdleStreamSocket", track_, flow_,
                      "idle_stream_count", idle_stream_sockets_.size());

  ProcessPendingRequest();
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

  TRACE_EVENT_INSTANT("net.stream", "Group::GetIdleStreamSocket", track_, flow_,
                      "idle_stream_count", idle_stream_sockets_.size());

  return stream_socket;
}

void HttpStreamPool::Group::ProcessPendingRequest() {
  // TODO(crbug.com/381742472): Ensure what we should do when failing.
  if (!attempt_manager_) {
    return;
  }
  attempt_manager_->ProcessPendingJob();
}

bool HttpStreamPool::Group::CloseOneIdleStreamSocket() {
  if (idle_stream_sockets_.empty()) {
    return false;
  }

  RecordNetLogClosingSocket(*idle_stream_sockets_.front().stream_socket,
                            kExceededSocketLimits);
  idle_stream_sockets_.pop_front();
  pool_->DecrementTotalIdleStreamCount();
  // Use MaybeCompleteLater since MaybeComplete() may delete `this`, and this
  // method could be called while iterating all groups.
  MaybeCompleteLater();
  return true;
}

size_t HttpStreamPool::Group::ConnectingStreamSocketCount() const {
  return attempt_manager_ ? attempt_manager_->TcpBasedAttemptSlotCount() : 0;
}

size_t HttpStreamPool::Group::ActiveStreamSocketCount() const {
  return handed_out_stream_count_ + idle_stream_sockets_.size() +
         ConnectingStreamSocketCount();
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
    StreamSocketCloseReason attempt_cancel_reason,
    std::string_view net_log_close_reason_utf8) {
  Refresh(net_log_close_reason_utf8, attempt_cancel_reason);
  CancelJobs(error, attempt_cancel_reason);
}

void HttpStreamPool::Group::Refresh(std::string_view net_log_close_reason_utf8,
                                    StreamSocketCloseReason cancel_reason) {
  TRACE_EVENT_INSTANT("net.stream", "Group::Refresh", track_, flow_,
                      "cancel_reason", static_cast<int>(cancel_reason));

  ++generation_;
  if (attempt_manager_) {
    attempt_manager_->CancelTcpBasedAttempts(cancel_reason);
  }
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
}

void HttpStreamPool::Group::CloseIdleStreams(
    std::string_view net_log_close_reason_utf8) {
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
}

void HttpStreamPool::Group::CancelJobs(int error,
                                       StreamSocketCloseReason cancel_reason) {
  TRACE_EVENT_INSTANT("net.stream", "Group::CancelJobs", track_, flow_,
                      "cancel_reason", static_cast<int>(cancel_reason));
  if (attempt_manager_) {
    attempt_manager_->CancelJobs(error, cancel_reason);
  }
}

HttpStreamPool::AttemptManager* HttpStreamPool::Group::GetAttemptManagerForJob(
    Job* job) {
  if (job->type() == JobType::kAltSvcQuicPreconnect) {
    return GetAttemptManagerForAltSvcQuicPreconnect();
  }

  if (!attempt_manager_) {
    attempt_manager_ = std::make_unique<AttemptManager>(
        this, http_network_session()->net_log());
  }
  return attempt_manager_.get();
}

void HttpStreamPool::Group::OnAttemptManagerShuttingDown(
    AttemptManager* attempt_manager) {
  if (attempt_manager == attempt_manager_.get()) {
    shutting_down_attempt_managers_.emplace(std::move(attempt_manager_));
    CHECK(!attempt_manager_.get());
  } else if (attempt_manager ==
             alt_svc_quic_preconnect_attempt_manager_.get()) {
    shutting_down_attempt_managers_.emplace(
        std::move(alt_svc_quic_preconnect_attempt_manager_));
    CHECK(!alt_svc_quic_preconnect_attempt_manager_.get());
  } else {
    NOTREACHED();
  }
}

void HttpStreamPool::Group::OnAttemptManagerComplete(
    AttemptManager* attempt_manager) {
  auto it = shutting_down_attempt_managers_.find(attempt_manager);
  if (it != shutting_down_attempt_managers_.end()) {
    CHECK_NE(attempt_manager_.get(), attempt_manager);
    CHECK_NE(alt_svc_quic_preconnect_attempt_manager_.get(), attempt_manager);
    shutting_down_attempt_managers_.erase(it);
  } else {
    if (attempt_manager == attempt_manager_.get()) {
      attempt_manager_.reset();
    } else if (attempt_manager ==
               alt_svc_quic_preconnect_attempt_manager_.get()) {
      alt_svc_quic_preconnect_attempt_manager_.reset();
    } else {
      NOTREACHED();
    }
  }

  MaybeComplete();
}

base::Value::Dict HttpStreamPool::Group::GetInfoAsValue() const {
  base::Value::Dict dict;
  dict.Set("active_socket_count", static_cast<int>(ActiveStreamSocketCount()));
  dict.Set("idle_socket_count", static_cast<int>(IdleStreamSocketCount()));
  dict.Set("handed_out_socket_count",
           static_cast<int>(HandedOutStreamSocketCount()));
  dict.Set("attempt_manager_alive", !!attempt_manager_);
  if (attempt_manager_) {
    dict.Set("attempt_state", attempt_manager_->GetInfoAsValue());
  }

  return dict;
}

void HttpStreamPool::Group::CleanupTimedoutIdleStreamSocketsForTesting() {
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly, "For testing");
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
  // Use MaybeCompleteLater since MaybeComplete() may delete `this`, and this
  // method could be called while iterating all groups.
  MaybeCompleteLater();
}

HttpStreamPool::AttemptManager*
HttpStreamPool::Group::GetAttemptManagerForAltSvcQuicPreconnect() {
  if (!alt_svc_quic_preconnect_attempt_manager_) {
    alt_svc_quic_preconnect_attempt_manager_ = std::make_unique<AttemptManager>(
        this, http_network_session()->net_log());
  }
  return alt_svc_quic_preconnect_attempt_manager_.get();
}

bool HttpStreamPool::Group::CanComplete() const {
  return ActiveStreamSocketCount() == 0 && !attempt_manager_ &&
         !alt_svc_quic_preconnect_attempt_manager_ &&
         shutting_down_attempt_managers_.empty();
}

void HttpStreamPool::Group::MaybeComplete() {
  if (!CanComplete()) {
    return;
  }

  pool_->OnGroupComplete(this);
  // `this` is deleted.
}

void HttpStreamPool::Group::MaybeCompleteLater() {
  if (CanComplete()) {
    // Use IDLE priority since completing group is not urgent.
    TaskRunner(IDLE)->PostTask(
        FROM_HERE,
        base::BindOnce(&Group::MaybeComplete, weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace net

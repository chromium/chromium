// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include "base/task/sequenced_task_runner.h"
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

bool HttpStreamPool::Group::PausedJobComparator::operator()(Job* a,
                                                            Job* b) const {
  if (a->create_time() == b->create_time()) {
    return a < b;
  }
  return a->create_time() < b->create_time();
}

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
    quic::ParsedQuicVersion quic_version,
    NextProto expected_protocol,
    const NetLogWithSource& net_log) {
  return std::make_unique<Job>(delegate, this, quic_version, expected_protocol,
                               net_log);
}

bool HttpStreamPool::Group::CanStartJob(Job* job) {
  if (IsFailing()) {
    auto [_, inserted] = paused_jobs_.emplace(job);
    CHECK(inserted);
    // `job` will be resumed once the current AttemptManager completes with a
    // new AttemptManager.
    return false;
  }

  EnsureAttemptManager();
  return true;
}

void HttpStreamPool::Group::OnJobComplete(Job* job) {
  paused_jobs_.erase(job);
  notified_paused_jobs_.erase(job);

  if (attempt_manager_) {
    attempt_manager_->OnJobComplete(job);
    // `this` may be deleted.
  } else {
    MaybeComplete();
  }
}

int HttpStreamPool::Group::Preconnect(size_t num_streams,
                                      quic::ParsedQuicVersion quic_version,
                                      CompletionOnceCallback callback) {
  if (ActiveStreamSocketCount() >= num_streams) {
    return OK;
  }

  // When failing, just returns the current error.
  // TODO(crbug.com/381742472): Consider resuming this preconnect after the
  // current failing attempt manager completes.
  if (IsFailing()) {
    return attempt_manager_->error_to_notify();
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
  } else if (ReachedMaxStreamLimit()) {
    not_reusable_reason = kExceededSocketLimits;
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
  MaybeComplete();
}

void HttpStreamPool::Group::AddIdleStreamSocket(
    std::unique_ptr<StreamSocket> socket) {
  CHECK(socket->IsConnectedAndIdle());
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_LE(ActiveStreamSocketCount(), pool_->max_stream_sockets_per_group());

  idle_stream_sockets_.emplace_back(std::move(socket), base::TimeTicks::Now());
  pool_->IncrementTotalIdleStreamCount();
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly, kIdleTimeLimitExpired);
  MaybeComplete();
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
  // Use MaybeCompeleteLater since MaybeComplete() may delete `this`, and this
  // method could be called while iterating all groups.
  MaybeCompleteLater();
  return true;
}

size_t HttpStreamPool::Group::ConnectingStreamSocketCount() const {
  return attempt_manager_ ? attempt_manager_->InFlightAttemptCount() : 0;
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
    StreamCloseReason attempt_cancel_reason,
    std::string_view net_log_close_reason_utf8) {
  // Refresh() may delete this. Get a weak pointer to this and call CancelJobs()
  // only when this is still alive.
  base::WeakPtr<Group> weak_this = weak_ptr_factory_.GetWeakPtr();
  Refresh(net_log_close_reason_utf8, attempt_cancel_reason);
  if (weak_this) {
    CancelJobs(error);
  }
}

void HttpStreamPool::Group::Refresh(std::string_view net_log_close_reason_utf8,
                                    StreamCloseReason cancel_reason) {
  // TODO(crbug.com/381742472): Should we do anything for paused
  // jobs/preconnects?
  ++generation_;
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
  if (attempt_manager_) {
    attempt_manager_->CancelInFlightAttempts(cancel_reason);
  }
}

void HttpStreamPool::Group::CloseIdleStreams(
    std::string_view net_log_close_reason_utf8) {
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
  // Use MaybeCompleteLater since MaybeComplete() may delete `this`, and this
  // method could be called while iterating all groups.
  MaybeCompleteLater();
}

void HttpStreamPool::Group::CancelJobs(int error) {
  if (!paused_jobs_.empty()) {
    CancelPausedJob(error);
  }
  // TODO(crbug.com/381742472): Need to cancel paused preconnects when we
  // support paused preconnects.
  if (attempt_manager_) {
    attempt_manager_->CancelJobs(error);
  }
}

void HttpStreamPool::Group::OnRequiredHttp11() {
  // This method is called from the upper layer to fall back HTTP/1.1 for
  // on-going jobs/preconnects (not for paused ones). No need to handle
  // paused jobs/preconnects.
  // TODO(crbug.com/381742472): Confirm the above is correct.
  if (attempt_manager_) {
    attempt_manager_->OnRequiredHttp11();
  }
}

void HttpStreamPool::Group::OnAttemptManagerComplete() {
  CHECK(attempt_manager_);

  // TODO(crbug.com/381742472): Need to handle paused preconnects when we
  // support paused preconnects.
  const bool should_start_new_attempt_manager =
      attempt_manager_->is_failing() && !paused_jobs_.empty();

  attempt_manager_.reset();

  if (should_start_new_attempt_manager) {
    EnsureAttemptManager();
    ResumePausedJob();
  } else {
    MaybeComplete();
  }
}

base::Value::Dict HttpStreamPool::Group::GetInfoAsValue() const {
  base::Value::Dict dict;
  dict.Set("active_socket_count", static_cast<int>(ActiveStreamSocketCount()));
  dict.Set("idle_socket_count", static_cast<int>(IdleStreamSocketCount()));
  dict.Set("handed_out_socket_count",
           static_cast<int>(HandedOutStreamSocketCount()));
  dict.Set("paused_job_count", static_cast<int>(PausedJobCount()));
  dict.Set("notified_paused_job_count",
           static_cast<int>(notified_paused_jobs_.size()));
  dict.Set("attempt_manager_alive", !!attempt_manager_);
  if (attempt_manager_) {
    dict.Set("attempt_state", attempt_manager_->GetInfoAsValue());
  }

  if (!paused_jobs_.empty()) {
    base::Value::List paused_jobs;
    for (const auto job : paused_jobs_) {
      base::Value::Dict job_dict;
      job_dict.Set(
          "create_to_resume_ms",
          static_cast<int>(job->CreateToResumeTime().InMilliseconds()));
      paused_jobs.Append(std::move(job_dict));
    }
    dict.Set("paused_jobs", std::move(paused_jobs));
  }

  return dict;
}

void HttpStreamPool::Group::CleanupTimedoutIdleStreamSocketsForTesting() {
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly, "For testing");
}

bool HttpStreamPool::Group::IsFailing() const {
  // If we don't have an AttemptManager the group is not considered as failing
  // because we destroy an AttemptManager after all in-flight attempts are
  // completed (There are only handed out streams and/or idle streams).
  return attempt_manager_ && attempt_manager_->is_failing();
}

void HttpStreamPool::Group::ResumePausedJob() {
  // The current AttemptManager could be failing again while resuming jobs.
  if (IsFailing()) {
    return;
  }

  if (paused_jobs_.empty()) {
    return;
  }

  // Using PostTask() to resume the remaining paused jobs to avoid reentrancy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Group::ResumePausedJob, weak_ptr_factory_.GetWeakPtr()));

  raw_ptr<Job> job =
      std::move(paused_jobs_.extract(paused_jobs_.begin())).value();
  job->Resume();
}

void HttpStreamPool::Group::CancelPausedJob(int error) {
  Job* job = ExtractOnePausedJob();
  if (!job) {
    // Try to complete asynchronously because this method can be called in the
    // middle of CancelJobs() and `this` must be alive until CancelJobs()
    // completes.
    MaybeCompleteLater();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Group::CancelPausedJob,
                                weak_ptr_factory_.GetWeakPtr(), error));

  job->OnStreamFailed(error, NetErrorDetails(), ResolveErrorInfo());
}

HttpStreamPool::Job* HttpStreamPool::Group::ExtractOnePausedJob() {
  if (paused_jobs_.empty()) {
    return nullptr;
  }

  raw_ptr<Job> job =
      std::move(paused_jobs_.extract(paused_jobs_.begin())).value();
  Job* job_raw_ptr = job.get();
  notified_paused_jobs_.emplace(std::move(job));
  return job_raw_ptr;
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

bool HttpStreamPool::Group::CanComplete() const {
  // TODO(crbug.com/381742472): Check paused preconnects once we support
  // paused preconnects.
  return ActiveStreamSocketCount() == 0 && paused_jobs_.empty() &&
         notified_paused_jobs_.empty() && !attempt_manager_;
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Group::MaybeComplete, weak_ptr_factory_.GetWeakPtr()));
  }
}

}  // namespace net

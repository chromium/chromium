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
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_job.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"

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
                                 NetLogSourceType::HTTP_STREAM_POOL_GROUP)) {
  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_ALIVE, [&] {
    base::Value::Dict dict;
    dict.Set("stream_key", stream_key_.ToValue());
    return dict;
  });
}

HttpStreamPool::Group::~Group() {
  // TODO(crbug.com/346835898): Ensure `pool_`'s total active stream counts
  // are consistent.
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_ALIVE);
}

std::unique_ptr<HttpStreamRequest> HttpStreamPool::Group::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    bool enable_ip_based_pooling,
    bool enable_alternative_services,
    quic::ParsedQuicVersion quic_version,
    const NetLogWithSource& net_log) {
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_REQUEST_STREAM, [&] {
        base::Value::Dict dict;
        dict.Set("priority", priority);
        base::Value::List allowed_bad_certs_list;
        for (const auto& cert_and_status : allowed_bad_certs) {
          allowed_bad_certs_list.Append(
              cert_and_status.cert->subject().GetDisplayName());
        }
        dict.Set("allowed_bad_certs", std::move(allowed_bad_certs_list));
        dict.Set("enable_ip_based_pooling", enable_ip_based_pooling);
        net_log.source().AddToEventParameters(dict);
        return dict;
      });
  net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_GROUP_REQUEST_BOUND, net_log_.source());

  EnsureInFlightJob();
  return in_flight_job_->RequestStream(
      delegate, priority, allowed_bad_certs, enable_ip_based_pooling,
      enable_alternative_services, quic_version, net_log);
}

int HttpStreamPool::Group::Preconnect(size_t num_streams,
                                      quic::ParsedQuicVersion quic_version,
                                      CompletionOnceCallback callback) {
  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_POOL_GROUP_PRECONNECT, [&] {
    base::Value::Dict dict;
    dict.Set("num_streams", static_cast<int>(num_streams));
    return dict;
  });
  EnsureInFlightJob();
  return in_flight_job_->Preconnect(num_streams, quic_version,
                                    std::move(callback));
}

std::unique_ptr<HttpStreamPoolHandle> HttpStreamPool::Group::CreateHandle(
    std::unique_ptr<StreamSocket> socket,
    StreamSocketHandle::SocketReuseType reuse_type,
    LoadTimingInfo::ConnectTiming connect_timing) {
  CHECK_LE(ActiveStreamSocketCount(), pool_->max_stream_sockets_per_group());

  ++handed_out_stream_count_;
  pool_->IncrementTotalHandedOutStreamCount();

  auto handle = std::make_unique<HttpStreamPoolHandle>(this, std::move(socket),
                                                       generation_);
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
  if (!in_flight_job_) {
    return;
  }
  in_flight_job_->ProcessPendingRequest();
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
         (in_flight_job_ ? in_flight_job_->InFlightAttemptCount() : 0);
}

bool HttpStreamPool::Group::ReachedMaxStreamLimit() const {
  return ActiveStreamSocketCount() >= pool_->max_stream_sockets_per_group();
}

std::optional<RequestPriority>
HttpStreamPool::Group::GetPriorityIfStalledByPoolLimit() const {
  if (!in_flight_job_) {
    return std::nullopt;
  }

  return in_flight_job_->IsStalledByPoolLimit()
             ? std::make_optional(in_flight_job_->GetPriority())
             : std::nullopt;
}

void HttpStreamPool::Group::Refresh(
    std::string_view net_log_close_reason_utf8) {
  ++generation_;
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
  if (in_flight_job_) {
    in_flight_job_->CancelInFlightAttempts();
  }
}

void HttpStreamPool::Group::CloseIdleStreams(
    std::string_view net_log_close_reason_utf8) {
  CleanupIdleStreamSockets(CleanupMode::kForce, net_log_close_reason_utf8);
}

void HttpStreamPool::Group::CancelRequests(int error) {
  if (in_flight_job_) {
    in_flight_job_->CancelRequests(error);
  }
}

void HttpStreamPool::Group::OnRequiredHttp11() {
  if (in_flight_job_) {
    in_flight_job_->OnRequiredHttp11();
  }
}

void HttpStreamPool::Group::OnJobComplete() {
  CHECK(in_flight_job_);
  in_flight_job_.reset();
  MaybeComplete();
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
}

void HttpStreamPool::Group::EnsureInFlightJob() {
  if (in_flight_job_) {
    return;
  }
  in_flight_job_ =
      std::make_unique<Job>(this, http_network_session()->net_log());
}

void HttpStreamPool::Group::MaybeComplete() {
  if (ActiveStreamSocketCount() > 0) {
    return;
  }

  pool_->OnGroupComplete(this);
  // `this` is deleted.
}

}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include "net/http/http_basic_stream.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_text_based_stream_handle.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

bool IsNegotiatedProtocolTextBased(NextProto next_proto) {
  return next_proto == kProtoUnknown || next_proto == kProtoHTTP11;
}

enum class StreamSocketUsableState {
  kUsable,
  kRemoteSideClosed,
  kDataReceivedUnexpectedly,
};

StreamSocketUsableState CalculateStreamSocketUsableState(
    const StreamSocket& socket) {
  if (socket.WasEverUsed()) {
    if (socket.IsConnectedAndIdle()) {
      return StreamSocketUsableState::kUsable;
    }
    return socket.IsConnected()
               ? StreamSocketUsableState::kDataReceivedUnexpectedly
               : StreamSocketUsableState::kRemoteSideClosed;
  }

  return socket.IsConnected() ? StreamSocketUsableState::kUsable
                              : StreamSocketUsableState::kRemoteSideClosed;
}

}  // namespace

HttpStreamPool::Group::IdleStreamSocket::IdleStreamSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    base::TimeTicks time_became_idle)
    : stream_socket(std::move(stream_socket)),
      time_became_idle(time_became_idle) {}

HttpStreamPool::Group::IdleStreamSocket::~IdleStreamSocket() = default;

HttpStreamPool::Group::Group(HttpStreamPool* pool, HttpStreamKey stream_key)
    : pool_(pool), stream_key_(std::move(stream_key)) {}

HttpStreamPool::Group::~Group() {
  // TODO(crbug.com/346835898): Ensure `pool_`'s total active stream counts
  // are consistent.
}

std::unique_ptr<HttpStreamRequest> HttpStreamPool::Group::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    RequestPriority priority,
    const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
    const NetLogWithSource& net_log) {
  if (!in_flight_job_) {
    in_flight_job_ = std::make_unique<Job>(this, net_log.net_log());
  }

  return in_flight_job_->RequestStream(delegate, priority, allowed_bad_certs,
                                       net_log);
}

std::unique_ptr<HttpStream> HttpStreamPool::Group::CreateTextBasedStream(
    std::unique_ptr<StreamSocket> socket) {
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_LE(ActiveStreamSocketCount(), pool_->max_stream_sockets_per_group());

  ++handed_out_stream_count_;
  pool_->IncrementTotalHandedOutStreamCount();

  auto stream_handle = std::make_unique<HttpTextBasedStreamHandle>(
      this, std::move(socket), generation_);
  return std::make_unique<HttpBasicStream>(std::move(stream_handle),
                                           /*is_for_get_to_http_proxy=*/false);
}

void HttpStreamPool::Group::ReleaseStreamSocket(
    std::unique_ptr<StreamSocket> socket,
    int64_t generation) {
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_GT(handed_out_stream_count_, 0u);
  --handed_out_stream_count_;
  pool_->DecrementTotalHandedOutStreamCount();

  bool can_reuse = generation == generation_ && socket->IsConnectedAndIdle();
  if (can_reuse) {
    AddIdleStreamSocket(std::move(socket));
    ProcessPendingRequest();
  } else {
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
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly);
}

std::unique_ptr<StreamSocket> HttpStreamPool::Group::GetIdleStreamSocket() {
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly);

  if (idle_stream_sockets_.empty()) {
    return nullptr;
  }

  // Iterate through the idle streams from oldtest to newest and try to find a
  // used idle stream. Prefer the newest used idle stream.
  auto idle_it = idle_stream_sockets_.end();
  for (auto it = idle_stream_sockets_.begin();
       it != idle_stream_sockets_.end();) {
    CHECK_EQ(CalculateStreamSocketUsableState(*it->stream_socket),
             StreamSocketUsableState::kUsable);
    if (it->stream_socket->WasEverUsed()) {
      idle_it = it;
    }
    ++it;
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
  if (ReachedMaxStreamLimit()) {
    return std::nullopt;
  }

  if (!in_flight_job_ || in_flight_job_->PendingRequestCount() == 0) {
    return std::nullopt;
  }

  return in_flight_job_->GetPriority();
}

void HttpStreamPool::Group::Refresh() {
  ++generation_;
  CleanupIdleStreamSockets(CleanupMode::kForce);
  if (in_flight_job_) {
    in_flight_job_->CancelInFlightAttempts();
  }
}

void HttpStreamPool::Group::CancelRequests(int error) {
  if (in_flight_job_) {
    in_flight_job_->CancelRequests(error);
  }
}

void HttpStreamPool::Group::CleanupTimedoutIdleStreamSocketsForTesting() {
  CleanupIdleStreamSockets(CleanupMode::kTimeoutOnly);
}

void HttpStreamPool::Group::CleanupIdleStreamSockets(CleanupMode mode) {
  const base::TimeTicks now = base::TimeTicks::Now();

  // Iterate though the idle sockets to delete any disconnected ones.
  for (auto it = idle_stream_sockets_.begin();
       it != idle_stream_sockets_.end();) {
    bool should_delete = mode == CleanupMode::kForce;
    base::TimeDelta timeout = it->stream_socket->WasEverUsed()
                                  ? kUsedIdleStreamSocketTimeout
                                  : kUnusedIdleStreamSocketTimeout;
    StreamSocketUsableState state =
        CalculateStreamSocketUsableState(*it->stream_socket);
    if (state != StreamSocketUsableState::kUsable) {
      should_delete = true;
    } else if (now - it->time_became_idle >= timeout) {
      should_delete = true;
    }

    if (should_delete) {
      it = idle_stream_sockets_.erase(it);
      pool_->DecrementTotalIdleStreamCount();
    } else {
      ++it;
    }
  }
}

}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_group.h"

#include "net/http/http_basic_stream.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_text_based_stream_handle.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

bool IsNegotiatedProtocolTextBased(NextProto next_proto) {
  return next_proto == kProtoUnknown || next_proto == kProtoHTTP11;
}

}  // namespace

HttpStreamPool::Group::IdleStreamSocket::IdleStreamSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    base::TimeTicks start_time)
    : stream_socket(std::move(stream_socket)), time_became_idle(start_time) {}

HttpStreamPool::Group::IdleStreamSocket::~IdleStreamSocket() = default;

HttpStreamPool::Group::Group(HttpStreamPool* pool, HttpStreamKey stream_key)
    : pool_(pool), stream_key_(std::move(stream_key)) {}

HttpStreamPool::Group::~Group() = default;

std::unique_ptr<HttpStream> HttpStreamPool::Group::CreateTextBasedStream(
    std::unique_ptr<StreamSocket> socket) {
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_LE(ActiveStreamSocketCount(), kMaxStreamSocketsPerGroup);

  ++handed_out_stream_count_;
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

  bool can_reuse = generation == generation_ && socket->IsConnectedAndIdle();
  if (can_reuse) {
    AddIdleStreamSocket(std::move(socket));
  }
}

void HttpStreamPool::Group::AddIdleStreamSocket(
    std::unique_ptr<StreamSocket> socket) {
  CHECK(socket->IsConnectedAndIdle());
  CHECK(IsNegotiatedProtocolTextBased(socket->GetNegotiatedProtocol()));
  CHECK_LE(ActiveStreamSocketCount(), kMaxStreamSocketsPerGroup);

  idle_stream_sockets_.emplace_back(std::move(socket), base::TimeTicks::Now());
  CleanupIdleStreamSockets();
}

void HttpStreamPool::Group::CleanupIdleStreamSockets() {
  const base::TimeTicks now = base::TimeTicks::Now();

  // Iterate though the idle sockets to delete any disconnected ones.
  for (auto it = idle_stream_sockets_.begin();
       it != idle_stream_sockets_.end();) {
    bool should_delete = false;
    base::TimeDelta timeout = it->stream_socket->WasEverUsed()
                                  ? kUsedIdleStreamSocketTimeout
                                  : kUnusedIdleStreamSocketTimeout;
    if (!it->stream_socket->IsConnected()) {
      should_delete = true;
    } else if (now - it->time_became_idle >= timeout) {
      should_delete = true;
    }

    if (should_delete) {
      it = idle_stream_sockets_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace net

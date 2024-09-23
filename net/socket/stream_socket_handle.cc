// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stream_socket_handle.h"

#include <memory>

#include "net/base/load_timing_info.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"

namespace net {

StreamSocketHandle::StreamSocketHandle() = default;

StreamSocketHandle::~StreamSocketHandle() = default;

void StreamSocketHandle::SetSocket(std::unique_ptr<StreamSocket> socket) {
  socket_ = std::move(socket);
}

std::unique_ptr<StreamSocket> StreamSocketHandle::PassSocket() {
  return std::move(socket_);
}

bool StreamSocketHandle::GetLoadTimingInfo(
    bool is_reused,
    LoadTimingInfo* load_timing_info) const {
  if (socket_) {
    load_timing_info->socket_log_id = socket_->NetLog().source().id;
  } else {
    // Only return load timing information when there's a socket.
    return false;
  }

  load_timing_info->socket_reused = is_reused;

  // No times if the socket is reused.
  if (is_reused) {
    return true;
  }

  load_timing_info->connect_timing = connect_timing_;
  return true;
}

void StreamSocketHandle::AddHigherLayeredPool(HigherLayeredPool* pool) {}

void StreamSocketHandle::RemoveHigherLayeredPool(HigherLayeredPool* pool) {}

}  // namespace net

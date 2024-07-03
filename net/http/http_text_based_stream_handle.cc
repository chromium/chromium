// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_text_based_stream_handle.h"

#include <memory>

#include "net/http/http_stream_pool_group.h"
#include "net/socket/stream_socket.h"

namespace net {

HttpTextBasedStreamHandle::HttpTextBasedStreamHandle(
    HttpStreamPool::Group* group,
    std::unique_ptr<StreamSocket> socket,
    int64_t generation)
    : group_(group), generation_(generation) {
  CHECK(group);
  CHECK(socket);

  // Always considered initialized.
  SetSocket(std::move(socket));
  set_is_initialized(true);
}

HttpTextBasedStreamHandle::~HttpTextBasedStreamHandle() {
  Reset();
}

void HttpTextBasedStreamHandle::Reset() {
  if (socket()) {
    group_->ReleaseStreamSocket(PassSocket(), generation_);
  }
}

}  // namespace net

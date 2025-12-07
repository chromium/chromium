// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_handle.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/socket/stream_socket.h"

namespace net {

HttpStreamPoolHandle::HttpStreamPoolHandle(
    base::WeakPtr<HttpStreamPool::Group> group,
    std::unique_ptr<StreamSocket> socket,
    int64_t generation)
    : group_(std::move(group)), generation_(generation) {
  CHECK(group_);
  CHECK(socket);

  // Always considered initialized.
  SetSocket(std::move(socket));
  set_is_initialized(true);
}

HttpStreamPoolHandle::~HttpStreamPoolHandle() {
  Reset();
}

void HttpStreamPoolHandle::Reset() {
  if (socket() && group_) {
    group_->ReleaseStreamSocket(PassSocket(), generation_);
  }
}

bool HttpStreamPoolHandle::IsPoolStalled() const {
  return group_->pool()->IsPoolStalled();
}

}  // namespace net

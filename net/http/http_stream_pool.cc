// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool.h"

#include <map>
#include <memory>

#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"

namespace net {

HttpStreamPool::HttpStreamPool() = default;

HttpStreamPool::~HttpStreamPool() = default;

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroupForTesting(
    const HttpStreamKey& stream_key) {
  return GetOrCreateGroup(stream_key);
}

HttpStreamPool::Group& HttpStreamPool::GetOrCreateGroup(
    const HttpStreamKey& stream_key) {
  auto it = groups_.find(stream_key);
  if (it == groups_.end()) {
    it = groups_.try_emplace(it, stream_key,
                             std::make_unique<Group>(this, stream_key));
  }
  return *it->second;
}

}  // namespace net

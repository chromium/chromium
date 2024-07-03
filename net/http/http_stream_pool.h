// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_H_
#define NET_HTTP_HTTP_STREAM_POOL_H_

#include <map>
#include <memory>

#include "net/base/net_export.h"

namespace net {

class HttpStreamKey;

// Manages in-flight HTTP stream requests and maintains idle stream sockets.
// Restricts the number of streams open at a time. HttpStreams are grouped by
// HttpStreamKey.
//
// Currently only supports non-proxy streams.
class NET_EXPORT_PRIVATE HttpStreamPool {
 public:
  // The maximum number of sockets per pool. The same as
  // ClientSocketPoolManager::max_sockets_per_pool().
  static constexpr size_t kMaxStreamSocketsPerPool = 256;

  class NET_EXPORT_PRIVATE Group;

  HttpStreamPool();

  HttpStreamPool(const HttpStreamPool&) = delete;
  HttpStreamPool& operator=(const HttpStreamPool&) = delete;

  ~HttpStreamPool();

  Group& GetOrCreateGroupForTesting(const HttpStreamKey& stream_key);

 private:
  Group& GetOrCreateGroup(const HttpStreamKey& stream_key);

  std::map<HttpStreamKey, std::unique_ptr<Group>> groups_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_H_

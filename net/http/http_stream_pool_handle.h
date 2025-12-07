// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_HANDLE_H_
#define NET_HTTP_HTTP_STREAM_POOL_HANDLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/http/http_stream_pool.h"
#include "net/socket/stream_socket_handle.h"

namespace net {

class StreamSocket;

// A StreamSocketHandle that is associated with an HttpStreamPool::Group.
class NET_EXPORT_PRIVATE HttpStreamPoolHandle : public StreamSocketHandle {
 public:
  HttpStreamPoolHandle(base::WeakPtr<HttpStreamPool::Group> group,
                       std::unique_ptr<StreamSocket> socket,
                       int64_t generation);

  HttpStreamPoolHandle(const HttpStreamPoolHandle&) = delete;
  HttpStreamPoolHandle& operator=(const HttpStreamPoolHandle&) = delete;

  ~HttpStreamPoolHandle() override;

  // StreamSocketHandle implementation:
  void Reset() override;
  bool IsPoolStalled() const override;

 private:
  base::WeakPtr<HttpStreamPool::Group> group_;
  const int64_t generation_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_HANDLE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_GROUP_H_
#define NET_HTTP_HTTP_STREAM_POOL_GROUP_H_

#include <list>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"

namespace net {

class HttpStream;
class StreamSocket;

// Maintains active/idle text-based HTTP streams.
class HttpStreamPool::Group {
 public:
  // The same timeout as ClientSocketPool::used_idle_socket_timeout().
  static constexpr base::TimeDelta kUsedIdleStreamSocketTimeout =
      base::Seconds(300);

  // The same timeout as
  // ClientSocketPoolManager::unused_idle_socket_timeout().
  static constexpr base::TimeDelta kUnusedIdleStreamSocketTimeout =
      base::Seconds(60);

  // The maximum number of socket per group. The same as
  // ClientSocketPoolManager::max_sockets_per_group().
  static constexpr size_t kMaxStreamSocketsPerGroup = 6;

  Group(HttpStreamPool* pool, HttpStreamKey stream_key);

  Group(const Group&) = delete;
  Group& operator=(const Group&) = delete;

  ~Group();

  // Creates a text-based HttpStream from `socket`. Call sites must ensure that
  // the number of active streams do not exceed the global/per-group limits.
  // `socket` must not be negotiated to use HTTP/2.
  std::unique_ptr<HttpStream> CreateTextBasedStream(
      std::unique_ptr<StreamSocket> socket);

  // Creates a text-based HttpStream from an existing idle stream. Returns
  // nullptr when there is no idle stream.
  std::unique_ptr<HttpStream> CreateTextBasedStreamFromIdleStreamSocket();

  // Releases a StreamSocket that was used to create a text-based HttpStream.
  void ReleaseStreamSocket(std::unique_ptr<StreamSocket> socket,
                           int64_t generation);

  // Adds `socket` as an idle StreamSocket for text-based HttpStream. Call sites
  // must ensure that the number of idle streams do not exceed the global/per-
  // group limits.
  // `socket` must not be negotiated to use HTTP/2.
  void AddIdleStreamSocket(std::unique_ptr<StreamSocket> socket);

  // Returns the number of idle streams.
  size_t IdleStreamSocketCount() const { return idle_stream_sockets_.size(); }

  // Returns the number of active streams.
  size_t ActiveStreamSocketCount() const {
    return handed_out_stream_count_ + idle_stream_sockets_.size();
  }

  // Increments the generation of this group. Closes idle streams. Streams
  // handed out before this increment won't be reused.
  void IncrementGeneration();

  void CleanupTimedoutIdleStreamSocketsForTesting();

 private:
  struct IdleStreamSocket {
    IdleStreamSocket(std::unique_ptr<StreamSocket> stream_socket,
                     base::TimeTicks start_time);
    ~IdleStreamSocket();

    IdleStreamSocket(const IdleStreamSocket&) = delete;
    IdleStreamSocket& operator=(const IdleStreamSocket&) = delete;

    std::unique_ptr<StreamSocket> stream_socket;
    base::TimeTicks time_became_idle;
  };

  enum class CleanupMode {
    // Clean up only timed out idle streams.
    kTimeoutOnly,
    // Clean up all idle streams.
    kForce,
  };
  void CleanupIdleStreamSockets(CleanupMode mode);

  const raw_ptr<HttpStreamPool> pool_;
  const HttpStreamKey stream_key_;

  size_t handed_out_stream_count_ = 0;
  int64_t generation_ = 0;
  std::list<IdleStreamSocket> idle_stream_sockets_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_GROUP_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_H_
#define NET_HTTP_HTTP_STREAM_POOL_H_

#include <map>
#include <memory>

#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

class HttpStreamKey;

// Manages in-flight HTTP stream requests and maintains idle stream sockets.
// Restricts the number of streams open at a time. HttpStreams are grouped by
// HttpStreamKey.
//
// Currently only supports non-proxy streams.
class NET_EXPORT_PRIVATE HttpStreamPool
    : public NetworkChangeNotifier::IPAddressObserver {
 public:
  // The maximum number of sockets per pool. The same as
  // ClientSocketPoolManager::max_sockets_per_pool().
  static constexpr size_t kMaxStreamSocketsPerPool = 256;

  class NET_EXPORT_PRIVATE Group;

  explicit HttpStreamPool(bool cleanup_on_ip_address_change = true);

  HttpStreamPool(const HttpStreamPool&) = delete;
  HttpStreamPool& operator=(const HttpStreamPool&) = delete;

  ~HttpStreamPool() override;

  // Increments/Decrements the total number of idle streams in this pool.
  void IncrementTotalIdleStreamCount();
  void DecrementTotalIdleStreamCount();

  // Increments/Decrements the total number of active streams this pool handed
  // out.
  void IncrementTotalHandedOutStreamCount();
  void DecrementTotalHandedOutStreamCount();

  size_t TotalActiveStreamCount() const {
    return total_handed_out_stream_count_ + total_idle_stream_count_;
  }

  // NetworkChangeNotifier::IPAddressObserver methods:
  void OnIPAddressChanged() override;

  Group& GetOrCreateGroupForTesting(const HttpStreamKey& stream_key);

 private:
  Group& GetOrCreateGroup(const HttpStreamKey& stream_key);

  const bool cleanup_on_ip_address_change_;

  // The total number of active streams this pool handed out across all groups.
  size_t total_handed_out_stream_count_ = 0;

  // The total number of idle streams in this pool.
  size_t total_idle_stream_count_ = 0;

  std::map<HttpStreamKey, std::unique_ptr<Group>> groups_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_H_

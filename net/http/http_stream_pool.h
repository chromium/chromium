// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_H_
#define NET_HTTP_HTTP_STREAM_POOL_H_

#include <map>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class HttpStreamKey;
class HttpNetworkSession;

// Manages in-flight HTTP stream requests and maintains idle stream sockets.
// Restricts the number of streams open at a time. HttpStreams are grouped by
// HttpStreamKey.
//
// Currently only supports non-proxy streams.
class NET_EXPORT_PRIVATE HttpStreamPool
    : public NetworkChangeNotifier::IPAddressObserver,
      public SSLClientContext::Observer {
 public:
  // The maximum number of sockets per pool. The same as
  // ClientSocketPoolManager::max_sockets_per_pool().
  static constexpr size_t kMaxStreamSocketsPerPool = 256;

  // The maximum number of socket per group. The same as
  // ClientSocketPoolManager::max_sockets_per_group().
  static constexpr size_t kMaxStreamSocketsPerGroup = 6;

  // The time to wait between connection attempts.
  static constexpr base::TimeDelta kConnectionAttemptDelay =
      base::Milliseconds(250);

  class NET_EXPORT_PRIVATE Group;
  class NET_EXPORT_PRIVATE Job;

  explicit HttpStreamPool(HttpNetworkSession* http_network_session,
                          bool cleanup_on_ip_address_change = true);

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

  // Increments/Decrements the total number of connecting streams this pool.
  void IncrementTotalConnectingStreamCount();
  void DecrementTotalConnectingStreamCount();

  size_t TotalActiveStreamCount() const {
    return total_handed_out_stream_count_ + total_idle_stream_count_ +
           total_connecting_stream_count_;
  }

  bool ReachedMaxStreamLimit() const {
    return TotalActiveStreamCount() >= max_stream_sockets_per_pool();
  }

  // NetworkChangeNotifier::IPAddressObserver methods:
  void OnIPAddressChanged() override;

  // SSLClientContext::Observer methods.
  void OnSSLConfigChanged(
      SSLClientContext::SSLConfigChangeType change_type) override;
  void OnSSLConfigForServersChanged(
      const base::flat_set<HostPortPair>& servers) override;

  // Checks if there are any pending requests in groups and processes them. If
  // `this` reached the maximum number of streams, it will try to close idle
  // streams before processing pending requests.
  void ProcessPendingRequestsInGroups();

  Group& GetOrCreateGroupForTesting(const HttpStreamKey& stream_key);

  HttpNetworkSession* http_network_session() const {
    return http_network_session_;
  }

  size_t max_stream_sockets_per_pool() const {
    return max_stream_sockets_per_pool_;
  }

  size_t max_stream_sockets_per_group() const {
    return max_stream_sockets_per_group_;
  }

  void set_max_stream_sockets_per_pool_for_testing(
      size_t max_stream_sockets_per_pool) {
    max_stream_sockets_per_pool_ = max_stream_sockets_per_pool;
  }

  void set_max_stream_sockets_per_group_for_testing(
      size_t max_stream_sockets_per_group) {
    max_stream_sockets_per_group_ = max_stream_sockets_per_group;
  }

 private:
  Group& GetOrCreateGroup(const HttpStreamKey& stream_key);

  // Searches for a group that has the highest priority pending request and
  // hasn't reached reach the `max_stream_socket_per_group()` limit. Returns
  // nullptr if no such group is found.
  Group* FindHighestStalledGroup();

  // Closes one idle stream from an arbitrary group. Returns true if it closed a
  // stream.
  bool CloseOneIdleStreamSocket();

  const raw_ptr<HttpNetworkSession> http_network_session_;

  const bool cleanup_on_ip_address_change_;

  size_t max_stream_sockets_per_pool_ = kMaxStreamSocketsPerPool;
  size_t max_stream_sockets_per_group_ = kMaxStreamSocketsPerGroup;

  // The total number of active streams this pool handed out across all groups.
  size_t total_handed_out_stream_count_ = 0;

  // The total number of idle streams in this pool.
  size_t total_idle_stream_count_ = 0;

  // The total number of connecting streams in this pool.
  size_t total_connecting_stream_count_ = 0;

  std::map<HttpStreamKey, std::unique_ptr<Group>> groups_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_H_

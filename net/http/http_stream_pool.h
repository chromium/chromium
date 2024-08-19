// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_H_
#define NET_HTTP_HTTP_STREAM_POOL_H_

#include <map>
#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/http/http_stream_request.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_attempt.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace net {

class HttpStreamKey;
class HttpNetworkSession;
class NetLogWithSource;

// Manages in-flight HTTP stream requests and maintains idle stream sockets.
// Restricts the number of streams open at a time. HttpStreams are grouped by
// HttpStreamKey.
//
// Currently only supports non-proxy streams.
class NET_EXPORT_PRIVATE HttpStreamPool
    : public NetworkChangeNotifier::IPAddressObserver,
      public SSLClientContext::Observer {
 public:
  // Reasons for closing streams.
  static constexpr std::string_view kIpAddressChanged = "IP address changed";
  static constexpr std::string_view kSslConfigChanged =
      "SSL configuration changed";
  static constexpr std::string_view kIdleTimeLimitExpired =
      "Idle time limit expired";
  static constexpr std::string_view kSwitchingToHttp2 = "Switching to HTTP/2";
  static constexpr std::string_view kSwitchingToHttp3 = "Switching to HTTP/3";
  static constexpr std::string_view kRemoteSideClosedConnection =
      "Remote side closed connection";
  static constexpr std::string_view kDataReceivedUnexpectedly =
      "Data received unexpectedly";
  static constexpr std::string_view kClosedConnectionReturnedToPool =
      "Connection was closed when it was returned to the pool";
  static constexpr std::string_view kSocketGenerationOutOfDate =
      "Socket generation out of date";

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
  class NET_EXPORT_PRIVATE QuicTask;

  explicit HttpStreamPool(HttpNetworkSession* http_network_session,
                          bool cleanup_on_ip_address_change = true);

  HttpStreamPool(const HttpStreamPool&) = delete;
  HttpStreamPool& operator=(const HttpStreamPool&) = delete;

  ~HttpStreamPool() override;

  // Requests an HttpStream.
  std::unique_ptr<HttpStreamRequest> RequestStream(
      HttpStreamRequest::Delegate* delegate,
      const HttpStreamKey& stream_key,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      quic::ParsedQuicVersion quic_version,
      const NetLogWithSource& net_log);

  // Requests that enough connections/sessions for `num_streams` be opened.
  // `callback` is only invoked when the return value is `ERR_IO_PENDING`.
  int Preconnect(const HttpStreamKey& stream_key,
                 size_t num_streams,
                 quic::ParsedQuicVersion quic_version,
                 CompletionOnceCallback callback);

  // Increments/Decrements the total number of idle streams in this pool.
  void IncrementTotalIdleStreamCount();
  void DecrementTotalIdleStreamCount();

  size_t TotalIdleStreamCount() { return total_idle_stream_count_; }

  // Increments/Decrements the total number of active streams this pool handed
  // out.
  void IncrementTotalHandedOutStreamCount();
  void DecrementTotalHandedOutStreamCount();

  // Increments/Decrements the total number of connecting streams this pool.
  void IncrementTotalConnectingStreamCount();
  void DecrementTotalConnectingStreamCount(size_t amount = 1);

  size_t TotalConnectingStreamCount() const {
    return total_connecting_stream_count_;
  }

  size_t TotalActiveStreamCount() const {
    return total_handed_out_stream_count_ + total_idle_stream_count_ +
           total_connecting_stream_count_;
  }

  void CloseIdleStreams(std::string_view net_log_close_reason_utf8);

  bool ReachedMaxStreamLimit() const {
    return TotalActiveStreamCount() >= max_stream_sockets_per_pool();
  }

  // Return true if there is a request blocked on this pool.
  bool IsPoolStalled();

  // NetworkChangeNotifier::IPAddressObserver methods:
  void OnIPAddressChanged() override;

  // SSLClientContext::Observer methods.
  void OnSSLConfigChanged(
      SSLClientContext::SSLConfigChangeType change_type) override;
  void OnSSLConfigForServersChanged(
      const base::flat_set<HostPortPair>& servers) override;

  // Called when a group has completed.
  void OnGroupComplete(Group* group);

  // Checks if there are any pending requests in groups and processes them. If
  // `this` reached the maximum number of streams, it will try to close idle
  // streams before processing pending requests.
  void ProcessPendingRequestsInGroups();

  // Returns true when HTTP/1.1 is required for `stream_key`.
  bool RequiresHTTP11(const HttpStreamKey& stream_key);

  // Returns true when QUIC can be used for `stream_key`.
  bool CanUseQuic(const HttpStreamKey& stream_key,
                  bool enable_ip_based_pooling,
                  bool enable_alternative_services);

  // Returns true when there is an existing QUIC session for `stream_key` and
  // `quic_session_key`.
  bool CanUseExistingQuicSession(const HttpStreamKey& stream_key,
                                 const QuicSessionKey& quic_session_key,
                                 bool enable_ip_based_pooling,
                                 bool enable_alternative_services);

  Group& GetOrCreateGroupForTesting(const HttpStreamKey& stream_key);

  HttpNetworkSession* http_network_session() const {
    return http_network_session_;
  }

  const StreamAttemptParams* stream_attempt_params() const {
    return &stream_attempt_params_;
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
  class PooledStreamRequestHelper;

  Group& GetOrCreateGroup(const HttpStreamKey& stream_key);

  Group* GetGroup(const HttpStreamKey& stream_key);

  // Searches for a group that has the highest priority pending request and
  // hasn't reached reach the `max_stream_socket_per_group()` limit. Returns
  // nullptr if no such group is found.
  Group* FindHighestStalledGroup();

  // Closes one idle stream from an arbitrary group. Returns true if it closed a
  // stream.
  bool CloseOneIdleStreamSocket();

  base::WeakPtr<SpdySession> FindAvailableSpdySession(
      const HttpStreamKey& stream_key,
      const SpdySessionKey& spdy_session_key,
      bool enable_ip_based_pooling,
      const NetLogWithSource& net_log = NetLogWithSource());

  std::unique_ptr<HttpStreamRequest> CreatePooledStreamRequest(
      HttpStreamRequest::Delegate* delegate,
      std::unique_ptr<HttpStream> http_stream,
      NextProto negotiated_protocol,
      const NetLogWithSource& net_log);

  void OnPooledStreamRequestComplete(PooledStreamRequestHelper* helper);

  const raw_ptr<HttpNetworkSession> http_network_session_;

  StreamAttemptParams stream_attempt_params_;

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

  std::set<std::unique_ptr<PooledStreamRequestHelper>,
           base::UniquePtrComparator>
      pooled_stream_request_helpers_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_H_

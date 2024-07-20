// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_JOB_H_
#define NET_HTTP_HTTP_STREAM_POOL_JOB_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "url/gurl.h"

namespace net {

class HttpNetworkSession;
class NetLog;
class HttpStreamKey;

// Maintains in-flight HTTP stream requests. Peforms DNS resolution.
class HttpStreamPool::Job
    : public HostResolver::ServiceEndpointRequest::Delegate,
      public TlsStreamAttempt::SSLConfigProvider {
 public:
  // `group` must outlive `this`.
  Job(Group* group, NetLog* net_log);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job() override;

  // Creates an HttpStreamRequest. Will call delegate's methods. See the
  // comments of HttpStreamRequest::Delegate methods for details.
  // TODO(crbug.com/346835898): Support TLS, HTTP/2 and QUIC.
  std::unique_ptr<HttpStreamRequest> RequestStream(
      HttpStreamRequest::Delegate* delegate,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      const NetLogWithSource& net_log);

  // HostResolver::ServiceEndpointRequest::Delegate implementation:
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

  // TlsStreamAttempt::SSLConfigProvider implementation:
  int WaitForSSLConfigReady(CompletionOnceCallback callback) override;
  SSLConfig GetSSLConfig() override;

  // Tries to process a pending request.
  void ProcessPendingRequest();

  // Returns the number of total requests in this job.
  size_t RequestCount() const { return requests_.size(); }

  // Returns the number of in-flight attempts.
  size_t InFlightAttemptCount() const { return in_flight_attempts_.size(); }

  // Cancels all in-flight attempts.
  void CancelInFlightAttempts();

  // Cancels all requests.
  void CancelRequests(int error);

  // Returns the number of pending requests. The number is calculated by
  // subtracting the number of in-flight attempts (excluding slow attempts) from
  // the number of total requests.
  size_t PendingRequestCount() const;

  // Returns the highest priority in `requests_`.
  RequestPriority GetPriority() const;

 private:
  // Represents failure of connection attempts. Used to call request's delegate
  // methods.
  enum class FailureKind {
    kStreamFailed,
    kCertifcateError,
    kNeedsClientAuth,
  };

  // A peer of an HttpStreamRequest. Holds the HttpStreamRequest's delegate
  // pointer and implements HttpStreamRequest::Helper.
  class RequestEntry : public HttpStreamRequest::Helper {
   public:
    explicit RequestEntry(Job* job);

    RequestEntry(RequestEntry&) = delete;
    RequestEntry& operator=(const RequestEntry&) = delete;

    ~RequestEntry() override;

    std::unique_ptr<HttpStreamRequest> CreateRequest(
        HttpStreamRequest::Delegate* delegate,
        const NetLogWithSource& net_log);

    HttpStreamRequest* request() const { return request_; }

    HttpStreamRequest::Delegate* delegate() const { return delegate_; }

    // HttpStreamRequest::Helper methods:
    LoadState GetLoadState() const override;
    void OnRequestComplete() override;
    int RestartTunnelWithProxyAuth() override;
    void SetPriority(RequestPriority priority) override;

   private:
    const raw_ptr<Job> job_;
    raw_ptr<HttpStreamRequest> request_;
    raw_ptr<HttpStreamRequest::Delegate> delegate_;
  };

  using RequestQueue = PriorityQueue<std::unique_ptr<RequestEntry>>;

  struct InFlightAttempt;

  const HttpStreamKey& stream_key() const;

  HttpNetworkSession* http_network_session();

  HttpStreamPool* pool();
  const HttpStreamPool* pool() const;

  bool UsingTls() const;

  // Returns the current load state.
  LoadState GetLoadState() const;

  void ResolveServiceEndpoint(RequestPriority initial_priority);

  void MaybeChangeServiceEndpointRequestPriority();

  // Called when service endpoint results have changed or finished.
  void ProcessServiceEndpoindChanges();

  // Calculate SSLConfig if it's not calculated yet and `this` has received
  // enough information to calculate it.
  void MaybeCalculateSSLConfig();

  // Attempts connections if there are pending requests and IPEndPoints that
  // haven't failed. If `max_attempts` is given, attempts connections up to
  // `max_attempts`.
  void MaybeAttemptConnection(
      std::optional<size_t> max_attempts = std::nullopt);

  std::optional<IPEndPoint> GetIPEndPointToAttempt();
  std::optional<IPEndPoint> FindUnattemptedIPEndPoint(
      const std::vector<IPEndPoint>& ip_endpoints);

  // Calculate the failure kind to notify requests of failure. Used to call
  // one of the delegate's methods.
  FailureKind DetermineFailureKind();

  // Notifies a failure to all requests.
  void NotifyFailure();

  // Creates a text based stream and notifies the highest priority request.
  void CreateTextBasedStreamAndNotify(
      std::unique_ptr<StreamSocket> stream_socket);

  // Extracts an entry from `requests_` of which priority is highest. The
  // ownership of the entry is moved to `notified_requests_`.
  RequestEntry* ExtractFirstRequestToNotify();

  // Called when the priority of `request` is set.
  void SetRequestPriority(HttpStreamRequest* request, RequestPriority priority);

  // Called when an HttpStreamRequest associated with `entry` is going to
  // be destroyed.
  void OnRequestComplete(RequestEntry* entry);

  void OnInFlightAttemptComplete(InFlightAttempt* raw_attempt, int rv);
  void OnInFlightAttemptSlow(InFlightAttempt* raw_attempt);

  void HandleAttemptFailure(std::unique_ptr<InFlightAttempt> in_flight_attempt,
                            int rv);

  void MaybeComplete();

  const raw_ptr<Group> group_;
  const NetLogWithSource net_log_;

  ProxyInfo proxy_info_;

  // Holds requests that are waiting for notifications (a delegate method call
  // to indicate success or failure).
  RequestQueue requests_;
  // Holds requests that are already notified results. We need to keep them
  // to avoid dangling pointers.
  std::set<std::unique_ptr<RequestEntry>, base::UniquePtrComparator>
      notified_requests_;

  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
  bool service_endpoint_request_finished_ = false;

  // Set to true when `this` cannot handle further requests. Used to ensure that
  // `this` doesn't accept further requests while notifying the failure to the
  // existing requests.
  bool is_failing_ = false;

  // Set to true when `CancelRequests()` is called.
  bool is_canceling_requests_ = false;

  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;

  // Set to an error from the latest stream attempt failure or network change
  // events. Used to notify delegates when all attempts failed.
  int error_to_notify_ = ERR_FAILED;

  // Set to a SSLInfo when an attempt has failed with a certificate error. Used
  // to notify requests.
  std::optional<SSLInfo> cert_error_ssl_info_;

  // Set to a SSLCertRequestInfo when an attempt has requested a client cert.
  // Used to notify requests.
  scoped_refptr<SSLCertRequestInfo> client_auth_cert_info_;

  // Allowed bad certificates from the newest request.
  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  // SSLConfig for all TLS connection attempts. Calculated after the service
  // endpoint request is ready to proceed cryptographic handshakes.
  // TODO(crbug.com/40812426): We need to have separate SSLConfigs when we
  // support multiple HTTPS RR that have different service endpoints.
  std::optional<SSLConfig> ssl_config_;
  std::vector<CompletionOnceCallback> ssl_config_waiting_callbacks_;

  StreamAttemptParams attempt_params_;
  std::set<std::unique_ptr<InFlightAttempt>, base::UniquePtrComparator>
      in_flight_attempts_;
  // The number of in-flight attempts that are treated as slow.
  size_t slow_attempt_count_ = 0;

  // When true, try to use IPv6 for the next attempt first.
  bool prefer_ipv6_ = true;
  // Updated when a stream attempt failed. Used to calculate next IPEndPoint to
  // attempt.
  std::set<IPEndPoint> failed_ip_endpoints_;
  // Updated when a stream attempt is considered slow. Used to calculate next
  // IPEndPoint to attempt.
  std::set<IPEndPoint> slow_ip_endpoints_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_JOB_H_

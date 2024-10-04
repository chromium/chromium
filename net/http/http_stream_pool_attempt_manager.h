// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_H_
#define NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_handle.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/gurl.h"

namespace net {

class HttpNetworkSession;
class NetLog;
class HttpStreamKey;

// Maintains in-flight Jobs. Peforms DNS resolution.
class HttpStreamPool::AttemptManager
    : public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  // Time to delay connection attempts more than one when the destination is
  // known to support HTTP/2, to avoid unnecessary socket connection
  // establishments. See https://crbug.com/718576
  static constexpr base::TimeDelta kSpdyThrottleDelay = base::Milliseconds(300);

  // `group` must outlive `this`.
  AttemptManager(Group* group, NetLog* net_log);

  AttemptManager(const AttemptManager&) = delete;
  AttemptManager& operator=(const AttemptManager&) = delete;

  ~AttemptManager() override;

  Group* group() { return group_; }

  HostResolver::ServiceEndpointRequest* service_endpoint_request() {
    return service_endpoint_request_.get();
  }

  bool is_service_endpoint_request_finished() const {
    return service_endpoint_request_finished_;
  }

  base::TimeTicks dns_resolution_start_time() const {
    return dns_resolution_start_time_;
  }

  base::TimeTicks dns_resolution_end_time() const {
    return dns_resolution_end_time_;
  }

  const NetLogWithSource& net_log();

  // Starts a Job. Will call one of Job::Delegate methods to notify results.
  void StartJob(Job* job,
                RequestPriority priority,
                const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
                RespectLimits respect_limits,
                bool enable_ip_based_pooling,
                bool enable_alternative_services,
                quic::ParsedQuicVersion quic_version,
                const NetLogWithSource& net_log);

  // Creates idle streams or sessions for `num_streams` be opened.
  // Note that this method finishes synchronously, or `callback` is called, once
  // `this` has enough streams/sessions for `num_streams` be opened. This means
  // that when there are two preconnect requests with `num_streams = 1`, all
  // callbacks are invoked when one stream/session is established (not two).
  int Preconnect(size_t num_streams,
                 quic::ParsedQuicVersion quic_version,
                 CompletionOnceCallback callback);

  // HostResolver::ServiceEndpointRequest::Delegate implementation:
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

  int WaitForSSLConfigReady(CompletionOnceCallback callback);

  SSLConfig GetSSLConfig();

  // Tries to process a single pending request/preconnect.
  void ProcessPendingJob();

  // Returns the number of total jobs in this manager.
  size_t JobCount() const { return jobs_.size(); }

  // Returns the number of in-flight attempts.
  size_t InFlightAttemptCount() const { return in_flight_attempts_.size(); }

  // Cancels all in-flight attempts.
  void CancelInFlightAttempts();

  // Called when `job` is going to be destroyed.
  void OnJobComplete(Job* job);

  // Cancels all jobs.
  void CancelJobs(int error);

  // Returns the number of pending jobs/preconnects. The number is
  // calculated by subtracting the number of in-flight attempts (excluding slow
  // attempts) from the number of total jobs.
  size_t PendingJobCount() const;
  size_t PendingPreconnectCount() const;

  // Returns the current load state.
  LoadState GetLoadState() const;

  // Called when the priority of `job` is set.
  void SetJobPriority(Job* job, RequestPriority priority);

  // Returns the highest priority in `jobs_` when there is at least one job.
  // Otherwise, returns IDLE assuming this manager is doing preconnects.
  RequestPriority GetPriority() const;

  // Returns true when `this` is blocked by the pool's stream limit.
  bool IsStalledByPoolLimit();

  // Called when the server required HTTP/1.1. Clears the current SPDY session
  // if exists. Subsequent jobs will fail while `this` is alive.
  void OnRequiredHttp11();

  // Called when the QuicTask owned by `this` is completed.
  void OnQuicTaskComplete(int rv, NetErrorDetails details);

  // Retrieves information on the current state of `this` as a base::Value.
  base::Value::Dict GetInfoAsValue();

  std::optional<int> GetQuicTaskResultForTesting() { return quic_task_result_; }

 private:
  // Represents failure of connection attempts. Used to notify job of completion
  // for failure cases.
  enum class FailureKind {
    kStreamFailed,
    kCertifcateError,
    kNeedsClientAuth,
  };

  // Represents reasons if future connection attempts could be blocked or not.
  enum class CanAttemptResult {
    kAttempt,
    kNoPendingJob,
    kBlockedStreamAttempt,
    kThrottledForSpdy,
    kReachedGroupLimit,
    kReachedPoolLimit,
  };

  // The state of TCP/TLS connection attempts.
  enum class TcpBasedAttemptState {
    kNotStarted,
    kAttempting,
    kSucceededAtLeastOnce,
    kAllAttemptsFailed,
  };

  using JobQueue = PriorityQueue<raw_ptr<Job>>;

  class InFlightAttempt;
  struct PreconnectEntry;

  const HttpStreamKey& stream_key() const;

  const SpdySessionKey& spdy_session_key() const;

  const QuicSessionKey& quic_session_key() const;

  HttpNetworkSession* http_network_session();
  SpdySessionPool* spdy_session_pool();
  QuicSessionPool* quic_session_pool();

  HttpStreamPool* pool();
  const HttpStreamPool* pool() const;

  bool UsingTls() const;

  bool RequiresHTTP11();

  void StartInternal(RequestPriority priority);

  void ResolveServiceEndpoint(RequestPriority initial_priority);

  void MaybeChangeServiceEndpointRequestPriority();

  // Called when service endpoint results have changed or finished.
  void ProcessServiceEndpointChanges();

  // Returns true when there is an active SPDY session that can be used for
  // on-going jobs after service endpoint results has changed. May notify jobs
  // of stream ready.
  bool CanUseExistingSessionAfterEndpointChanges();

  // Runs the stream attempt delay timer if stream attempts are blocked and the
  // timer is not running.
  void MaybeRunStreamAttemptDelayTimer();

  // Calculate SSLConfig if it's not calculated yet and `this` has received
  // enough information to calculate it.
  void MaybeCalculateSSLConfig();

  // Attempts QUIC sessions if QUIC can be used and `this` is ready to start
  // cryptographic connection handshakes.
  void MaybeAttemptQuic();

  // Attempts connections if there are pending jobs and IPEndPoints that
  // haven't failed. If `max_attempts` is given, attempts connections up to
  // `max_attempts`.
  void MaybeAttemptConnection(
      std::optional<size_t> max_attempts = std::nullopt);

  // Returns true if there are pending jobs and the pool and the group
  // haven't reached stream limits. If the pool reached the stream limit, may
  // close idle sockets in other groups. Also may cancel preconnects or trigger
  // `spdy_throttle_timer_`.
  bool IsConnectionAttemptReady();

  // Actual implementation of IsConnectionAttemptReady(), without having side
  // effects.
  CanAttemptResult CanAttemptConnection();

  // Returns true when connection attempts should be throttled because there is
  // an in-flight attempt and the destination is known to support HTTP/2.
  bool ShouldThrottleAttemptForSpdy();

  // Helper method to calculate pending jobs/preconnects.
  size_t PendingCountInternal(size_t pending_count) const;

  std::optional<IPEndPoint> GetIPEndPointToAttempt();
  std::optional<IPEndPoint> FindPreferredIPEndpoint(
      const std::vector<IPEndPoint>& ip_endpoints);

  // Calculate the failure kind to notify jobs of failure. Used to call one of
  // the job's methods.
  FailureKind DetermineFailureKind();

  // Notifies a failure to all jobs.
  void NotifyFailure();

  // Notifies a failure to a single job. Used by NotifyFailure().
  void NotifyJobOfFailure();

  // Notifies all preconnects of completion.
  void NotifyPreconnectsComplete(int rv);

  // Called after completion of a connection attempt to decriment stream
  // counts in preconnect entries. Invokes the callback of an entry when the
  // entry's stream counts becomes zero (i.e., `this` has enough streams).
  void ProcessPreconnectsAfterAttemptComplete(int rv);

  // Creates a text based stream and notifies the highest priority job.
  void CreateTextBasedStreamAndNotify(
      std::unique_ptr<StreamSocket> stream_socket,
      StreamSocketHandle::SocketReuseType reuse_type,
      LoadTimingInfo::ConnectTiming connect_timing);

  void CreateSpdyStreamAndNotify();

  void CreateQuicStreamAndNotify();

  void NotifyStreamReady(std::unique_ptr<HttpStream> stream,
                         NextProto negotiated_protocol);

  // Called when a SPDY session is ready to use. Cancels in-flight attempts.
  // Closes idle streams. Completes preconnects.
  void HandleSpdySessionReady();

  // Called when a QUIC session is ready to use. Cancels in-flight attempts.
  // Closes idle streams. Completes preconnects.
  void HandleQuicSessionReady();

  // Extracts an entry from `jobs_` of which priority is highest. The ownership
  // of the entry is moved to `notified_jobs_`.
  Job* ExtractFirstJobToNotify();

  void OnInFlightAttemptComplete(InFlightAttempt* raw_attempt, int rv);
  void OnInFlightAttemptTcpHandshakeComplete(InFlightAttempt* raw_attempt,
                                             int rv);
  void OnInFlightAttemptSlow(InFlightAttempt* raw_attempt);

  void HandleAttemptFailure(std::unique_ptr<InFlightAttempt> in_flight_attempt,
                            int rv);

  void OnSpdyThrottleDelayPassed();

  // Returns the delay for TCP-based stream attempts in favor of QUIC.
  base::TimeDelta GetStreamAttemptDelay();

  // Updates whether stream attempts should be blocked or not. May cancel
  // `stream_attempt_delay_timer_`.
  void UpdateStreamAttemptState();

  // Called when `stream_attempt_delay_timer_` is fired.
  void OnStreamAttemptDelayPassed();

  bool CanUseQuic();

  bool CanUseExistingQuicSession();

  // Mark QUIC brokenness if QUIC attempts failed but TCP/TLS attempts succeeded
  // or not attempted.
  void MaybeMarkQuicBroken();

  void MaybeComplete();

  const raw_ptr<Group> group_;

  const NetLogWithSource net_log_;

  RespectLimits respect_limits_ = RespectLimits::kRespect;

  bool enable_ip_based_pooling_ = true;

  bool enable_alternative_services_ = true;

  // Holds jobs that are waiting for notifications.
  JobQueue jobs_;
  // Holds jobs that are already notified results. We need to keep them to avoid
  // dangling pointers.
  std::set<raw_ptr<Job>> notified_jobs_;

  // Holds preconnect requests.
  std::set<std::unique_ptr<PreconnectEntry>, base::UniquePtrComparator>
      preconnects_;

  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
  bool service_endpoint_request_finished_ = false;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;

  // Set to true when `this` cannot handle further jobs. Used to ensure that
  // `this` doesn't accept further jobs while notifying the failure to the
  // existing jobs.
  bool is_failing_ = false;

  // Set to true when `CancelJobs()` is called.
  bool is_canceling_jobs_ = false;

  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;
  ConnectionAttempts connection_attempts_;

  // Set to an error from the latest stream attempt failure or network change
  // events. Used to notify delegates when all attempts failed.
  int error_to_notify_ = ERR_FAILED;

  // Set to a SSLInfo when an attempt has failed with a certificate error. Used
  // to notify jobs.
  std::optional<SSLInfo> cert_error_ssl_info_;

  // Set to a SSLCertRequestInfo when an attempt has requested a client cert.
  // Used to notify jobs.
  scoped_refptr<SSLCertRequestInfo> client_auth_cert_info_;

  // Allowed bad certificates from the newest job.
  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  // SSLConfig for all TLS connection attempts. Calculated after the service
  // endpoint request is ready to proceed cryptographic handshakes.
  // TODO(crbug.com/40812426): We need to have separate SSLConfigs when we
  // support multiple HTTPS RR that have different service endpoints.
  std::optional<SSLConfig> ssl_config_;
  std::vector<CompletionOnceCallback> ssl_config_waiting_callbacks_;

  std::set<std::unique_ptr<InFlightAttempt>, base::UniquePtrComparator>
      in_flight_attempts_;
  // The number of in-flight attempts that are treated as slow.
  size_t slow_attempt_count_ = 0;

  base::OneShotTimer spdy_throttle_timer_;
  bool spdy_throttle_delay_passed_ = false;

  // When true, try to use IPv6 for the next attempt first.
  bool prefer_ipv6_ = true;
  // Updated when a stream attempt failed. Used to calculate next IPEndPoint to
  // attempt.
  std::set<IPEndPoint> failed_ip_endpoints_;
  // Updated when a stream attempt is considered slow. Used to calculate next
  // IPEndPoint to attempt.
  std::set<IPEndPoint> slow_ip_endpoints_;

  // The current state of TCP/TLS connection attempts.
  TcpBasedAttemptState tcp_based_attempt_state_ =
      TcpBasedAttemptState::kNotStarted;

  // Initialized when one of an attempt is negotiated to use HTTP/2.
  base::WeakPtr<SpdySession> spdy_session_;

  // QUIC version that is known to be used for the destination, usually coming
  // from Alt-Svc.
  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();
  // Created when attempting QUIC sessions.
  std::unique_ptr<QuicTask> quic_task_;
  // Set when `quic_task_` is completed.
  std::optional<int> quic_task_result_;

  // The delay for TCP-based stream attempts in favor of QUIC.
  base::TimeDelta stream_attempt_delay_;
  // Set to true when stream attempts should be blocked.
  bool should_block_stream_attempt_ = false;
  base::OneShotTimer stream_attempt_delay_timer_;

  base::WeakPtrFactory<AttemptManager> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_H_
#define NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/stream_socket_handle.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/gurl.h"

namespace net {

class HttpNetworkSession;
class NetLog;
class HttpStreamKey;

// Drives connection attempts for a single destination.
//
// Maintains multiple in-flight Jobs for a single destination keyed by
// HttpStreamKey. Peforms DNS resolution and manages connection attempts.
// Delegates QUIC connection attempts to QuicTask. Upon successful HttpStream
// creations or fatal error occurrence, notify jobs of success or failure.
//
// Created by an HttpStreamPool::Group when new connection attempts are needed
// and destroyed when all jobs, in-flight attempts, and the QuicTask are
// completed.
class HttpStreamPool::AttemptManager
    : public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  class NET_EXPORT_PRIVATE QuicTask;

  // The state of an IPEndPoint. There is no success state. The absence of a
  // state for an endpoint means that we haven't yet attempted to connect to the
  // endpoint, or that a connection to the endpoint was successfully completed
  // and was not slow. Public for testing.
  enum class IPEndPointState {
    // The endpoint has failed.
    kFailed,
    // The endpoint is considered slow and haven't timed out yet.
    kSlowAttempting,
    // The endpoint was slow to connect, but the connection establishment
    // completed successfully.
    kSlowSucceeded,
  };

  using IPEndPointStateMap = std::map<IPEndPoint, IPEndPointState>;

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

  bool is_failing() const { return is_failing_; }

  int final_error_to_notify_jobs() const;

  base::TimeTicks dns_resolution_start_time() const {
    return dns_resolution_start_time_;
  }

  base::TimeTicks dns_resolution_end_time() const {
    return dns_resolution_end_time_;
  }

  const NetLogWithSource& net_log();

  // Starts a Job. Will call one of Job::Delegate methods to notify results.
  void StartJob(Job* job, const NetLogWithSource& request_net_log);

  // Creates idle streams or sessions for `num_streams` be opened.
  // Note that `job` will be notified once `this` has enough streams/sessions
  // for `num_streams` be opened. This means that when there are two preconnect
  // requests with `num_streams = 1`, all jobs are notified when one
  // stream/session is established (not two).
  void Preconnect(Job* job);

  // HostResolver::ServiceEndpointRequest::Delegate implementation:
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

  // Tries to process a single pending request/preconnect.
  void ProcessPendingJob();

  // Returns the number of jobs that haven't yet been notified success or
  // failure.
  size_t JobCount() const { return jobs_.size(); }

  // Returns the number of jobs that have already been notified success or
  // failure.
  size_t NotifiedJobCount() const { return notified_jobs_.size(); }

  // Returns the number of in-flight attempts.
  size_t InFlightAttemptCount() const { return in_flight_attempts_.size(); }

  // Cancels all in-flight attempts.
  void CancelInFlightAttempts(StreamSocketCloseReason reason);

  // Called when `job` is going to be destroyed.
  void OnJobComplete(Job* job);

  // Cancels all jobs.
  void CancelJobs(int error);

  // Cancels the QuicTask if it exists.
  void CancelQuicTask(int error);

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

  // Returns whether attempts is "SVCB-optional". See
  // https://www.rfc-editor.org/rfc/rfc9460.html#section-3-4
  // Note that the result can be changed over time while the DNS resolution is
  // still ongoing.
  bool IsSvcbOptional();

  // Called when the server required HTTP/1.1. Clears the current SPDY session
  // if exists. Subsequent jobs will fail while `this` is alive.
  void OnRequiredHttp11();

  // Called when the QuicTask owned by `this` is completed.
  void OnQuicTaskComplete(int rv, NetErrorDetails details);

  // Retrieves information on the current state of `this` as a base::Value.
  base::Value::Dict GetInfoAsValue() const;

  MultiplexedSessionCreationInitiator
  CalculateMultiplexedSessionCreationInitiator();

  std::optional<int> GetQuicTaskResultForTesting() { return quic_task_result_; }

  void SetIsFailingForTest(bool is_failing) { is_failing_ = is_failing; }

  QuicTask* quic_task_for_testing() const { return quic_task_.get(); }

  IPEndPointStateMap& ip_endpoint_states_for_testing() {
    return ip_endpoint_states_;
  }
  const IPEndPointStateMap& ip_endpoint_states_for_testing() const {
    return ip_endpoint_states_;
  }

  bool HasSSLConfigForTesting() const { return ssl_config_.has_value(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpStreamPoolAttemptManagerTest,
                           GetIPEndPointToAttempt);

  // Represents the initial attempt state of this manager.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(InitialAttemptState)
  enum class InitialAttemptState {
    kOther = 0,
    // CanUseQuic() && quic_version_.IsKnown() && !SupportsSpdy()
    kCanUseQuicWithKnownVersion = 1,
    // CanUseQuic() && quic_version_.IsKnown() && SupportsSpdy()
    kCanUseQuicWithKnownVersionAndSupportsSpdy = 2,
    // CanUseQuic() && !quic_version_.IsKnown() && !SupportsSpdy()
    kCanUseQuicWithUnknownVersion = 3,
    // CanUseQuic() && !quic_version_.IsKnown() && SupportsSpdy()
    kCanUseQuicWithUnknownVersionAndSupportsSpdy = 4,
    // !CanUseQuic() && quic_version_.IsKnown() && !SupportsSpdy()
    kCannotUseQuicWithKnownVersion = 5,
    // !CanUseQuic() && quic_version_.IsKnown() && SupportsSpdy()
    kCannotUseQuicWithKnownVersionAndSupportsSpdy = 6,
    // !CanUseQuic() && !quic_version_.IsKnown() && !SupportsSpdy()
    kCannotUseQuicWithUnknownVersion = 7,
    // !CanUseQuic() && !quic_version_.IsKnown() && SupportsSpdy()
    kCannotUseQuicWithUnknownVersionAndSupportsSpdy = 8,
    kMaxValue = kCannotUseQuicWithUnknownVersionAndSupportsSpdy,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:HttpStreamPoolInitialAttemptState)

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
    kAllEndpointsFailed,
  };

  std::string_view InitialAttemptStateToString(InitialAttemptState state);

  using JobQueue = PriorityQueue<raw_ptr<Job>>;

  class InFlightAttempt;

  static std::string_view CanAttemptResultToString(CanAttemptResult result);

  static std::string_view TcpBasedAttemptStateToString(
      TcpBasedAttemptState state);

  static std::string_view IPEndPointStateToString(IPEndPointState state);

  const HttpStreamKey& stream_key() const;

  const SpdySessionKey& spdy_session_key() const;

  const QuicSessionAliasKey& quic_session_alias_key() const;

  HttpNetworkSession* http_network_session() const;
  SpdySessionPool* spdy_session_pool() const;
  QuicSessionPool* quic_session_pool() const;

  HttpStreamPool* pool();
  const HttpStreamPool* pool() const;

  HostResolver::ServiceEndpointRequest* service_endpoint_request() {
    return service_endpoint_request_.get();
  }

  bool is_service_endpoint_request_finished() const {
    return service_endpoint_request_finished_;
  }

  int WaitForSSLConfigReady();

  void SetInitialAttemptState();
  InitialAttemptState CalculateInitialAttemptState();

  base::expected<SSLConfig, TlsStreamAttempt::GetSSLConfigError> GetSSLConfig(
      InFlightAttempt* attempt);

  bool UsingTls() const;

  bool RequiresHTTP11();

  void StartInternal(Job* job);

  void ResolveServiceEndpoint(RequestPriority initial_priority);

  void RestrictAllowedProtocols(NextProtoSet allowed_alpns);

  void MaybeChangeServiceEndpointRequestPriority();

  // Called when service endpoint results have changed or finished.
  void ProcessServiceEndpointChanges();

  // Returns true when there is an active SPDY/QUIC session that can be used for
  // on-going jobs after service endpoint results has changed. May notify jobs
  // of stream ready.
  bool CanUseExistingQuicSessionAfterEndpointChanges();
  bool CanUseExistingSpdySessionAfterEndpointChanges();

  // Calculate SSLConfig if it's not calculated yet and `this` has received
  // enough information to calculate it.
  void MaybeCalculateSSLConfig();

  // When SSLConfig is ready and the notification has not yet been sent,
  // notifies in-flight attempts that SSLConfig is ready.
  void MaybeNotifySSLConfigReady();

  // Attempts QUIC sessions if QUIC can be used and `this` is ready to start
  // cryptographic connection handshakes.
  void MaybeAttemptQuic();

  // Attempts connections if there are pending jobs and IPEndPoints that
  // haven't failed. If `exclude_ip_endpoint` is given, exclude the IPEndPoint
  // from attempts. If `max_attempts` is given, attempts connections up to
  // `max_attempts`.
  void MaybeAttemptConnection(
      std::optional<IPEndPoint> exclude_ip_endpoint = std::nullopt,
      std::optional<size_t> max_attempts = std::nullopt);

  // Returns true if there are pending jobs and the pool and the group
  // haven't reached stream limits. If the pool reached the stream limit, may
  // close idle sockets in other groups. Also may cancel preconnects or trigger
  // `spdy_throttle_timer_`.
  bool IsConnectionAttemptReady();

  // Actual implementation of IsConnectionAttemptReady(), without having side
  // effects.
  CanAttemptResult CanAttemptConnection() const;

  // Returns true only when there are no jobs that ignore the pool and group
  // limits.
  bool ShouldRespectLimits() const;

  // Returns true only when there are no jobs that disable IP based pooling.
  bool IsIpBasedPoolingEnabled() const;

  // Returns true only when there are no jobs that disable alternative services.
  bool IsAlternativeServiceEnabled() const;

  // Returns true when the destination is known to support HTTP/2. Note that
  // this could return false while initializing HttpServerProperties.
  bool SupportsSpdy() const;

  // Returns true when connection attempts should be throttled because there is
  // an in-flight attempt and the destination is known to support HTTP/2.
  bool ShouldThrottleAttemptForSpdy() const;

  // Calculates the maximum streams counts requested by preconnects.
  size_t CalculateMaxPreconnectCount() const;

  // Helper method to calculate pending jobs/preconnects.
  size_t PendingCountInternal(size_t pending_count) const;

  // Returns an IPEndPoint to attempt a connection. If `exclude_ip_endpoint` is
  // given, exclude the endpoint. Brief summary of the behavior are:
  //  * Try preferred address family first.
  //  * Prioritize unattempted or fast endpoints.
  //  * Fall back to slow but succeeded endpoints.
  //  * Use slow and attempting endpoints as the last option.
  //  * For a slow endpoint, skip the endpoint if there are enough attempts for
  //    the endpoint.
  // TODO(crbug.com/383606724): The current logic relies on rather naive and not
  // very well-founded heuristics. Write a design document and implement more
  // appropriate algorithm to pick an IPEndPoint.
  std::optional<IPEndPoint> GetIPEndPointToAttempt(
      std::optional<IPEndPoint> exclude_ip_endpoint = std::nullopt);
  void FindBetterIPEndPoint(const std::vector<IPEndPoint>& ip_endpoints,
                            std::optional<IPEndPoint> exclude_ip_endpoint,
                            std::optional<IPEndPointState>& current_state,
                            std::optional<IPEndPoint>& current_endpoint);
  bool HasEnoughAttemptsForSlowIPEndPoint(const IPEndPoint& ip_endpoint);

  // Called when this gets a fatal error. Notifies all jobs of the failure and
  // cancels in-flight TCP-based attempts and QuicTask's, if they exist.
  void HandleFinalError(int error);

  // Calculate the failure kind to notify jobs of failure. Used to call one of
  // the job's methods.
  FailureKind DetermineFailureKind();

  // Notifies a failure to a single job. Used by NotifyFailure().
  void NotifyJobOfFailure();

  // Notifies all preconnects of completion.
  void NotifyPreconnectsComplete(int rv);

  // Called after completion of a connection attempt to decrement stream
  // counts in preconnect entries. Invokes the callback of an entry when the
  // entry's stream counts is less than or equal to `active_stream_count`
  // (i.e., `this` has enough streams).
  void ProcessPreconnectsAfterAttemptComplete(int rv,
                                              size_t active_stream_count);

  // Helper methods to post a task to notify a job of preconnect completion. If
  // `this` is deleted the notification is canceled.
  void NotifyJobOfPreconnectCompleteLater(Job* job, int rv);
  void NotifyJobOfPreconnectComplete(Job* job, int rv);

  // Creates a text based stream and notifies the highest priority job.
  void CreateTextBasedStreamAndNotify(
      std::unique_ptr<StreamSocket> stream_socket,
      StreamSocketHandle::SocketReuseType reuse_type,
      LoadTimingInfo::ConnectTiming connect_timing);

  bool HasAvailableSpdySession() const;

  void CreateSpdyStreamAndNotify(base::WeakPtr<SpdySession> spdy_session);

  void CreateQuicStreamAndNotify();

  void NotifyStreamReady(std::unique_ptr<HttpStream> stream,
                         NextProto negotiated_protocol);

  // Called when a SPDY session is ready to use. Cancels in-flight attempts.
  // Closes idle streams. Completes preconnects and jobs.
  void HandleSpdySessionReady(base::WeakPtr<SpdySession> spdy_session,
                              StreamSocketCloseReason refresh_group_reason);

  // Called when a QUIC session is ready to use. Cancels in-flight attempts.
  // Closes idle streams. Completes preconnects.
  void HandleQuicSessionReady(StreamSocketCloseReason refresh_group_reason);

  // Extracts an entry from `jobs_` of which priority is highest. The ownership
  // of the entry is moved to `notified_jobs_`.
  Job* ExtractFirstJobToNotify();

  // Remove the pointeee of `job_pointer` from `jobs_`. May cancel in-flight
  // attempts when there are no limit ignoring jobs after removing the job and
  // in-flight attempts count is larger than the limit.
  raw_ptr<Job> RemoveJobFromQueue(JobQueue::Pointer job_pointer);

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

  // Runs the stream attempt delay timer if stream attempts are blocked and the
  // timer is not running. StreamAttemptDelayBehavior specifies when this method
  // is called.
  void MaybeRunStreamAttemptDelayTimer();

  // Cancels `stream_attempt_delay_timer_`.
  void CancelStreamAttemptDelayTimer();

  // Called when `stream_attempt_delay_timer_` is fired.
  void OnStreamAttemptDelayPassed();

  bool CanUseTcpBasedProtocols();

  bool CanUseQuic();

  bool CanUseExistingQuicSession();

  bool IsEchEnabled() const;

  // Returns true when `endpoint` can be used to attempt TCP/TLS connections.
  bool IsEndpointUsableForTcpBasedAttempt(const ServiceEndpoint& endpoint,
                                          bool svcb_optional);

  // Mark QUIC brokenness if QUIC attempts failed but TCP/TLS attempts succeeded
  // or not attempted.
  void MaybeMarkQuicBroken();

  base::Value::Dict GetStatesAsNetLogParams() const;

  // Returns true when this can complete.
  bool CanComplete() const;

  // Notifies `group_` that `this` has completed and can be destroyed.
  void MaybeComplete();

  // If `this` is ready to complete, posts a task to call MaybeComplete().
  void MaybeCompleteLater();

  const raw_ptr<Group> group_;

  const NetLogWithSource net_log_;

  // Keeps the initial attempt state. Set when `this` starts a job or
  // preconnect.
  std::optional<InitialAttemptState> initial_attempt_state_;

  NextProtoSet allowed_alpns_ = NextProtoSet::All();

  // Holds jobs that are waiting for notifications.
  JobQueue jobs_;
  // Holds jobs that are already notified results. We need to keep them to avoid
  // dangling pointers.
  std::set<raw_ptr<Job>> notified_jobs_;

  base::flat_set<raw_ptr<Job>> limit_ignoring_jobs_;

  base::flat_set<raw_ptr<Job>> ip_based_pooling_disabling_jobs_;

  base::flat_set<raw_ptr<Job>> alternative_service_disabling_jobs_;

  // Holds preconnect requests.
  std::set<raw_ptr<Job>> preconnect_jobs_;
  size_t notifying_preconnect_completion_count_ = 0;

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

  // An error code to notify jobs when `this` cannot make any further progress.
  // Set to an error from service endpoint resolution failure, the last stream
  // attempt failure, network change events, or QUIC task failure.
  std::optional<int> final_error_to_notify_jobs_;

  // Set to the most recent TCP-based attempt failure, if any.
  std::optional<int> most_recent_tcp_error_;

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
  bool ssl_config_ready_notified_ = false;

  std::set<std::unique_ptr<InFlightAttempt>, base::UniquePtrComparator>
      in_flight_attempts_;
  // The number of in-flight attempts that are treated as slow.
  size_t slow_attempt_count_ = 0;

  base::OneShotTimer spdy_throttle_timer_;
  bool spdy_throttle_delay_passed_ = false;

  // When true, try to use IPv6 for the next attempt first.
  bool prefer_ipv6_ = true;
  // Updated when a stream attempt is completed or considered slow. Used to
  // calculate next IPEndPoint to attempt.
  IPEndPointStateMap ip_endpoint_states_;

  // The current state of TCP/TLS connection attempts.
  TcpBasedAttemptState tcp_based_attempt_state_ =
      TcpBasedAttemptState::kNotStarted;

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

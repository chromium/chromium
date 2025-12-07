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
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_internal_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_ip_endpoint_state_tracker.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_pool.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/stream_socket_handle.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class HttpNetworkSession;
class NetLog;
class HttpStreamKey;

// Drives connection attempts for a single destination.
//
// Maintains multiple in-flight Jobs for a single destination keyed by
// HttpStreamKey. Peforms DNS resolution and manages connection attempts.
// Delegates QUIC connection attempts to QuicAttempt. Upon successful HttpStream
// creations or fatal error occurrence, notify jobs of success or failure.
//
// Created by an HttpStreamPool::Group when new connection attempts are needed
// and destroyed when all jobs, in-flight attempts, and the QuicAttempt are
// completed.
class HttpStreamPool::AttemptManager
    : public HostResolver::ServiceEndpointRequest::Delegate,
      public IPEndPointStateTracker::Delegate {
 public:
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

  // Time to delay connection attempts more than one when the destination is
  // known to support HTTP/2, to avoid unnecessary socket connection
  // establishments. See https://crbug.com/718576
  static constexpr base::TimeDelta kSpdyThrottleDelay = base::Milliseconds(300);

  // `group` must outlive `this`.
  AttemptManager(Group* group, NetLog* net_log);

  AttemptManager(const AttemptManager&) = delete;
  AttemptManager& operator=(const AttemptManager&) = delete;

  ~AttemptManager() override;

  const HttpStreamKey& stream_key() const;

  const SpdySessionKey& spdy_session_key() const;

  const QuicSessionAliasKey& quic_session_alias_key() const;

  HttpNetworkSession* http_network_session() const;
  SpdySessionPool* spdy_session_pool() const;
  QuicSessionPool* quic_session_pool() const;

  HttpStreamPool* pool();
  const HttpStreamPool* pool() const;

  Group* group() { return group_; }

  HostResolver::ServiceEndpointRequest* service_endpoint_request() {
    return service_endpoint_request_.get();
  }

  const perfetto::Track& track() const { return track_; }

  base::TimeTicks created_time() const { return created_time_; }

  bool using_tls() const { return is_using_tls_; }

  std::optional<InitialAttemptState> initial_attempt_state() const {
    return initial_attempt_state_;
  }

  bool is_shutting_down() const {
    return availability_state_ != AvailabilityState::kAvailable;
  }

  int final_error_to_notify_jobs() const;

  base::TimeTicks dns_resolution_start_time() const {
    return dns_resolution_start_time_;
  }

  base::TimeTicks dns_resolution_end_time() const {
    return dns_resolution_end_time_;
  }

  const NetLogWithSource& net_log();

  // Starts `job` for a stream request. Will call one of Job::Delegate methods
  // to notify results.
  void RequestStream(Job* job);

  // Creates idle streams or sessions for `num_streams` be opened.
  // Note that `job` will be notified once `this` has enough streams/sessions
  // for `num_streams` be opened. This means that when there are two preconnect
  // requests with `num_streams = 1`, all jobs are notified when one
  // stream/session is established (not two).
  void Preconnect(Job* job);

  // HostResolver::ServiceEndpointRequest::Delegate implementation:
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

  // IPEndPointStateTracker::Delegate implementation:
  HostResolver::ServiceEndpointRequest* GetServiceEndpointRequest() override;
  bool IsSvcbOptional() override;
  bool HasEnoughTcpBasedAttemptsForSlowIPEndPoint(
      const IPEndPoint& ip_endpoint) override;
  bool IsEndpointUsableForTcpBasedAttempt(const ServiceEndpoint& endpoint,
                                          bool svcb_optional) override;

  // Tries to process a single pending request/preconnect.
  void ProcessPendingJob();

  // Returns the number of request jobs that haven't yet been notified success
  // or failure.
  size_t RequestJobCount() const { return request_jobs_.size(); }

  // Returns the number of in-flight TCP based attempt slots.
  size_t TcpBasedAttemptSlotCount() const {
    return tcp_based_attempt_slots_.size();
  }

  // Cancels all in-flight TCP based attempts.
  void CancelTcpBasedAttempts(StreamSocketCloseReason reason);

  // Called when `job` that has not completed is destroyed.
  void OnJobCancelled(Job* job);

  // Cancels all jobs.
  void CancelJobs(int error, StreamSocketCloseReason cancel_reason);

  // Cancels the QuicAttempt if it exists.
  void CancelQuicAttempt(int error);

  // Returns the current load state.
  LoadState GetLoadState() const;

  // Called when the priority of `job` is set.
  void SetJobPriority(Job* job, RequestPriority priority);

  // Returns the highest priority in `jobs_` when there is at least one job.
  // Otherwise, returns IDLE assuming this manager is doing preconnects.
  RequestPriority GetPriority() const;

  // Returns true when `this` is blocked by the pool's stream limit.
  bool IsStalledByPoolLimit();

  // Returns the SSLConfig to use for TLS connections, not incorporating any
  // configuration based on the service endpoint.
  SSLConfig GetBaseSSLConfig();

  base::expected<ServiceEndpoint, TlsStreamAttempt::GetServiceEndpointError>
  GetServiceEndpoint(const IPEndPoint& endpoint);

  // Returns the total number of TCP based attempts. Calculated by adding up all
  // attempts in `tcp_based_attempt_slots_`, so avoid calling this method from
  // hot paths.
  size_t TotalTcpBasedAttemptCount() const;

  void OnTcpBasedAttemptComplete(TcpBasedAttempt* raw_attempt, int rv);
  void OnTcpBasedAttemptSlow(TcpBasedAttempt* raw_attempt);

  bool CanUseExistingQuicSession() const;

  // Runs the TCP based attempt delay timer if TCP based attempts are blocked
  // and the timer is not running. TcpBasedAttemptDelayBehavior specifies when
  // this method is called.
  void MaybeRunTcpBasedAttemptDelayTimer();

  // Called when the QuicAttempt owned by `this` is completed.
  void OnQuicAttemptComplete(QuicAttemptOutcome result);

  // Called when the QuicAttempt owned by `this` is slow.
  void OnQuicAttemptSlow();

  // Retrieves information on the current state of `this` as a base::Value.
  base::Value::Dict GetInfoAsValue() const;

  base::Value::Dict GetStatesAsNetLogParams() const;

  MultiplexedSessionCreationInitiator
  CalculateMultiplexedSessionCreationInitiator();

  std::optional<int> GetQuicAttemptResultForTesting() {
    return quic_attempt_result_;
  }

  base::WeakPtr<AttemptManager> GetWeakPtrForTesting() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  QuicAttempt* quic_attempt_for_testing() const { return quic_attempt_.get(); }

  void SetOnCompleteCallbackForTesting(base::OnceClosure callback);

 private:
  // Represents the availability of this instance. If not kAvailable, `this`
  // can't handle new Jobs and this should not have in-flight attempts.
  enum class AvailabilityState {
    // Can handle new Jobs and make connection attempts.
    kAvailable = 0,
    // Is in preparation of a successful completion.
    kDraining = 1,
    // Is handling a fatal error.
    kFailing = 2,
  };

  // Represents reasons if future connection attempts could be blocked or not.
  enum class CanAttemptResult {
    kAttempt,
    kNoPendingJob,
    kBlockedTcpBasedAttempt,
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
  using PreconnectJobs = std::set<raw_ptr<Job>>;

  static std::string_view CanAttemptResultToString(CanAttemptResult result);

  static std::string_view TcpBasedAttemptStateToString(
      TcpBasedAttemptState state);

  bool is_service_endpoint_request_finished() const {
    return service_endpoint_request_finished_;
  }

  void SetInitialAttemptState();
  InitialAttemptState CalculateInitialAttemptState();

  void StartInternal(Job* job);

  void ResolveServiceEndpoint(RequestPriority initial_priority);

  // Helper methods to reset ServiceEndpointRequest later.
  // TODO(crbug.com/421299722, crbug.com/397597592): Remove these helper
  // methods and reset ServiceEndpointRequest without PostTask(). We need to
  // update the HostResolver's object management first. See comment #8 of
  // crbug.com/397597592.
  void ResetServiceEndpointRequestLater();
  void ResetServiceEndpointRequest();

  void RestrictAllowedProtocols(NextProtoSet allowed_alpns);

  void MaybeChangeServiceEndpointRequestPriority();

  // Called when service endpoint results have changed or finished.
  void ProcessServiceEndpointChanges();

  // Returns an active QUIC session when there is an active QUIC session that
  // can be used for on-going jobs after service endpoint results have changed.
  QuicChromiumClientSession* CanUseExistingQuicSessionAfterEndpointChanges();

  // Returns an active SPDY session when there is an active SPDY session that
  // can be used for on-going jobs after service endpoint results have changed.
  base::WeakPtr<SpdySession> CanUseExistingSpdySessionAfterEndpointChanges();

  // If `this` is ready to start cryptographic handshakes, notifies TCP based
  // attempts that SSLConfigs are ready.
  void MaybeNotifySSLConfigReady();

  // Attempts QUIC sessions if QUIC can be used and `this` is ready to start
  // cryptographic connection handshakes.
  void MaybeAttemptQuic();

  // Attempts connections if there are pending jobs and IPEndPoints that
  // haven't failed.
  void MaybeAttemptTcpBased();

  // Creates and starts a TCP based attempt.
  void CreateAndStartTcpBasedAttempt(bool using_tls,
                                     IPEndPoint ip_endpoint,
                                     TcpBasedAttemptSlot* slot);

  // Finds or allocates a TcpBasedAttemptSlot for `ip_endpoint`. If under the
  // group limit, allocates a new slot. Otherwise, tries to find an existing
  // slot that doesn't have an attempt for the same address family as
  // `ip_endpoint`. Returns nullptr when there is no available slot.
  TcpBasedAttemptSlot* FindTcpBasedAttemptSlot(const IPEndPoint& ip_endpoint);

  // Cancels `raw_slot` and removes it from `tcp_based_attempt_slots_`.
  void CancelTcpBasedAttemptSlot(
      TcpBasedAttemptSlot* raw_slot,
      std::optional<StreamSocketCloseReason> reason = std::nullopt);

  // Returns true if there are pending jobs and the pool and the group
  // haven't reached stream limits. If the pool reached the stream limit, may
  // close idle sockets in other groups. Also may cancel preconnects or trigger
  // `spdy_throttle_timer_`.
  bool IsTcpBasedAttemptReady();

  // When an attempt to one address faimily (e.g., IPv4) is slow, this allows a
  // new attempt to the other address family (e.g. IPv6) to be started in
  // parallel. This is allowed even if the group's stream limit has been reached
  // because the new attempt reuses the same "slot" as the slow attempt.
  //
  // Returns true if there is a slow attempt for one address family and no
  // corresponding attempt for the other has been started yet.
  bool CanStartFallbackTcpBasedAttempt() const;

  // Actual implementation of IsConnectionAttemptReady(), without having side
  // effects, other than populating `supports_spdy_`, if needed.
  CanAttemptResult CanAttemptConnection() const;

  // Returns true only when there are no jobs that ignore the pool and group
  // limits.
  bool ShouldRespectLimits() const;

  // Returns true only when there are no jobs that disable IP based pooling for
  // HTTP/2. Note that this does nothing with QUIC.
  bool IsIpBasedPoolingEnabledForH2() const;

  // Returns true when the destination is known to support HTTP/2. The value is
  // retrieved from HttpServerProperties and cached on first invocation, as
  // calculating it can be expensive. If HttpServerProperties are still loading
  // on startup, could be incorrectly set to false.
  bool SupportsSpdy() const;

  // Returns true when connection attempts should be throttled because there is
  // an in-flight TCP based attempt and the destination is known to support
  // HTTP/2.
  bool ShouldThrottleAttemptForSpdy() const;

  // Calculates the maximum streams counts requested by preconnects.
  size_t CalculateMaxPreconnectCount() const;

  // Calculates the number of TCP based attempts required to satisfy
  // preconnects.
  size_t CalculateRequiredTcpBasedAttemptForPreconnect() const;

  // Returns the number of TCP based attempt slots that are not considered as
  // slow.
  size_t NonSlowTcpBasedAttemptCount() const;

  // Returns a QUIC endpoint to make a connection attempt. See the comments in
  // QuicSessionPool::SelectQuicVersion() for the criteria to select a QUIC
  // endpoint.
  std::optional<QuicEndpoint> GetQuicEndpointToAttempt();

  // Called when this gets a fatal error. Notifies all jobs of the failure and
  // cancels in-flight TCP based attempts and QuicAttempt's, if they exist.
  void HandleFinalError(int error);

  // Notifies the final failure to all request jobs.
  void NotifyRequestJobsOfFailure();

  // Notifies a failure to a single request job.
  // Note that `connection_attempts` is a list of failed IPEndPoints, not
  // TcpBasedAttempt or QuicAttempt.
  void NotifySingleRequestJobOfFailure(
      Job& job,
      int error,
      const ConnectionAttempts& connection_attempts);

  // Notifies all preconnects of completion.
  void NotifyPreconnectsComplete(int rv);

  // Called after completion of a connection attempt to decrement stream
  // counts in preconnect entries. Invokes the callback of an entry when the
  // entry's stream counts is less than or equal to `active_stream_count`
  // (i.e., `this` has enough streams).
  void ProcessPreconnectsAfterAttemptComplete(int rv,
                                              size_t active_stream_count);

  // Notifies a job of preconnect completion.
  void NotifyJobOfPreconnectComplete(PreconnectJobs::iterator job_it, int rv);

  // Creates a text based stream and Notifies the highest priority job.
  void CreateTextBasedStreamAndNotify(
      std::unique_ptr<StreamSocket> stream_socket,
      StreamSocketHandle::SocketReuseType reuse_type,
      LoadTimingInfo::ConnectTiming connect_timing);

  bool HasAvailableSpdySession() const;

  void MaybeStartDraining();

  void MaybeCreateSpdyStreamAndNotify(base::WeakPtr<SpdySession> spdy_session,
                                      SessionSource session_source);

  void MaybeCreateQuicStreamAndNotify(QuicChromiumClientSession* quic_session,
                                      SessionSource session_source);

  void NotifyStreamReady(std::unique_ptr<HttpStream> stream,
                         NextProto negotiated_protocol,
                         std::optional<SessionSource> session_source);

  // Called when a SPDY session is ready to use. Cancels in-flight attempts.
  // Closes idle streams. Completes request/preconnect jobs.
  void HandleSpdySessionReady(base::WeakPtr<SpdySession> spdy_session,
                              StreamSocketCloseReason refresh_group_reason);

  // Called when a QUIC session is ready to use. Cancels in-flight attempts.
  // Closes idle streams. Completes request/preconnect jobs.
  void HandleQuicSessionReady(QuicChromiumClientSession* quic_session,
                              StreamSocketCloseReason refresh_group_reason);

  // Called when a job is done, due to success, failure, or cancellation. `job`
  // must have already been removed from `request_jobs_` and `preconnect_jobs_`,
  // but may still be in other job lists (which this method will remove the job
  // from).
  void OnJobDone(Job* job);

  // Extracts an entry from `request_jobs_` of which priority is highest. The
  // ownership of the entry is moved to `notified_jobs_`.
  Job* ExtractFirstJobToNotify();

  // Remove the pointeee of `job_pointer` from `request_jobs_`. May cancel
  // in-flight TCP based attempts when there are no limit ignoring jobs after
  // removing the job and in-flight TCP based attempts count is larger than the
  // limit.
  Job* RemoveJobFromQueue(JobQueue::Pointer job_pointer);

  // Transfers the ownership of `raw_slot` to the caller.
  std::unique_ptr<TcpBasedAttemptSlot> ExtractTcpBasedAttemptSlot(
      TcpBasedAttemptSlot* raw_slot);

  // Transfers the ownership of `raw_attempt` to the caller. If `rv` is OK, also
  // removes the corresponding slot from `tcp_based_attempt_slots_`.
  std::unique_ptr<TcpBasedAttempt> ExtractTcpBasedAttempt(
      TcpBasedAttempt* raw_attempt,
      int rv);

  void HandleTcpBasedAttemptFailure(
      std::unique_ptr<TcpBasedAttempt> tcp_based_attempt,
      int rv);

  void OnSpdyThrottleDelayPassed();

  // Returns the delay for TCP based attempts in favor of QUIC.
  base::TimeDelta GetTcpBasedAttemptDelay();

  // Updates whether TCP based attempts should be blocked or not. May cancel
  // `tcp_based_attempt_delay_timer_`.
  void UpdateTcpBasedAttemptState();

  // Cancels `tcp_based_attempt_delay_timer_`.
  void CancelTcpBasedAttemptDelayTimer();

  // Called when `tcp_based_attempt_delay_timer_` is fired.
  void OnTcpBasedAttemptDelayPassed();

  bool CanUseTcpBasedProtocols();

  bool CanUseQuic() const;

  bool IsEchEnabled() const;

  // Mark QUIC brokenness if QUIC attempts failed but TCP/TLS attempts succeeded
  // or not attempted.
  void MaybeMarkQuicBroken();

  base::Value::Dict GetTcpBasedAttemptSlotsAsValue() const;

  // Returns true when this can complete.
  bool CanComplete() const;

  // Notifies `group_` that `this` has completed and can be destroyed.
  void MaybeComplete();

  // If `this` is ready to complete, posts a task to call MaybeComplete().
  void MaybeCompleteLater();

  const raw_ptr<Group> group_;

  const NetLogWithSource net_log_;

  // For trace events.
  const perfetto::Track track_;
  const perfetto::Flow flow_;

  const base::TimeTicks created_time_;

  // Whether the destination is using TLS or not.
  const bool is_using_tls_;

  // Keeps the initial attempt state. Set when `this` attempts a TCP based
  // attempt for the first time.
  std::optional<InitialAttemptState> initial_attempt_state_;

  // List of allowed protocols. Excludes protocols when, e.g., one protocol or
  // another is marked as broken or is disabled for one or more jobs. Never
  // includes NextProto::kProtoUnknown, since that's an alias for any protocol.
  NextProtoSet allowed_alpns_;

  // Holds request jobs that are waiting for notifications.
  JobQueue request_jobs_;
  // Holds preconnect jobs that are waiting for notifications.
  PreconnectJobs preconnect_jobs_;

  base::flat_set<raw_ptr<Job>> limit_ignoring_jobs_;

  base::flat_set<raw_ptr<Job>> ip_based_pooling_disabling_jobs_;

  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
  bool service_endpoint_request_finished_ = false;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;

  AvailabilityState availability_state_ = AvailabilityState::kAvailable;

  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;
  ConnectionAttempts connection_attempts_;

  // TODO(crbug.com/406936736): Remove this once we identify the cause of the
  // bug.
  bool ip_matching_spdy_session_found_ = false;

  // An error code to notify jobs when `this` cannot make any further progress.
  // Set to an error from service endpoint resolution failure, the last stream
  // attempt failure, network change events, or QUIC task failure.
  std::optional<int> final_error_to_notify_jobs_;

  // Set to the most recent TCP based attempt failure, if any.
  std::optional<int> most_recent_tcp_error_;

  // Set to a SSLInfo when an attempt has failed with a certificate error. Used
  // to notify jobs.
  std::optional<SSLInfo> cert_error_ssl_info_;

  // Set to a SSLCertRequestInfo when an attempt has requested a client cert.
  // Used to notify jobs.
  scoped_refptr<SSLCertRequestInfo> client_auth_cert_info_;

  // Base SSLConfig for TCP based attempts, Allowed bad certificates are set
  // from the newest job.
  std::optional<SSLConfig> base_ssl_config_;

  std::set<std::unique_ptr<TcpBasedAttemptSlot>, base::UniquePtrComparator>
      tcp_based_attempt_slots_;

  base::OneShotTimer spdy_throttle_timer_;
  bool spdy_throttle_delay_passed_ = false;

  // Tracks the states of IPEndPoints.
  IPEndPointStateTracker ip_endpoint_state_tracker_{this};

  // The current state of TCP/TLS connection attempts.
  TcpBasedAttemptState tcp_based_attempt_state_ =
      TcpBasedAttemptState::kNotStarted;

  // QUIC version that is known to be used for the destination, usually coming
  // from Alt-Svc.
  quic::ParsedQuicVersion quic_version_ =
      quic::ParsedQuicVersion::Unsupported();
  // Created when attempting a QUIC session.
  std::unique_ptr<QuicAttempt> quic_attempt_;
  // Set when `quic_attempt_` is completed.
  std::optional<int> quic_attempt_result_;

  // Whether the host has previously been observed to support SPDY. Populated as
  // needed, from HttpServerProperties. Set to false (without updating
  // HttpServerProperties) if an HTTP/1.x connection is established.
  //
  // Mutable because setting it does not actually modify AttemptManager state,
  // and it's read/population from methods that are otherwise const.
  //
  // To check the value, call SupportsSpdy(), which will populate it if needed.
  mutable std::optional<bool> supports_spdy_;

  // The delay for TCP based stream attempts in favor of QUIC.
  base::TimeDelta tcp_based_attempt_delay_;
  // Set to true when TCP based attempts should be blocked.
  bool should_block_tcp_based_attempt_ = false;
  base::OneShotTimer tcp_based_attempt_delay_timer_;

  base::OnceClosure on_complete_callback_for_testing_;

  base::WeakPtrFactory<AttemptManager> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_H_

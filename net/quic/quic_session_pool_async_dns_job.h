// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_ASYNC_DNS_JOB_H_
#define NET_QUIC_QUIC_SESSION_POOL_ASYNC_DNS_JOB_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/base/reconnect_notifier.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolution_details.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_job.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

// An AsyncDnsJob is a QuicSessionPool::Job that handles direct connections to
// the destination, resolving the hostname with
// HostResolver::ServiceEndpointRequest.
//
// The job drives DNS resolution, owns the resolver results and the DNS
// timestamps, and translates the outcome into the one-shot QuicSessionRequest
// signals. Connection establishment is delegated to owned EndpointConnectors,
// which ask for their next candidate through TakeNextCandidate() and report
// back through OnAttemptFailed(), OnSessionCreationDecided() and
// OnConnectorComplete().
//
// The job holds a primary and a secondary connector slot. The slot a
// connector occupies decides which address families the job hands it. While
// the secondary slot is empty the primary connector may use both families and
// prefers IPv6. The secondary slot is filled when the slow timer fires, and
// from then on the primary takes IPv6 and the secondary takes IPv4 while DNS
// is resolving. Once DNS resolution completes, connectors may attempt the
// other family's remaining candidates when their preferred family is exhausted.
// The first connector to succeed settles the job, and the other one is
// destroyed together with the attempt it had in flight.
//
// The job acts on partial resolver results once the endpoints are crypto
// ready. It tries to advance every connector that has no attempt in flight
// again on later results, so a connector that ran out of untried candidates
// can continue on endpoints that arrive later. A resolver error never changes
// the outcome once a connector exists. It only decides when running out of
// candidates becomes a failure.
class QuicSessionPool::AsyncDnsJob
    : public QuicSessionPool::Job,
      public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  // A resolved endpoint usable for QUIC, paired with the QUIC version
  // selected for it.
  struct UsableEndpoint {
    UsableEndpoint(ServiceEndpoint endpoint,
                   quic::ParsedQuicVersion quic_version);
    ~UsableEndpoint();

    UsableEndpoint(const UsableEndpoint&);
    UsableEndpoint& operator=(const UsableEndpoint&);
    UsableEndpoint(UsableEndpoint&&);
    UsableEndpoint& operator=(UsableEndpoint&&);

    ServiceEndpoint endpoint;
    quic::ParsedQuicVersion quic_version =
        quic::ParsedQuicVersion::Unsupported();
  };

  // One candidate of the job's usable results, with everything an attempt to
  // it needs. Copied out of the results so that it stays valid across the
  // calls made while starting the attempt. Two candidates are the same only
  // when all three fields match.
  struct Candidate {
    bool operator==(const Candidate& other) const = default;

    IPEndPoint ip_endpoint;
    ConnectionEndpointMetadata metadata;
    quic::ParsedQuicVersion quic_version =
        quic::ParsedQuicVersion::Unsupported();
  };

  // Parameters for constructing a QuicSessionAttempt, captured from the job's
  // state at attempt start.
  struct AttemptParams {
    AttemptParams();
    ~AttemptParams();

    AttemptParams(const AttemptParams&);
    AttemptParams& operator=(const AttemptParams&);
    AttemptParams(AttemptParams&&);
    AttemptParams& operator=(AttemptParams&&);

    int cert_verify_flags = 0;
    base::TimeTicks dns_resolution_start_time;
    base::TimeTicks dns_resolution_end_time;
    std::optional<ResolutionDetails> resolution_details;
    bool retry_on_alternate_network_before_handshake = false;
    bool use_dns_aliases = false;
    std::set<std::string> dns_aliases;
    MultiplexedSessionCreationInitiator session_creation_initiator =
        MultiplexedSessionCreationInitiator::kUnknown;
    QuicConnectionReuseDetails quic_connection_reuse_details;
    std::optional<ConnectionManagementConfig> connection_management_config;
  };

  // How the job obtained its session. Attempt-based results use connector
  // identity rather than the connector's final slot because connectors can
  // change slots. Failed jobs use kNone. The non-kNone values are recorded in
  // Net.QuicSession.AsyncDnsJob.SuccessSource.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(QuicSessionPoolAsyncDnsJobSuccessSource)
  enum class SuccessSource {
    kNone = 0,
    // Another request activated a session for the same key.
    kActiveSession = 1,
    // The job found an existing session through IP pooling.
    kIpPooling = 2,
    // The initial connector's first attempt succeeded.
    kInitialConnectorFirstAttempt = 3,
    // A later attempt by the initial connector succeeded.
    kInitialConnectorLaterAttempt = 4,
    // An attempt by the connector created when the slow timer fired succeeded.
    kSlowTimerConnector = 5,
    kMaxValue = kSlowTimerConnector,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:QuicSessionPoolAsyncDnsJobSuccessSource)

  AsyncDnsJob(
      QuicSessionPool* pool,
      quic::ParsedQuicVersion quic_version,
      HostResolver* host_resolver,
      QuicSessionAliasKey key,
      std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
      bool retry_on_alternate_network_before_handshake,
      RequestPriority priority,
      bool use_dns_aliases,
      bool require_dns_https_alpn,
      int cert_verify_flags,
      MultiplexedSessionCreationInitiator session_creation_initiator,
      QuicConnectionReuseDetails quic_connection_reuse_details,
      std::optional<ConnectionManagementConfig> connection_management_config,
      const NetLogWithSource& net_log);

  ~AsyncDnsJob() override;

  // QuicSessionPool::Job implementation.
  int Run(CompletionOnceCallback callback) override;
  void SetRequestExpectations(QuicSessionRequest* request) override;
  void UpdatePriority(RequestPriority old_priority,
                      RequestPriority new_priority) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;

  // HostResolver::ServiceEndpointRequest::Delegate implementation.
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

  void MaybePromoteStaleConnectors();
  bool IsStaleConnector(const EndpointConnector& connector) const;
  bool IsEndpointInFreshList(const IPEndPoint& endpoint) const;

  // Returns the endpoints in the current resolver results that are usable
  // for QUIC, applying QUIC version selection to each endpoint. This is the
  // only interpretation of the resolver results. The IP-pooling check and
  // the connector's candidate selection both consume it. The returned
  // reference is invalidated by the next resolver event, so do not hold it
  // across one.
  const std::vector<UsableEndpoint>& GetUsableEndpoints() const;

  // Returns the parameters for constructing a QuicSessionAttempt, read from
  // the job's current state. Call at attempt start.
  AttemptParams GetAttemptParams() const;

  // Tries to pool to an existing session whose peer IP matches one of the
  // usable endpoints. Returns true when pooled.
  bool MaybePoolToExistingSession();

  // Claims and returns the next candidate `connector` may attempt, or nothing
  // when no such candidate is available. The allowed address families come from
  // the slot `connector` occupies while resolution is in flight, and each
  // connector falls back to the other family once resolution finishes and its
  // preferred family is exhausted. Each candidate is handed out at most once
  // per job. The same IP listed under two ServiceEndpoints with different
  // metadata is two candidates, because the metadata can decide whether the
  // handshake succeeds. An attempt with a stale ECH config can fail where a
  // plain A/AAAA endpoint to the same IP works.
  std::optional<Candidate> TakeNextCandidate(
      const EndpointConnector& connector);

  // Called by a connector every time one of its attempts failed. The job
  // keeps the most recent failure and reports it when it runs out of
  // candidates. The two connectors do not complete in the order their
  // attempts failed, so the job records the failures itself.
  void OnAttemptFailed(int rv, const NetErrorDetails& details);

  // Called by `connector` when its attempt finished creating its session.
  // ERR_IO_PENDING means the session was created and its crypto handshake
  // is still in flight. A failed result is held until the job's outcome is
  // known, because a later attempt may still create a session.
  void OnSessionCreationDecided(int rv, const EndpointConnector& connector);

  // Called by `connector` when it settled successfully or when it ran out of
  // untried candidates. A connector settles successfully when its attempt
  // succeeds or when it finds an existing session through IP pooling. The job
  // then destroys the other connector together with its in-flight attempt.
  // Running out of candidates fails the job only when the other connector has
  // nothing in flight and DNS has finished.
  void OnConnectorComplete(int rv, EndpointConnector& connector);

  // Returns the name of the slot `connector` occupies. For logging.
  const char* SlotName(const EndpointConnector& connector) const;

  // Called immediately before `connector` starts an attempt. Updates the
  // attempt metrics, logs the attempt, and returns its job-wide identifier.
  int OnAttemptStarted(const EndpointConnector& connector,
                       const Candidate& candidate,
                       base::TimeTicks start_time);

 private:
  struct ConnectionState {
    ConnectionState();
    ~ConnectionState();

    // Runs while only the primary slot is filled. On expiry the job fills the
    // secondary slot so that an IPv4 attempt runs next to the IPv6 one.
    base::OneShotTimer slow_timer;
    // Set once the slow timer was armed. The timer is armed at most once per
    // connection state.
    bool slow_timer_started = false;
    // The connector that may use IPv6 once both slots are filled, and both
    // families while the secondary slot is empty. Created when the job first
    // has something to attempt.
    std::unique_ptr<EndpointConnector> primary_connector;
    // The connector that may use IPv4 only. Created when the slow timer fires.
    std::unique_ptr<EndpointConnector> secondary_connector;
    // The candidates already handed out for an attempt. Lives here because the
    // connectors of one state must not attempt the same candidate twice. A
    // vector because ParsedQuicVersion can be compared but not ordered.
    std::vector<Candidate> claimed_candidates_;
  };

  ConnectionState& GetState(const EndpointConnector& connector);
  const ConnectionState& GetState(const EndpointConnector& connector) const;

  // The result and the error details of one failed attempt.
  struct AttemptFailure {
    int rv = OK;
    NetErrorDetails details;
  };

  int DoResolveHost();
  int DoResolveHostComplete(int rv);

  // Tries IP pooling and then advances the connectors. Returns the job result
  // when the job settled, ERR_IO_PENDING when an attempt is in flight, or
  // std::nullopt when the job is waiting for more resolver results.
  std::optional<int> ProcessServiceEndpointResults();

  // Advances `connector` when it has no attempt in flight. Returns what the
  // connector returned, ERR_IO_PENDING when it kept the attempt it already
  // had, and std::nullopt when there is no such connector.
  std::optional<int> AdvanceConnector(EndpointConnector* connector);

  // Advances both slots and arms the slow timer once the primary connector
  // has an attempt in flight. Returns OK when a connector settled the job, in
  // which case the other connector has been destroyed. Returns ERR_IO_PENDING
  // while an attempt is in flight, and std::nullopt when nothing could be
  // started.
  // Tries to advance the connectors of a specific state.
  std::optional<int> AdvanceConnectors(ConnectionState& state);

  // Called when `connector` settled the job. Logs how it settled, destroys the
  // other connector together with the attempt it had in flight, and moves
  // `connector` into the primary slot when it was in the secondary one.
  void DestroyOtherConnector(const EndpointConnector& connector);

  // Starts the slow timer when the primary connector has its first attempt in
  // flight. The deadline is kept while the primary moves on to other
  // candidates, and the timer never starts a second time.
  void MaybeStartSlowTimer(ConnectionState& state);

  // Fills the secondary slot so that the job can run two attempts at once.
  // Called by the slow timer. The slot is filled whether or not the primary
  // connector has an attempt in flight at that moment.
  void OnSlowTimer(ConnectionState* state);

  // True when a connector could start an attempt as soon as the job has a
  // candidate for it.
  bool HasWaitingConnector(const ConnectionState& state) const;

  // True when a connector has an attempt in flight.
  bool HasAttemptInFlight() const;


  // Returns the result of the most recently failed attempt, or nothing while
  // no attempt failed.
  std::optional<int> LastFailureResult() const;

  // Delivers any undelivered session creation result and completes the job.
  void CompleteJob(int rv);

  // Delivers the host resolution signal if not already notified, and completes
  // the job if it settled.
  void MaybeNotifyHostResolutionAndComplete(int rv);

  // Fires the requests' one-shot host resolution signal. Called at most
  // once.
  void NotifyRequestsOfHostResolution(int rv);

  // Fires the requests' one-shot session creation signal. Called at most
  // once.
  void NotifyRequestsOfSessionCreation(int rv);

  // Sets the DNS resolution end time if not already set. Only the first call
  // takes effect. The end time reflects how long the job was blocked on DNS,
  // matching the meaning of TcpConnectJob's domain_lookup_end.
  void MaybeSetDnsResolutionEndTime();

  // Returns whether the client should be SVCB-optional when connecting to
  // `endpoints`.
  bool IsSvcbOptional(base::span<const ServiceEndpoint> endpoints) const;

  // Logs the job's outcome. Logging only.
  void LogJobComplete(int rv) const;

  // Logs the resolver's final result. Logging only.
  void LogServiceEndpointRequestFinished(int rv) const;

  // Records this job's histograms. Called once when the job finishes.
  void RecordMetrics(int rv) const;

  const quic::ParsedQuicVersion quic_version_;
  const raw_ptr<HostResolver> host_resolver_;
  const bool use_dns_aliases_;
  const bool require_dns_https_alpn_;
  const int cert_verify_flags_;
  const bool retry_on_alternate_network_before_handshake_;
  const std::optional<ConnectionManagementConfig> connection_management_config_;

  // Set when the resolver reported its final result.
  bool resolution_finished_ = false;
  // Set when the one-shot host resolution signal fired. This can happen
  // before the resolver finishes.
  bool host_resolution_notified_ = false;
  // Set when the one-shot session creation signal fired. Later creation
  // results are dropped.
  bool session_creation_notified_ = false;
  // A failed session creation result waiting for the job's outcome. Set
  // while another attempt could still create a session.
  std::optional<int> held_session_creation_result_;
  // Cleared after the first IP pooling check that saw endpoints. Later
  // checks do not record negative metric entries.
  bool log_negative_ip_pool_result_ = true;

  // The number of attempts the connectors of this job started. Reported when
  // the job settles.
  size_t attempt_count_ = 0;
  // Tracks how many endpoints have already been checked for pooling to avoid
  // wasteful active_sessions_ scans.
  size_t num_endpoints_evaluated_for_pooling_ = 0;
  // True if stale endpoints were evaluated for pooling.
  bool stale_endpoints_evaluated_for_pooling_ = false;

  // Set before every successful completion.
  SuccessSource success_source_ = SuccessSource::kNone;
  // The most recently failed attempt of either connector. The job's failure
  // is reported from here.
  std::optional<AttemptFailure> last_attempt_failure_;
  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
  // Usable endpoints derived from the current resolver results. Reset on
  // every resolver event and recomputed on the next GetUsableEndpoints()
  // call.
  mutable std::optional<std::vector<UsableEndpoint>> usable_endpoints_;
  base::TimeTicks dns_resolution_start_time_;
  // Stamped once, when the job first stops waiting on DNS. Not moved when
  // the resolver finishes later.
  base::TimeTicks dns_resolution_end_time_;
  // When the resolver reported its final result. Null while resolution is in
  // flight. Distinct from `dns_resolution_end_time_`, which is stamped at the
  // first usable partial results.
  base::TimeTicks resolution_finished_time_;
  // When the job's first attempt started. Null while no attempt started.
  base::TimeTicks first_attempt_start_time_;
  // When the successful attempt started.
  base::TimeTicks successful_attempt_start_time_;

  // Tracks connection attempts for fresh DNS results. This is the primary
  // state machine for standard connection attempts.
  ConnectionState fresh_state_;
  // Tracks connection attempts based on stale DNS results when optimistic DNS
  // is enabled. Operates independently until fresh results arrive.
  ConnectionState stale_state_;

  CompletionOnceCallback callback_;

  base::WeakPtrFactory<AsyncDnsJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_ASYNC_DNS_JOB_H_

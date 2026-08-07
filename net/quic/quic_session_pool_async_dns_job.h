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
#include "net/base/completion_once_callback.h"
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
// signals. Connection establishment is delegated to an owned
// EndpointConnector, which reads the job's current results through
// GetUsableEndpoints() and GetAttemptParams() and reports back through
// OnSessionCreationDecided() and OnConnectorComplete().
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
    QuicSessionEstablishmentReason quic_session_establishment_reason =
        QuicSessionEstablishmentReason::kUnknown;
    std::optional<ConnectionManagementConfig> connection_management_config;
  };

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
      QuicSessionEstablishmentReason quic_session_establishment_reason,
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

  // Called by the connector when an attempt finished creating its session.
  // ERR_IO_PENDING means the session was created and its crypto handshake
  // is still in flight. Delivers the requests' one-shot session creation
  // signal.
  void OnSessionCreationDecided(int rv);

  // Called by the connector when connection establishment settled
  // asynchronously. Called at most once. Completes the job.
  void OnConnectorComplete(int rv);

 private:
  int DoResolveHost();
  int DoResolveHostComplete(int rv);

  // Returns whether the client should be SVCB-optional when connecting to
  // `endpoints`.
  bool IsSvcbOptional(base::span<const ServiceEndpoint> endpoints) const;

  const quic::ParsedQuicVersion quic_version_;
  const raw_ptr<HostResolver> host_resolver_;
  const bool use_dns_aliases_;
  const bool require_dns_https_alpn_;
  const int cert_verify_flags_;
  const bool retry_on_alternate_network_before_handshake_;
  const MultiplexedSessionCreationInitiator session_creation_initiator_;
  const std::optional<ConnectionManagementConfig> connection_management_config_;

  bool host_resolution_finished_ = false;
  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
  // Usable endpoints derived from the current resolver results. Reset on
  // every resolver event and recomputed on the next GetUsableEndpoints()
  // call.
  mutable std::optional<std::vector<UsableEndpoint>> usable_endpoints_;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;
  std::unique_ptr<EndpointConnector> connector_;
  CompletionOnceCallback callback_;

  base::WeakPtrFactory<AsyncDnsJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_ASYNC_DNS_JOB_H_

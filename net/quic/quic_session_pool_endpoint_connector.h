// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_
#define NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_async_dns_job.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

// Establishes one QUIC session out of the endpoints resolved by the owning
// AsyncDnsJob, adopting the first attempt that succeeds. The connector holds
// no DNS data. It reads the job's current results when making a start
// decision, and reports results back through methods on the job.
//
// The connector advances itself. It has at most one attempt in flight, and
// starts the next untried candidate when that attempt fails. A candidate is
// one IP endpoint of the job's usable results together with the metadata and
// the QUIC version an attempt to it would use. Each candidate is attempted at
// most once per job, which the job tracks.
class QuicSessionPool::EndpointConnector : public QuicSessionAttempt::Delegate {
 public:
  explicit EndpointConnector(AsyncDnsJob* job);

  EndpointConnector(const EndpointConnector&) = delete;
  EndpointConnector& operator=(const EndpointConnector&) = delete;

  ~EndpointConnector() override;

  // Starts an attempt when none is in flight and the job's current usable
  // endpoints contain an untried candidate. Called on DNS updates and after
  // an attempt failed. Returns OK or a net error when establishment settled
  // synchronously, and ERR_IO_PENDING while an attempt is in flight. Returns
  // std::nullopt when no attempt is in flight and there is nothing to
  // attempt. The caller decides whether that is fatal.
  std::optional<int> TryAdvance();

  bool has_attempt() const { return attempt_ != nullptr; }

  // The result of the most recently failed attempt. Null until an attempt
  // failed.
  std::optional<int> last_attempt_error() const { return last_attempt_error_; }

  // True while an attempt is creating its session, i.e. the requests' session
  // creation signal can still fire.
  bool AwaitingSessionCreation() const;

  // Adds information about the attempted connection to `details`.
  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // QuicSessionAttempt::Delegate implementation.
  QuicSessionPool* GetQuicSessionPool() override;
  const QuicSessionAliasKey& GetKey() override;
  const NetLogWithSource& GetNetLog() override;
  void OnConnectionFailedOnDefaultNetwork() override;
  void OnQuicSessionCreationComplete(int rv) override;

 private:
  // Claims and returns the next candidate to attempt. Prefers untried IPv6
  // candidates. IPv4 is allowed only when no untried IPv6 candidate is
  // visible. Returns std::nullopt when every visible candidate was claimed.
  // TODO(crbug.com/531975349): On a network that silently drops IPv6
  // packets, every IPv6 attempt must time out before IPv4 is tried. Support
  // racing an IPv4 attempt in parallel to bound that wait.
  std::optional<AsyncDnsJob::Candidate> ClaimNextCandidate();

  // Snapshots the failed attempt's result and error details, then destroys
  // the attempt. The snapshot outlives the attempt because the job reports it
  // when nothing else can be attempted.
  void RecordAttemptFailure(int rv);

  void OnAttemptComplete(int rv);

  const raw_ptr<AsyncDnsJob> job_;
  std::unique_ptr<QuicSessionAttempt> attempt_;
  // True from Start() returning ERR_IO_PENDING until OnAttemptComplete().
  // `attempt_` alone cannot tell; it is kept after a successful completion.
  bool attempt_in_flight_ = false;
  std::optional<int> last_attempt_error_;
  // Error details of the most recently failed attempt. Snapshotted because
  // the attempt is destroyed on failure.
  NetErrorDetails last_attempt_details_;
  base::WeakPtrFactory<EndpointConnector> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

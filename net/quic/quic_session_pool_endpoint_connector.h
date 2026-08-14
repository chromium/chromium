// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_
#define NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_error_details.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_pool.h"

namespace net {

// Establishes one QUIC session out of the endpoints resolved by the owning
// AsyncDnsJob, adopting the first attempt that succeeds. The connector holds
// no DNS data and picks no candidates. It asks the job for its next candidate
// when it decides to start an attempt, and reports results back through
// methods on the job.
//
// The connector advances itself. It has at most one attempt in flight, and
// asks for the next candidate when that attempt fails. The connector is
// neutral about address families. Which families it may use is decided by the
// job, from the slot this connector occupies and whether DNS resolution has
// finished.
class QuicSessionPool::EndpointConnector : public QuicSessionAttempt::Delegate {
 public:
  explicit EndpointConnector(AsyncDnsJob* job);

  EndpointConnector(const EndpointConnector&) = delete;
  EndpointConnector& operator=(const EndpointConnector&) = delete;

  ~EndpointConnector() override;

  // Starts an attempt when none is in flight and the job hands this connector
  // a candidate. Called on DNS updates, when the slow timer fires, and after
  // an attempt failed. Returns OK or a net error when establishment settled
  // synchronously, and ERR_IO_PENDING while an attempt is in flight. Returns
  // std::nullopt when no attempt is in flight and there is nothing to
  // attempt. The caller decides whether that is fatal.
  std::optional<int> TryAdvance();

  bool has_attempt() const { return attempt_ != nullptr; }

  // True when this connector could start an attempt as soon as the job has a
  // candidate for it. The job advances such connectors when new resolver
  // results arrive.
  bool is_waiting_on_dns() const { return !has_attempt(); }

  // True while the attempt in flight connects to an IPv6 address. The job
  // reads this when it decides which slot this connector takes.
  bool is_attempting_ipv6() const;

  // True while an attempt is creating its session, i.e. the requests' session
  // creation signal can still fire.
  bool AwaitingSessionCreation() const;

  // Adds information about the attempt in flight to `details`. Call only
  // while this connector has one.
  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  // QuicSessionAttempt::Delegate implementation.
  QuicSessionPool* GetQuicSessionPool() override;
  const QuicSessionAliasKey& GetKey() override;
  const NetLogWithSource& GetNetLog() override;
  void OnConnectionFailedOnDefaultNetwork() override;
  void OnQuicSessionCreationComplete(int rv) override;

 private:
  // Hands the failed attempt's result and error details to the job, then
  // destroys the attempt. The job keeps the error details because the attempt
  // does not survive its own failure.
  void RecordAttemptFailure(int rv);

  void OnAttemptComplete(int rv);

  const raw_ptr<AsyncDnsJob> job_;
  std::unique_ptr<QuicSessionAttempt> attempt_;
  // True from Start() returning ERR_IO_PENDING until OnAttemptComplete().
  // `attempt_` alone cannot tell; it is kept after a successful completion.
  bool attempt_in_flight_ = false;
  // The result of the most recent failed attempt of this connector. A
  // connector that already failed once re-checks IP pooling before it starts
  // another attempt.
  std::optional<int> last_attempt_error_;
  base::WeakPtrFactory<EndpointConnector> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_
#define NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
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
  // `created_by_slow_timer` is true for the connector created when the slow
  // timer fires. This stays unchanged if the connector moves to another slot.
  EndpointConnector(AsyncDnsJob* job,
                    const char* name,
                    bool created_by_slow_timer);

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

  bool has_attempt_in_flight() const { return attempt_in_flight_; }

  // A stable name that does not change when this connector moves between
  // slots. For logging.
  const char* name() const { return name_; }

  // Returns the identifier of the current attempt, or nothing when there is
  // none. For logging.
  std::optional<int> attempt_id() const { return attempt_id_; }

  // When the current attempt started. Meaningful only while has_attempt().
  base::TimeTicks attempt_start_time() const { return attempt_start_time_; }

  // How many attempts this connector has started.
  size_t attempts_started() const { return attempts_started_; }

  bool created_by_slow_timer() const { return created_by_slow_timer_; }

  bool is_stale() const;

  // True when this connector could start an attempt as soon as the job has a
  // candidate for it. The job advances such connectors when new resolver
  // results arrive.
  bool is_waiting_on_dns() const { return !has_attempt(); }

  // True while the attempt in flight connects to an IPv6 address. The job
  // reads this when it decides which slot this connector takes.
  bool is_attempting_ipv6() const;

  // Returns the address of the attempt in flight, or nothing when there is
  // none. For logging.
  std::optional<IPEndPoint> attempt_ip_endpoint() const;

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
  const char* const name_;
  const bool created_by_slow_timer_;
  std::unique_ptr<QuicSessionAttempt> attempt_;
  // The job-wide identifier of `attempt_`.
  std::optional<int> attempt_id_;
  // True from Start() returning ERR_IO_PENDING until OnAttemptComplete().
  // `attempt_` alone cannot tell; it is kept after a successful completion.
  bool attempt_in_flight_ = false;
  base::TimeTicks attempt_start_time_;
  size_t attempts_started_ = 0;
  // The result of the most recent failed attempt of this connector. A
  // connector that already failed once re-checks IP pooling before it starts
  // another attempt.
  std::optional<int> last_attempt_error_;
  base::WeakPtrFactory<EndpointConnector> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

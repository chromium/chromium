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
// no DNS data. It reads the job's current results when making a start
// decision, and reports results back through methods on the job.
class QuicSessionPool::EndpointConnector : public QuicSessionAttempt::Delegate {
 public:
  explicit EndpointConnector(AsyncDnsJob* job);

  EndpointConnector(const EndpointConnector&) = delete;
  EndpointConnector& operator=(const EndpointConnector&) = delete;

  ~EndpointConnector() override;

  // Signals that the job's DNS results changed. Starts an attempt when the
  // results contain a usable endpoint. Returns OK or a net error when
  // establishment settled synchronously, and ERR_IO_PENDING while an
  // attempt is in flight. Returns std::nullopt when no attempt is in
  // flight and the current results have nothing to attempt. The caller
  // decides whether that is fatal.
  std::optional<int> OnEndpointsUpdated();

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
  void OnAttemptComplete(int rv);

  const raw_ptr<AsyncDnsJob> job_;
  std::unique_ptr<QuicSessionAttempt> attempt_;
  // True from Start() returning ERR_IO_PENDING until OnAttemptComplete().
  // `attempt_` alone cannot tell; it is kept after completion.
  bool attempt_in_flight_ = false;
  base::WeakPtrFactory<EndpointConnector> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_ENDPOINT_CONNECTOR_H_

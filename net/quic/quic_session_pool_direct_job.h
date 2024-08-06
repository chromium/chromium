// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_DIRECT_JOB_H_
#define NET_QUIC_QUIC_SESSION_POOL_DIRECT_JOB_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_error_details.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_job.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

// A DirectJob is a QuicSessionPool::Job that handles direct connections to the
// destination.
//
// A job works on behalf of a pool and a collection of requests to create a new
// QUIC session.
class QuicSessionPool::DirectJob : public QuicSessionPool::Job {
 public:
  DirectJob(QuicSessionPool* pool,
            quic::ParsedQuicVersion quic_version,
            HostResolver* host_resolver,
            QuicSessionAliasKey key,
            std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
            bool retry_on_alternate_network_before_handshake,
            RequestPriority priority,
            bool use_dns_aliases,
            bool require_dns_https_alpn,
            int cert_verify_flags,
            const NetLogWithSource& net_log);

  ~DirectJob() override;

  // QuicSessionPool::Job implementation.
  int Run(CompletionOnceCallback callback) override;
  void SetRequestExpectations(QuicSessionRequest* request) override;
  void UpdatePriority(RequestPriority old_priority,
                      RequestPriority new_priority) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;

 private:
  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoAttemptSession();

  void OnResolveHostComplete(int rv);
  void OnSessionAttemptComplete(int rv);

  base::WeakPtr<DirectJob> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_ATTEMPT_SESSION,
  };

  // Returns whether the client should be SVCB-optional when connecting to
  // `results`.
  bool IsSvcbOptional(
      base::span<const HostResolverEndpointResult> results) const;

  IoState io_state_ = STATE_RESOLVE_HOST;
  // TODO(bashi): Rename to `alt_svc_quic_version_` or `known_quic_version_`?
  const quic::ParsedQuicVersion quic_version_;
  const raw_ptr<HostResolver> host_resolver_;
  const bool use_dns_aliases_;
  const bool require_dns_https_alpn_;
  const int cert_verify_flags_;
  const bool retry_on_alternate_network_before_handshake_;
  bool host_resolution_finished_ = false;
  HostResolverEndpointResult endpoint_result_;
  CompletionOnceCallback callback_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;
  std::unique_ptr<QuicSessionAttempt> session_attempt_;
  base::WeakPtrFactory<DirectJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_DIRECT_JOB_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_JOB_H_
#define NET_QUIC_QUIC_SESSION_POOL_JOB_H_

#include "net/quic/quic_session_pool.h"

namespace net {

// Responsible for creating a new QUIC session to the specified server, and
// for notifying any associated requests when complete.
//
// A single Job can be associated with any number of `QuicSessionRequest`
// instances, and will update all of them with its progress.
//
// |client_config_handle| is not actually used, but serves to keep the
// corresponding CryptoClientConfig alive until the Job completes.
class QuicSessionPool::Job {
 public:
  Job(QuicSessionPool* pool,
      quic::ParsedQuicVersion quic_version,
      HostResolver* host_resolver,
      const QuicSessionAliasKey& key,
      std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
      bool was_alternative_service_recently_broken,
      bool retry_on_alternate_network_before_handshake,
      RequestPriority priority,
      bool use_dns_aliases,
      bool require_dns_https_alpn,
      int cert_verify_flags,
      const NetLogWithSource& net_log);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

  // Run the job. This should be called as soon as the job is created, then any
  // associated requests added with `AddRequest()`.
  int Run(CompletionOnceCallback callback);

  // Add a new request associated with this Job.
  void AddRequest(QuicSessionRequest* request);

  // Remove a request associated with this job. The request must already be
  // associated with the job.
  void RemoveRequest(QuicSessionRequest* request);

  // Update the priority of this Job.
  void SetPriority(RequestPriority priority);

  // Add information about the new session to `details`. Must be called after
  // the run has completed.
  void PopulateNetErrorDetails(NetErrorDetails* details) const;

  const QuicSessionAliasKey& key() const { return key_; }
  const NetLogWithSource& net_log() const { return net_log_; }
  const std::set<QuicSessionRequest*>& stream_requests() {
    return stream_requests_;
  }
  RequestPriority priority() const { return priority_; }

 private:
  int DoLoop(int rv);
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  int DoCreateSession();
  int DoCreateSessionComplete(int rv);
  int DoConnect(int rv);
  int DoConfirmConnection(int rv);

  void OnResolveHostComplete(int rv);
  void OnCreateSessionComplete(int rv);
  void OnCryptoConnectComplete(int rv);

  base::WeakPtr<Job> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  enum IoState {
    STATE_NONE,
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_CREATE_SESSION,
    STATE_CREATE_SESSION_COMPLETE,
    STATE_CONNECT,
    STATE_CONFIRM_CONNECTION,
  };

  void CloseStaleHostConnection();

  // Returns whether the client should be SVCB-optional when connecting to
  // `results`.
  bool IsSvcbOptional(
      base::span<const HostResolverEndpointResult> results) const;

  // Returns the QUIC version that would be used with `endpoint_result`, or
  // `quic::ParsedQuicVersion::Unsupported()` if `endpoint_result` cannot be
  // used with QUIC.
  quic::ParsedQuicVersion SelectQuicVersion(
      const HostResolverEndpointResult& endpoint_result,
      bool svcb_optional) const;

  void LogStaleAndFreshHostMatched(bool matched);

  IoState io_state_ = STATE_RESOLVE_HOST;
  raw_ptr<QuicSessionPool> pool_;
  quic::ParsedQuicVersion quic_version_;
  quic::ParsedQuicVersion quic_version_used_ =
      quic::ParsedQuicVersion::Unsupported();
  raw_ptr<HostResolver> host_resolver_;
  const QuicSessionAliasKey key_;
  const std::unique_ptr<CryptoClientConfigHandle> client_config_handle_;
  RequestPriority priority_;
  const bool use_dns_aliases_;
  const bool require_dns_https_alpn_;
  const int cert_verify_flags_;
  const bool was_alternative_service_recently_broken_;
  const bool retry_on_alternate_network_before_handshake_;
  const NetLogWithSource net_log_;
  bool host_resolution_finished_ = false;
  bool session_creation_finished_ = false;
  bool connection_retried_ = false;
  raw_ptr<QuicChromiumClientSession> session_ = nullptr;
  HostResolverEndpointResult endpoint_result_;
  // If connection migraiton is supported, |network_| denotes the network on
  // which |session_| is created.
  handles::NetworkHandle network_;
  CompletionOnceCallback host_resolution_callback_;
  CompletionOnceCallback callback_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;
  base::TimeTicks dns_resolution_start_time_;
  base::TimeTicks dns_resolution_end_time_;
  base::TimeTicks quic_connection_start_time_;
  std::set<QuicSessionRequest*> stream_requests_;
  base::WeakPtrFactory<Job> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_JOB_H_

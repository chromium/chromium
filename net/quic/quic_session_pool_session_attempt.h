// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_handle.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_job.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

#ifndef NET_QUIC_QUIC_SESSION_POOL_SESSION_ATTEMPT_H_
#define NET_QUIC_QUIC_SESSION_POOL_SESSION_ATTEMPT_H_

namespace net {

// Handles a single attempt to create a new QUIC session for an endpoint.
// On success, the new session is activated unless another session has been
// activated for the same endpoint. When failed on the default network, it may
// retry on an alternate network if the system supports non-default networks.
class QuicSessionPool::SessionAttempt {
 public:
  // Create a SessionAttempt for a direct connection.
  SessionAttempt(Job* job,
                 IPEndPoint ip_endpoint,
                 ConnectionEndpointMetadata metadata,
                 quic::ParsedQuicVersion quic_version,
                 int cert_verify_flags,
                 base::TimeTicks dns_resolution_start_time,
                 base::TimeTicks dns_resolution_end_time,
                 bool retry_on_alternate_network_before_handshake,
                 bool use_dns_aliases,
                 std::set<std::string> dns_aliases);
  // Create a SessionAttempt for a connection proxied over the given stream.
  SessionAttempt(Job* job,
                 IPEndPoint local_endpoint,
                 IPEndPoint proxy_peer_endpoint,
                 quic::ParsedQuicVersion quic_version,
                 int cert_verify_flags,
                 std::unique_ptr<QuicChromiumClientStream::Handle> proxy_stream,
                 const HttpUserAgentSettings* http_user_agent_settings);

  ~SessionAttempt();

  SessionAttempt(const SessionAttempt&) = delete;
  SessionAttempt& operator=(const SessionAttempt&) = delete;

  int Start(CompletionOnceCallback callback);

  bool session_creation_finished() const { return session_creation_finished_; }

  QuicChromiumClientSession* session() const { return session_.get(); }

 private:
  enum class State {
    kNone,
    kCreateSession,
    kCreateSessionComplete,
    kCryptoConnect,
    kConfirmConnection,
  };

  QuicSessionPool* pool() { return job_->pool(); }
  const QuicSessionAliasKey& key() { return job_->key(); }
  const NetLogWithSource& net_log() { return job_->net_log(); }

  int DoLoop(int rv);

  int DoCreateSession();
  int DoCreateSessionComplete(int rv);
  int DoCryptoConnect(int rv);
  int DoConfirmConnection(int rv);

  void OnCreateSessionComplete(int rv);
  void OnCryptoConnectComplete(int rv);

  const raw_ptr<Job> job_;

  const IPEndPoint ip_endpoint_;
  const ConnectionEndpointMetadata metadata_;
  const quic::ParsedQuicVersion quic_version_;
  const int cert_verify_flags_;
  const base::TimeTicks dns_resolution_start_time_;
  const base::TimeTicks dns_resolution_end_time_;
  const bool was_alternative_service_recently_broken_;
  const bool retry_on_alternate_network_before_handshake_;
  const bool use_dns_aliases_;
  std::set<std::string> dns_aliases_;

  // Fields only used for session attempts to a proxy.
  std::unique_ptr<QuicChromiumClientStream::Handle> proxy_stream_;
  const raw_ptr<const HttpUserAgentSettings> http_user_agent_settings_;
  const IPEndPoint local_endpoint_;

  State next_state_ = State::kNone;
  bool in_loop_ = false;

  raw_ptr<QuicChromiumClientSession> session_ = nullptr;
  bool session_creation_finished_ = false;
  bool connection_retried_ = false;

  base::TimeTicks quic_connection_start_time_;

  // If connection migraiton is supported, |network_| denotes the network on
  // which |session_| is created.
  handles::NetworkHandle network_ = handles::kInvalidNetworkHandle;

  CompletionOnceCallback callback_;

  base::WeakPtrFactory<SessionAttempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_SESSION_ATTEMPT_H_

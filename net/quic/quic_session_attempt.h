// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

#ifndef NET_QUIC_QUIC_SESSION_ATTEMPT_H_
#define NET_QUIC_QUIC_SESSION_ATTEMPT_H_

namespace net {

class QuicSessionPool;

// Handles a single attempt to create a new QUIC session for an endpoint.
// On success, the new session is activated unless another session has been
// activated for the same endpoint. When failed on the default network, it may
// retry on an alternate network if the system supports non-default networks.
class NET_EXPORT_PRIVATE QuicSessionAttempt {
 public:
  // Represents a successful QUIC session creation. Used for QUIC session
  // creations that could complete asynchronously.
  struct CreateSessionResult {
    raw_ptr<QuicChromiumClientSession> session;
    handles::NetworkHandle network = handles::kInvalidNetworkHandle;
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the QuicSessionPool that the attempt will use.
    virtual QuicSessionPool* GetQuicSessionPool() = 0;

    // Returns the QuicSessionAliasKey that the attempt will use to identify
    // the session.
    virtual const QuicSessionAliasKey& GetKey() = 0;

    // Returns the NetLogWithSource that the attempt should use.
    virtual const NetLogWithSource& GetNetLog() = 0;

    // Called when the attempt is failed on the default network.
    virtual void OnConnectionFailedOnDefaultNetwork() {}

    // Called when the attempt completed creating the session.
    virtual void OnQuicSessionCreationComplete(int rv) {}
  };

  // Create a SessionAttempt for a direct connection.
  // The `crypto_client_config_handle` is retained to keep the corresponding
  // CryptoClientConfig alive until `this` completes. Call sites can pass
  // nullptr to `crypto_client_config_handle` if the corresponding
  // CryptoClientConfig is guaranteed to be alive.
  QuicSessionAttempt(Delegate* delegate,
                     IPEndPoint ip_endpoint,
                     ConnectionEndpointMetadata metadata,
                     quic::ParsedQuicVersion quic_version,
                     int cert_verify_flags,
                     base::TimeTicks dns_resolution_start_time,
                     base::TimeTicks dns_resolution_end_time,
                     bool retry_on_alternate_network_before_handshake,
                     bool use_dns_aliases,
                     std::set<std::string> dns_aliases,
                     std::unique_ptr<QuicCryptoClientConfigHandle>
                         crypto_client_config_handle);
  // Create a SessionAttempt for a connection proxied over the given stream.
  QuicSessionAttempt(
      Delegate* delegate,
      IPEndPoint local_endpoint,
      IPEndPoint proxy_peer_endpoint,
      quic::ParsedQuicVersion quic_version,
      int cert_verify_flags,
      std::unique_ptr<QuicChromiumClientStream::Handle> proxy_stream,
      const HttpUserAgentSettings* http_user_agent_settings);

  ~QuicSessionAttempt();

  QuicSessionAttempt(const QuicSessionAttempt&) = delete;
  QuicSessionAttempt& operator=(const QuicSessionAttempt&) = delete;

  int Start(CompletionOnceCallback callback);

  bool session_creation_finished() const { return session_creation_finished_; }

  QuicChromiumClientSession* session() const { return session_.get(); }

  void PopulateNetErrorDetails(NetErrorDetails* details) const;

 private:
  enum class State {
    kNone,
    kCreateSession,
    kCreateSessionComplete,
    kCryptoConnect,
    kConfirmConnection,
  };

  QuicSessionPool* pool() { return delegate_->GetQuicSessionPool(); }
  const QuicSessionAliasKey& key() { return delegate_->GetKey(); }
  const NetLogWithSource& net_log() { return delegate_->GetNetLog(); }

  int DoLoop(int rv);

  int DoCreateSession();
  int DoCreateSessionComplete(int rv);
  int DoCryptoConnect(int rv);
  int DoConfirmConnection(int rv);

  void OnCreateSessionComplete(base::expected<CreateSessionResult, int> result);
  void OnCryptoConnectComplete(int rv);

  void ResetSession();

  const raw_ptr<Delegate> delegate_;

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
  std::unique_ptr<QuicCryptoClientConfigHandle> crypto_client_config_handle_;

  // Fields only used for session attempts to a proxy.
  std::unique_ptr<QuicChromiumClientStream::Handle> proxy_stream_;
  const raw_ptr<const HttpUserAgentSettings> http_user_agent_settings_;
  const IPEndPoint local_endpoint_;

  State next_state_ = State::kNone;
  bool in_loop_ = false;

  raw_ptr<QuicChromiumClientSession> session_ = nullptr;
  bool session_creation_finished_ = false;
  bool connection_retried_ = false;

  // Used to populate NetErrorDetails after we reset `session_`.
  HttpConnectionInfo connection_info_;
  quic::QuicErrorCode quic_connection_error_ = quic::QUIC_NO_ERROR;

  base::TimeTicks quic_connection_start_time_;

  // If connection migraiton is supported, |network_| denotes the network on
  // which |session_| is created.
  handles::NetworkHandle network_ = handles::kInvalidNetworkHandle;

  CompletionOnceCallback callback_;

  base::WeakPtrFactory<QuicSessionAttempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_ATTEMPT_H_

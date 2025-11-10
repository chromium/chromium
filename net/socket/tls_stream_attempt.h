// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TLS_STREAM_ATTEMPT_H_
#define NET_SOCKET_TLS_STREAM_ATTEMPT_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/socket/stream_attempt.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"

namespace net {

struct ServiceEndpoint;
class SSLClientSocket;
class TcpStreamAttempt;

// Represents a single TLS connection attempt.
class NET_EXPORT_PRIVATE TlsStreamAttempt final : public StreamAttempt {
 public:
  // Timeout for the TLS handshake. The timeout is the same as SSLConnectJob.
  static constexpr base::TimeDelta kTlsHandshakeTimeout = base::Seconds(30);

  // Represents an error of getting a SSLConfig for an attempt.
  enum class GetServiceEndpointError {
    // The attempt should abort. Currently this happens when we start an attempt
    // without waiting for HTTPS RR and the DNS resolution resulted in making
    // the attempt SVCB-reliant.
    kAbort,
  };

  // An interface to interact with TlsStreamAttempt.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called when TCP handshake completes.
    virtual void OnTcpHandshakeComplete() = 0;

    // Returns `OK` and ignores `callback` when the attempt can start TLS
    // handshake immediately. Otherwise, returns `ERR_IO_PENDING` when `this`
    // can't provide a ServiceEndpoint for TLS handshake immediately. `callback`
    // is invoked when a ServiceEndpoint is ready.
    virtual int WaitForTlsHandshakeReady(CompletionOnceCallback callback) = 0;

    // Returns a ServiceEndpoint for TLS handshake. Should be called only after
    // WaitForTlsHandshakeReady() returns `OK` or the callback is invoked.
    virtual base::expected<ServiceEndpoint, GetServiceEndpointError>
    GetServiceEndpointForTlsHandshake() = 0;
  };

  // `params` must outlive `this`. `base_ssl_config` contains the base SSL
  // configuration. Some additional configuration (things that depend on
  // ServiceEndpoint) is applied within TlsStreamAttempt.
  TlsStreamAttempt(const StreamAttemptParams* params,
                   IPEndPoint ip_endpoint,
                   perfetto::Track track,
                   HostPortPair host_port_pair,
                   SSLConfig base_ssl_config,
                   Delegate* delegate);

  TlsStreamAttempt(const TlsStreamAttempt&) = delete;
  TlsStreamAttempt& operator=(const TlsStreamAttempt&) = delete;

  ~TlsStreamAttempt() override;

  // StreamAttempt implementations:
  LoadState GetLoadState() const override;
  base::Value::Dict GetInfoAsValue() const override;
  scoped_refptr<SSLCertRequestInfo> GetCertRequestInfo() override;

  bool IsTcpHandshakeCompleted() { return tcp_handshake_completed_; }

  bool IsTlsHandshakeStarted() { return tls_handshake_started_; }

 private:
  enum class State {
    kNone,
    kTcpAttempt,
    kTcpAttemptComplete,
    kTlsAttempt,
    kTlsAttemptComplete,
  };

  static std::string_view StateToString(State state);

  // StreamAttempt methods:
  int StartInternal() override;
  base::Value::Dict GetNetLogStartParams() override;

  void OnIOComplete(int rv);

  int DoLoop(int rv);
  int DoTcpAttempt();
  int DoTcpAttemptComplete(int rv);
  int DoTlsAttempt(int rv);
  int DoTlsAttemptComplete(int rv);

  void OnTlsHandshakeTimeout();

  void MaybeRecordTlsHandshakeEnd(int rv);

  void ResetStateForRestart();

  State next_state_ = State::kNone;
  const HostPortPair host_port_pair_;
  const SSLConfig base_ssl_config_;
  const raw_ptr<Delegate> delegate_;

  std::unique_ptr<TcpStreamAttempt> nested_attempt_;

  bool tcp_handshake_completed_ = false;
  bool tls_handshake_started_ = false;
  base::OneShotTimer tls_handshake_timeout_timer_;
  std::unique_ptr<SSLClientSocket> ssl_socket_;
  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;

  std::optional<SSLConfig> ssl_config_;
  std::optional<std::vector<uint8_t>> ech_retry_configs_;
  // Set to true when the TlsStreamAttempt retries itself after receiving a
  // certificate error when sending TLS Trust Anchor IDs. Used to ensure that we
  // only retry once per connection attempt.
  bool retried_for_trust_anchor_ids_ = false;
  // Used for metrics. Set to true when the initial connection attempt used a
  // service endpoint that advertised trust anchor IDs and ECH, respectively,
  // whether or not sufficient features were enabled to use them.
  bool trust_anchor_ids_from_dns_ = false;
  bool is_ech_capable_ = false;

  base::WeakPtrFactory<TlsStreamAttempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_TLS_STREAM_ATTEMPT_H_

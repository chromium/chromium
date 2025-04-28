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

class TcpStreamAttempt;
class SSLClientSocket;

// Represents a single TLS connection attempt.
class NET_EXPORT_PRIVATE TlsStreamAttempt final : public StreamAttempt {
 public:
  // Timeout for the TLS handshake. The timeout is the same as SSLConnectJob.
  static constexpr base::TimeDelta kTlsHandshakeTimeout = base::Seconds(30);

  // Represents an error of getting a SSLConfig for an attempt.
  enum class GetSSLConfigError {
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

    // Returns OK when a SSLConfig is immediately available. `callback` is never
    // invoked. Otherwise, returns ERR_IO_PENDING when `this` can't provide a
    // SSLConfig immediately. `callback` is invoked when a SSLConfig is ready.
    virtual int WaitForSSLConfigReady(CompletionOnceCallback callback) = 0;

    // Returns a SSLConfig. Should be called only after WaitForSSLConfigReady()
    // returns OK or the callback is invoked.
    virtual base::expected<SSLConfig, GetSSLConfigError> GetSSLConfig() = 0;
  };

  // `params` and `ssl_config_provider` must outlive `this`.
  TlsStreamAttempt(const StreamAttemptParams* params,
                   IPEndPoint ip_endpoint,
                   perfetto::Track track,
                   HostPortPair host_port_pair,
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

  State next_state_ = State::kNone;
  const HostPortPair host_port_pair_;
  const raw_ptr<Delegate> delegate_;

  std::unique_ptr<TcpStreamAttempt> nested_attempt_;

  bool tcp_handshake_completed_ = false;
  bool tls_handshake_started_ = false;
  base::OneShotTimer tls_handshake_timeout_timer_;
  std::unique_ptr<SSLClientSocket> ssl_socket_;
  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;

  std::optional<SSLConfig> ssl_config_;
  std::optional<std::vector<uint8_t>> ech_retry_configs_;

  base::WeakPtrFactory<TlsStreamAttempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_TLS_STREAM_ATTEMPT_H_

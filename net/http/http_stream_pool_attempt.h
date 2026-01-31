// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_H_
#define NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_H_

#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_stream_pool.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config.h"

namespace net {

class StreamSocket;
class HttpStreamKey;

// Represents a single in-flight attempt which can make at most two concurrent
// TCP-based inner attempts (one IPv4, one IPv6). Upon success, it creates an
// HttpStream or a SpdySession and notifies the Delegate. If all inner attempts
// fail, it notifies the Delegate of the failure.
//
// In the future, this may also handle QUIC inner attempts.
class HttpStreamPool::Attempt {
 public:
  // An interface to abstract dependencies so that we can have unittests without
  // depending on AttemptManager. In production code AttemptManager will
  // implement this interface. Unless otherwise specified, all methods of this
  // interface must not delete the corresponding Attempt.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the HttpStreamKey this attempt uses to make inner attempts.
    virtual const HttpStreamKey& GetHttpStreamKey() const = 0;

    // Returns the ServiceEndpointRequest associated with this attempt.
    virtual HostResolver::ServiceEndpointRequest& GetServiceEndpointRequest()
        const = 0;

    // Returns true when the ServiceEndpointRequest is finished.
    virtual bool IsServiceEndpointRequestFinished() const = 0;

    // Returns true when endpoints are SVCB optional. See
    // https://www.rfc-editor.org/rfc/rfc9460#section-3
    virtual bool IsSvcbOptional() const = 0;

    // Returns the base SSLConfig associated with this attempt.
    virtual SSLConfig GetBaseSSLConfig() const = 0;

    // Returns the ALPNs to be used with ALPN.
    virtual const NextProtoVector& GetAlpnProtos() const = 0;

    // Callback methods. Only one of these methods will be called. Once one of
    // these methods is called, the Attempt must immediately be deleted, or it
    // will enter an undefined state.
    virtual void OnStreamSocketReady(
        Attempt* attempt,
        std::unique_ptr<StreamSocket> stream,
        LoadTimingInfo::ConnectTiming connect_timing) = 0;
    virtual void OnAttemptFailure(Attempt* attempt, int rv) = 0;
    virtual void OnCertificateError(Attempt* attempt,
                                    int rv,
                                    SSLInfo ssl_info) = 0;
    virtual void OnNeedsClientCertificate(
        Attempt* attempt,
        scoped_refptr<SSLCertRequestInfo> cert_info) = 0;
  };

  Attempt(Delegate& delegate,
          const StreamAttemptParams& stream_attempt_params,
          NetLogWithSource net_log);

  ~Attempt();

  // Starts the attempt.
  void Start();

  // Processes the service endpoint changes. Note that calling this method
  // may destroy `this` synchronously.
  void ProcessServiceEndpointChanges();

  const base::flat_set<IPEndPoint>& attempted_endpoints_for_testing() const {
    return attempted_endpoints_;
  }

 private:
  class TcpAttempt;

  // Represents a result of a failed attempt.
  struct AttemptResult {
    int error;
    IPEndPoint ip_endpoint;
  };

  // Tries to start an attempt. If all endpoints are failed, notifies the
  // delegate with the most recent attempt failure, or ERR_NAME_NOT_RESOLVED if
  // no endpoint is attempted. Otherwise, waits for further endpoints update,
  // and/or completing in-flight attempts.
  void MaybeAttempt();

  // Starts an internal attempt for `ip_endpoint`.
  void StartAttempt(IPEndPoint ip_endpoint);

  // Returns true when the `endpoint` is usable in the sense of
  // SVCB resource record. See
  // https://www.rfc-editor.org/rfc/rfc9460.html#section-9.3.
  bool IsEndpointUsable(const ServiceEndpoint& endpoint,
                        bool svcb_optional) const;

  // Calculates the IPEndPoint to attempt. Returns std::nullopt when no endpoint
  // is available.
  std::optional<IPEndPoint> GetIPEndPointToAttempt() const;

  // Returns the ServiceEndpoint to use for the TLS handshake. Returns an error
  // when the endpoint is not usable. See IsEndpointUsable() for details.
  base::expected<ServiceEndpoint, TlsStreamAttempt::GetServiceEndpointError>
  GetServiceEndpointForTlsHandshake(const IPEndPoint& ip_endpoint) const;

  void OnTcpAttemptSlow(TcpAttempt* attempt);

  void OnTcpAttemptComplete(TcpAttempt* attempt, int rv);

  void HandleSingleFailure(std::unique_ptr<TcpAttempt> attempt, int rv);

  const raw_ref<Delegate> delegate_;
  const raw_ref<const StreamAttemptParams> stream_attempt_params_;
  const bool using_tls_;

  const perfetto::Track track_;

  const NetLogWithSource net_log_;

  std::unique_ptr<TcpAttempt> ipv4_attempt_;
  std::unique_ptr<TcpAttempt> ipv6_attempt_;

  // Set to true when the associated ServiceEndpointRequest indicates that
  // endpoints are ready for crypto (TLS) handshake.
  bool is_crypto_ready_ = false;

  // Set to true when any of the inner attempts is observed to be slow.
  bool observed_slow_attempt_ = false;

  std::optional<AttemptResult> most_recent_attempt_failure_;

  // Contains IPEndPoints that have been attempted so far, including those
  // that are currently being attempted.
  base::flat_set<IPEndPoint> attempted_endpoints_;

  base::WeakPtrFactory<Attempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_H_

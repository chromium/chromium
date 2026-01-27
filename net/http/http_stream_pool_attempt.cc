// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_info.h"

namespace net {

// A single inner TCP-based attempt.
// TODO(crbug.com/457478038): Figure out better name for this class. Too many
// "Attempt" classes.
class HttpStreamPool::Attempt::TcpAttempt : public TlsStreamAttempt::Delegate {
 public:
  TcpAttempt(Attempt& owner, IPEndPoint ip_endpoint)
      : owner_(owner), ip_endpoint_(std::move(ip_endpoint)) {
    if (owner_->using_tls_) {
      attempt_ = std::make_unique<TlsStreamAttempt>(
          &owner_->stream_attempt_params_.get(), ip_endpoint_, owner_->track_,
          HostPortPair::FromSchemeHostPort(
              delegate().GetHttpStreamKey().destination()),
          delegate().GetBaseSSLConfig(),
          /*delegate=*/this);
    } else {
      attempt_ = std::make_unique<TcpStreamAttempt>(
          &owner_->stream_attempt_params_.get(), ip_endpoint_, owner_->track_);
    }
  }

  TcpAttempt(const TcpAttempt&) = delete;
  TcpAttempt& operator=(const TcpAttempt&) = delete;

  ~TcpAttempt() override = default;

  const IPEndPoint& ip_endpoint() const { return ip_endpoint_; }

  StreamAttempt& stream_attempt() const { return *attempt_.get(); }

  int Start(CompletionOnceCallback callback) {
    start_time_ = base::TimeTicks::Now();
    int rv = attempt_->Start(std::move(callback));
    if (rv == ERR_IO_PENDING && !owner_->observed_slow_attempt_) {
      slow_timer_.Start(FROM_HERE, HttpStreamPool::GetConnectionAttemptDelay(),
                        base::BindOnce(&TcpAttempt::OnSlowTimerFired,
                                       base::Unretained(this)));
    }
    return rv;
  }

  // Called when the delegate's service endpoint request is updated or finished
  // and endpoints are ready to start the TLS handshake. Note that this method
  // can be called multiple times, even after the TLS handshake has already
  // started.
  void MaybeStartTlsHandshake() {
    DCHECK(delegate().GetServiceEndpointRequest().EndpointsCryptoReady());
    if (!tls_handshake_ready_callback_) {
      return;
    }

    // Resume the slow timer if it was stopped.
    CHECK(!slow_timer_.IsRunning());
    if (!owner_->observed_slow_attempt_) {
      CHECK(!is_slow_);
      CHECK_GE(tcp_handshake_complete_time_, start_time_);
      // Timer is not guaranteed to fire exactly on time, so we need to check
      // if we are already past the delay.
      base::TimeDelta tcp_handshake_duration =
          tcp_handshake_complete_time_ - start_time_;
      if (HttpStreamPool::GetConnectionAttemptDelay() >
          tcp_handshake_duration) {
        slow_timer_.Start(FROM_HERE,
                          HttpStreamPool::GetConnectionAttemptDelay() -
                              tcp_handshake_duration,
                          base::BindOnce(&TcpAttempt::OnSlowTimerFired,
                                         base::Unretained(this)));
      } else {
        OnSlowTimerFired();
      }
    }

    std::move(tls_handshake_ready_callback_).Run(OK);
  }

  // TlsStreamAttempt::Delegate implementation.

  void OnTcpHandshakeComplete() override {
    tcp_handshake_complete_time_ = base::TimeTicks::Now();
  }

  // Called from TlsStreamAttempt when it is ready to start the TLS handshake
  // (i.e., TCP handshake is complete). If the endpoint is ready to start TLS
  // handshake, returns OK. Otherwise, returns ERR_IO_PENDING and stores the
  // callback to be called when the endpoint is ready to start TLS handshake.
  int WaitForTlsHandshakeReady(CompletionOnceCallback callback) override {
    if (owner_->is_crypto_ready_) {
      return OK;
    }

    // Pause the slow timer while waiting for the endpoints to be ready for TLS
    // handshake.
    if (slow_timer_.IsRunning()) {
      slow_timer_.Stop();
    }

    tls_handshake_ready_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  // Called from TlsStreamAttempt to get the service endpoint for TLS
  // handshake. Always called after the endpoint is ready to start TLS
  // handshake.
  base::expected<ServiceEndpoint, TlsStreamAttempt::GetServiceEndpointError>
  GetServiceEndpointForTlsHandshake() override {
    CHECK(delegate().GetServiceEndpointRequest().EndpointsCryptoReady());
    return owner_->GetServiceEndpointForTlsHandshake(ip_endpoint_);
  }

 private:
  Attempt::Delegate& delegate() const { return owner_->delegate_.get(); }

  void OnSlowTimerFired() {
    is_slow_ = true;
    owner_->OnTcpAttemptSlow(this);
  }

  const raw_ref<Attempt> owner_;
  const IPEndPoint ip_endpoint_;

  base::OneShotTimer slow_timer_;
  bool is_slow_ = false;

  base::TimeTicks start_time_;
  base::TimeTicks tcp_handshake_complete_time_;

  std::unique_ptr<StreamAttempt> attempt_;
  CompletionOnceCallback tls_handshake_ready_callback_;
};

HttpStreamPool::Attempt::Attempt(
    Delegate& delegate,
    const StreamAttemptParams& stream_attempt_params,
    NetLogWithSource net_log)
    : delegate_(delegate),
      stream_attempt_params_(stream_attempt_params),
      using_tls_(GURL::SchemeIsCryptographic(
          delegate_->GetHttpStreamKey().destination().scheme())),
      track_(base::trace_event::GetNextGlobalTraceId()),
      net_log_(std::move(net_log)) {
  if (delegate_->GetServiceEndpointRequest().EndpointsCryptoReady()) {
    is_crypto_ready_ = true;
  }
}

HttpStreamPool::Attempt::~Attempt() = default;

void HttpStreamPool::Attempt::Start() {
  MaybeAttempt();
}

void HttpStreamPool::Attempt::ProcessServiceEndpointChanges() {
  // First, trigger TLS handshakes for existing attempts if endpoints are ready
  // to start TLS handshakes.
  HostResolver::ServiceEndpointRequest& service_endpoint_request =
      delegate_->GetServiceEndpointRequest();
  if (!is_crypto_ready_ && service_endpoint_request.EndpointsCryptoReady()) {
    is_crypto_ready_ = true;
    // Starting TLS handshake may fail synchronously (also may succeed
    // synchronously, unlikely to happen in reality though). Synchronous
    // completion of an attempt could end up deleting `this`. Use WeakPtr to
    // detect whether `this` is deleted. Alternatively, we can use PostTask to
    // schedule `this` to be deleted when an attempt succeeds or fails. However,
    // scheduling tasks does have some overhead so we use WeakPtr here.
    base::WeakPtr<Attempt> weak_self = weak_ptr_factory_.GetWeakPtr();
    for (auto& attempt : {ipv6_attempt_.get(), ipv4_attempt_.get()}) {
      if (!attempt) {
        continue;
      }
      attempt->MaybeStartTlsHandshake();
      if (!weak_self) {
        // `this` is deleted. Have to return immediately.
        return;
      }
    }
  }

  // Second, try to start new attempts if possible.
  MaybeAttempt();
}

void HttpStreamPool::Attempt::MaybeAttempt() {
  std::optional<IPEndPoint> ip_endpoint = GetIPEndPointToAttempt();
  if (!ip_endpoint.has_value()) {
    // If all endpoints are failed, notify the delegate. Otherwise, wait for
    // further endpoints update, and/or completing in-flight attempts.
    const bool all_endpoints_failed =
        !ipv4_attempt_ && !ipv6_attempt_ &&
        delegate_->IsServiceEndpointRequestFinished();
    if (all_endpoints_failed) {
      base::WeakPtr<Attempt> weak_this = weak_ptr_factory_.GetWeakPtr();
      int error = most_recent_attempt_failure_.has_value()
                      ? most_recent_attempt_failure_->error
                      : ERR_NAME_NOT_RESOLVED;
      delegate_->OnAttemptFailure(this, error);
      // Do not add code, `this` is deleted.
      CHECK(!weak_this);
    }
    return;
  }

  StartAttempt(std::move(*ip_endpoint));
  // `this` may be deleted at this point. For example, if the attempt fails
  // synchronously and it's the last attempt.
}

void HttpStreamPool::Attempt::StartAttempt(IPEndPoint ip_endpoint) {
  const bool is_ipv4 = ip_endpoint.address().IsIPv4();
  DCHECK(is_ipv4 || ip_endpoint.address().IsIPv6());
  auto& attempt = is_ipv4 ? ipv4_attempt_ : ipv6_attempt_;
  CHECK(!attempt);

  auto [_, inserted] = attempted_endpoints_.insert(ip_endpoint);
  CHECK(inserted);

  attempt = std::make_unique<TcpAttempt>(*this, std::move(ip_endpoint));
  TcpAttempt* raw_attempt = attempt.get();
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_TCP_BASED_ATTEMPT_START, [&] {
        base::DictValue dict;
        dict.Set("ip_endpoint", raw_attempt->ip_endpoint().ToString());
        raw_attempt->stream_attempt().net_log().source().AddToEventParameters(
            dict);
        return dict;
      });
  raw_attempt->stream_attempt().net_log().AddEventReferencingSource(
      NetLogEventType::TCP_BASED_ATTEMPT_BOUND_TO_POOL, net_log_.source());

  int rv = attempt->Start(base::BindOnce(&Attempt::OnTcpAttemptComplete,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         raw_attempt));
  if (rv != ERR_IO_PENDING) {
    OnTcpAttemptComplete(raw_attempt, rv);
  }
}

bool HttpStreamPool::Attempt::IsEndpointUsable(const ServiceEndpoint& endpoint,
                                               bool svcb_optional) const {
  // No ALPNs means that the endpoint is an authority A/AAAA endpoint, even if
  // we are still in the middle of DNS resolution.
  if (endpoint.metadata.supported_protocol_alpns.empty()) {
    return svcb_optional;
  }

  // See https://www.rfc-editor.org/rfc/rfc9460.html#section-9.3. Endpoints are
  // usable if there is an overlap between the endpoint's ALPNs and the
  // configured ones.
  return std::ranges::any_of(
      endpoint.metadata.supported_protocol_alpns, [&](const auto& alpn) {
        NextProto next_proto = NextProtoFromString(alpn);
        if (!kTcpBasedProtocols.Has(next_proto)) {
          return false;
        }
        return std::ranges::contains(delegate_->GetAlpnProtos(), next_proto);
      });
}

std::optional<IPEndPoint> HttpStreamPool::Attempt::GetIPEndPointToAttempt()
    const {
  // Don't attempt more when the first attempt already exists and it's not slow.
  const bool has_attempt = ipv4_attempt_ || ipv6_attempt_;
  if (has_attempt && !observed_slow_attempt_) {
    return std::nullopt;
  }

  HostResolver::ServiceEndpointRequest& service_endpoint_request =
      delegate_->GetServiceEndpointRequest();
  if (service_endpoint_request.GetEndpointResults().empty()) {
    return std::nullopt;
  }

  // If there is a previous failed attempt, prefer the other address family.
  // Otherwise prefer IPv6.
  const bool prefer_ipv6 =
      !most_recent_attempt_failure_ ||
      !most_recent_attempt_failure_->ip_endpoint.address().IsIPv6();

  const bool svcb_optional = delegate_->IsSvcbOptional();
  const base::span<const ServiceEndpoint>& endpoint_results =
      service_endpoint_request.GetEndpointResults();

  for (bool use_ipv6 : {prefer_ipv6, !prefer_ipv6}) {
    if ((use_ipv6 && ipv6_attempt_) || (!use_ipv6 && ipv4_attempt_)) {
      continue;
    }

    for (const auto& service_endpoint : endpoint_results) {
      if (!IsEndpointUsable(service_endpoint, svcb_optional)) {
        continue;
      }

      const std::vector<IPEndPoint>& ip_endpoints =
          use_ipv6 ? service_endpoint.ipv6_endpoints
                   : service_endpoint.ipv4_endpoints;

      for (const auto& ip_endpoint : ip_endpoints) {
        if (attempted_endpoints_.contains(ip_endpoint)) {
          continue;
        }
        return ip_endpoint;
      }
    }
  }

  return std::nullopt;
}

base::expected<ServiceEndpoint, TlsStreamAttempt::GetServiceEndpointError>
HttpStreamPool::Attempt::GetServiceEndpointForTlsHandshake(
    const IPEndPoint& ip_endpoint) const {
  HostResolver::ServiceEndpointRequest& service_endpoint_request =
      delegate_->GetServiceEndpointRequest();

  const bool svcb_optional = delegate_->IsSvcbOptional();
  for (const auto& service_endpoint :
       service_endpoint_request.GetEndpointResults()) {
    if (!IsEndpointUsable(service_endpoint, svcb_optional)) {
      continue;
    }
    const std::vector<IPEndPoint>& ip_endpoints =
        ip_endpoint.address().IsIPv4() ? service_endpoint.ipv4_endpoints
                                       : service_endpoint.ipv6_endpoints;

    if (!std::ranges::contains(ip_endpoints, ip_endpoint)) {
      continue;
    }
    return service_endpoint;
  }

  return base::unexpected(TlsStreamAttempt::GetServiceEndpointError::kAbort);
}

void HttpStreamPool::Attempt::OnTcpAttemptSlow(TcpAttempt* attempt) {
  CHECK(attempt == ipv4_attempt_.get() || attempt == ipv6_attempt_.get());
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_SLOW,
      [&] {
        base::DictValue dict;
        dict.Set("ip_endpoint", attempt->ip_endpoint().ToString());
        attempt->stream_attempt().net_log().source().AddToEventParameters(dict);
        return dict;
      });
  observed_slow_attempt_ = true;
  MaybeAttempt();
}

void HttpStreamPool::Attempt::OnTcpAttemptComplete(TcpAttempt* attempt,
                                                   int rv) {
  net_log_.AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_TCP_BASED_ATTEMPT_END, [&] {
        base::DictValue dict;
        dict.Set("ip_endpoint", attempt->ip_endpoint().ToString());
        dict.Set("net_error", rv);
        attempt->stream_attempt().net_log().source().AddToEventParameters(dict);
        return dict;
      });

  std::unique_ptr<TcpAttempt> completed_attempt;
  if (attempt->ip_endpoint().address().IsIPv4()) {
    completed_attempt = std::move(ipv4_attempt_);
  } else {
    completed_attempt = std::move(ipv6_attempt_);
  }
  CHECK_EQ(attempt, completed_attempt.get());

  if (rv != OK) {
    HandleSingleFailure(std::move(completed_attempt), rv);
    // `this` may be deleted at this point.
    return;
  }

  std::unique_ptr<StreamSocket> stream_socket =
      completed_attempt->stream_attempt().ReleaseStreamSocket();
  CHECK(stream_socket);
  LoadTimingInfo::ConnectTiming connect_timing =
      completed_attempt->stream_attempt().connect_timing();
  HostResolver::ServiceEndpointRequest& service_endpoint_request =
      delegate_->GetServiceEndpointRequest();
  stream_socket->SetDnsAliases(service_endpoint_request.GetDnsAliasResults());

  // Reset `completed_attempt` before notifying delegate to prevent dangling
  // pointer.
  completed_attempt.reset();
  base::WeakPtr<Attempt> weak_this = weak_ptr_factory_.GetWeakPtr();
  delegate_->OnStreamSocketReady(this, std::move(stream_socket),
                                 std::move(connect_timing));
  // `this` is deleted.
  CHECK(!weak_this);
}

void HttpStreamPool::Attempt::HandleSingleFailure(
    std::unique_ptr<TcpAttempt> attempt,
    int rv) {
  CHECK_NE(rv, OK);
  CHECK_NE(rv, ERR_IO_PENDING);

  most_recent_attempt_failure_ = {rv, attempt->ip_endpoint()};

  // If the error is fatal, i.e., if the attempt requires a client certificate
  // or a certificate error, we should notify the delegate immediately instead
  // of trying further endpoints.
  if (rv == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info =
        attempt->stream_attempt().GetCertRequestInfo();
    CHECK(ssl_cert_request_info);
    attempt.reset();

    base::WeakPtr<Attempt> weak_this = weak_ptr_factory_.GetWeakPtr();
    delegate_->OnNeedsClientCertificate(this, std::move(ssl_cert_request_info));
    // `this` is deleted.
    CHECK(!weak_this);
    return;
  }

  if (IsCertificateError(rv)) {
    CHECK(attempt->stream_attempt().stream_socket());
    SSLInfo ssl_info;
    bool has_ssl_info =
        attempt->stream_attempt().stream_socket()->GetSSLInfo(&ssl_info);
    CHECK(has_ssl_info);
    attempt.reset();

    base::WeakPtr<Attempt> weak_this = weak_ptr_factory_.GetWeakPtr();
    delegate_->OnCertificateError(this, rv, ssl_info);
    // `this` is deleted.
    CHECK(!weak_this);
    return;
  }

  attempt.reset();

  MaybeAttempt();
  // Be careful of `this` may be deleted at this point.
}

}  // namespace net

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/contains.h"
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
#include "net/socket/stream_attempt.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/socket/tls_stream_attempt.h"
#include "net/ssl/ssl_config.h"

namespace net {

// A single inner TCP-based attempt.
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
    return attempt_->Start(std::move(callback));
  }

  void OnTcpHandshakeComplete() override {
    owner_->OnTcpHandshakeComplete(this);
  }

  int WaitForTlsHandshakeReady(CompletionOnceCallback callback) override {
    int rv = owner_->OnWaitForTlsHandshakeReady(this);
    // TODO(crbug.com/457478038): Handle cases where TLS handshake is not
    // ready yet.
    CHECK_EQ(rv, OK);
    return rv;
  }

  base::expected<ServiceEndpoint, TlsStreamAttempt::GetServiceEndpointError>
  GetServiceEndpointForTlsHandshake() override {
    CHECK(delegate().GetServiceEndpointRequest().EndpointsCryptoReady());
    return owner_->GetServiceEndpointForTlsHandshake(ip_endpoint_);
  }

 private:
  Attempt::Delegate& delegate() const { return owner_->delegate_.get(); }

  const raw_ref<Attempt> owner_;
  const IPEndPoint ip_endpoint_;

  std::unique_ptr<StreamAttempt> attempt_;
};

HttpStreamPool::Attempt::Attempt(
    Delegate& delegate,
    const StreamAttemptParams& stream_attempt_params)
    : delegate_(delegate),
      stream_attempt_params_(stream_attempt_params),
      using_tls_(GURL::SchemeIsCryptographic(
          delegate_->GetHttpStreamKey().destination().scheme())),
      track_(base::trace_event::GetNextGlobalTraceId()) {}

HttpStreamPool::Attempt::~Attempt() = default;

void HttpStreamPool::Attempt::Start() {
  MaybeAttempt();
}

void HttpStreamPool::Attempt::ProcessServiceEndpointChanges() {
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
  int rv = attempt->Start(base::BindOnce(&Attempt::OnTcpAttemptComplete,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         raw_attempt));
  if (rv != ERR_IO_PENDING) {
    OnTcpAttemptComplete(raw_attempt, rv);
    return;
  }

  const bool should_start_slow_timer =
      !slow_timer_expired_ && !slow_timer_.IsRunning();
  if (should_start_slow_timer) {
    // base::Unretained() is safe here because `this` owns `slow_timer_`.
    slow_timer_.Start(
        FROM_HERE, HttpStreamPool::GetConnectionAttemptDelay(),
        base::BindOnce(&Attempt::OnSlowTimerFired, base::Unretained(this)));
  }
}

bool HttpStreamPool::Attempt::IsEndpointUsable(const ServiceEndpoint& endpoint,
                                               bool svcb_optional) const {
  // TODO(crbug.com/457478038): Implement logic similar to
  // AttemptManager::IsEndpointUsableForTcpBasedAttempt().
  return true;
}

std::optional<IPEndPoint> HttpStreamPool::Attempt::GetIPEndPointToAttempt()
    const {
  // Don't attempt more when the first attempt already exists and it's not slow.
  const bool has_attempt = ipv4_attempt_ || ipv6_attempt_;
  if (has_attempt && slow_timer_.IsRunning()) {
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
        // TODO(crbug.com/457478038): Add tests for this case.
        continue;
      }

      const std::vector<IPEndPoint>& ip_endpoints =
          use_ipv6 ? service_endpoint.ipv6_endpoints
                   : service_endpoint.ipv4_endpoints;

      for (const auto& ip_endpoint : ip_endpoints) {
        if (base::Contains(attempted_endpoints_, ip_endpoint)) {
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
      // TODO(crbug.com/457478038): Add tests for this case.
      continue;
    }
    const std::vector<IPEndPoint>& ip_endpoints =
        ip_endpoint.address().IsIPv4() ? service_endpoint.ipv4_endpoints
                                       : service_endpoint.ipv6_endpoints;

    if (!base::Contains(ip_endpoints, ip_endpoint)) {
      // TODO(crbug.com/457478038): Add tests for this case.
      continue;
    }
    return service_endpoint;
  }

  return base::unexpected(TlsStreamAttempt::GetServiceEndpointError::kAbort);
}

void HttpStreamPool::Attempt::OnSlowTimerFired() {
  CHECK(!slow_timer_expired_);
  slow_timer_expired_ = true;
  MaybeAttempt();
}

void HttpStreamPool::Attempt::OnTcpHandshakeComplete(TcpAttempt* attempt) {
  CHECK(attempt == ipv4_attempt_.get() || attempt == ipv6_attempt_.get());
  CHECK(using_tls_);

  // TODO(crbug.com/457478038): Consider pausing the timer when the endpoints
  // are not ready for TLS handshake. The timer should be resumed when the
  // endpoints are ready for TLS handshake.
}

int HttpStreamPool::Attempt::OnWaitForTlsHandshakeReady(TcpAttempt* attempt) {
  CHECK(attempt == ipv4_attempt_.get() || attempt == ipv6_attempt_.get());
  CHECK(using_tls_);

  if (!delegate_->GetServiceEndpointRequest().EndpointsCryptoReady()) {
    // TODO(crbug.com/457478038): Add tests for this case.
    return ERR_IO_PENDING;
  }

  return OK;
}

void HttpStreamPool::Attempt::OnTcpAttemptComplete(TcpAttempt* attempt,
                                                   int rv) {
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

  // Mark the slow timer as expired unconditionally since the slow timer may
  // not be started if the first attempt completed synchronously.
  slow_timer_.Stop();
  slow_timer_expired_ = true;

  std::unique_ptr<StreamSocket> stream_socket =
      completed_attempt->stream_attempt().ReleaseStreamSocket();
  CHECK(stream_socket);
  HostResolver::ServiceEndpointRequest& service_endpoint_request =
      delegate_->GetServiceEndpointRequest();
  stream_socket->SetDnsAliases(service_endpoint_request.GetDnsAliasResults());

  // Reset `completed_attempt` before notifying delegate to prevent dangling
  // pointer.
  completed_attempt.reset();
  base::WeakPtr<Attempt> weak_this = weak_ptr_factory_.GetWeakPtr();
  delegate_->OnStreamSocketReady(this, std::move(stream_socket));
  // `this` is deleted.
  CHECK(!weak_this);
}

void HttpStreamPool::Attempt::HandleSingleFailure(
    std::unique_ptr<TcpAttempt> attempt,
    int rv) {
  most_recent_attempt_failure_ = {rv, attempt->ip_endpoint()};

  attempt.reset();

  CHECK_NE(rv, OK);
  CHECK_NE(rv, ERR_IO_PENDING);
  // If the error is fatal, we should notify the delegate immediately instead of
  // trying further endpoints.
  // TODO(crbug.com/457478038): Handle following fatal errors:
  // - ERR_SSL_CLIENT_AUTH_CERT_NEEDED
  // - Certificate errors
  CHECK_NE(rv, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  CHECK(!IsCertificateError(rv));

  MaybeAttempt();
  // Be careful of `this` may be deleted at this point.
}

}  // namespace net

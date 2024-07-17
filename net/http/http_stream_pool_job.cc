// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/load_states.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/tcp_stream_attempt.h"

namespace net {

HttpStreamPool::Job::RequestEntry::RequestEntry(Job* job) : job_(job) {
  CHECK(job_);
}

std::unique_ptr<HttpStreamRequest>
HttpStreamPool::Job::RequestEntry::CreateRequest(
    HttpStreamRequest::Delegate* delegate,
    const NetLogWithSource& net_log) {
  CHECK(!delegate_);
  CHECK(delegate);

  delegate_ = delegate;

  auto request = std::make_unique<HttpStreamRequest>(
      this, /*websocket_handshake_stream_create_helper=*/nullptr, net_log,
      HttpStreamRequest::HTTP_STREAM);

  request_ = request.get();
  return request;
}

LoadState HttpStreamPool::Job::RequestEntry::GetLoadState() const {
  return job_->GetLoadState();
}

void HttpStreamPool::Job::RequestEntry::OnRequestComplete() {
  CHECK(request_);
  CHECK(delegate_);
  request_ = nullptr;
  delegate_ = nullptr;
  job_->OnRequestComplete(this);
  // `this` is deleted.
}

int HttpStreamPool::Job::RequestEntry::RestartTunnelWithProxyAuth() {
  NOTREACHED_NORETURN();
}

void HttpStreamPool::Job::RequestEntry::SetPriority(RequestPriority priority) {
  CHECK(request_);
  job_->SetRequestPriority(request_, priority);
}

HttpStreamPool::Job::RequestEntry::~RequestEntry() = default;

// Represents an in-flight stream attempt.
struct HttpStreamPool::Job::InFlightAttempt {
  explicit InFlightAttempt(std::unique_ptr<StreamAttempt> attempt)
      : attempt(std::move(attempt)) {}

  InFlightAttempt(const InFlightAttempt&) = delete;
  InFlightAttempt& operator=(const InFlightAttempt&) = delete;

  ~InFlightAttempt() = default;

  std::unique_ptr<StreamAttempt> attempt;
  // TODO(crbug.com/346835898): Add timer to start another attempt when the
  // attempt is slow. The timer will be shorter than StreamAttempt timeouts.
};

HttpStreamPool::Job::Job(Group* group, NetLog* net_log)
    : group_(group),
      net_log_(NetLogWithSource::Make(net_log,
                                      NetLogSourceType::HTTP_STREAM_POOL_JOB)),
      requests_(NUM_PRIORITIES),
      attempt_params_(
          StreamAttemptParams::FromHttpNetworkSession(http_network_session())) {
  proxy_info_.UseDirect();
  CHECK(group_);
}

HttpStreamPool::Job::~Job() = default;

std::unique_ptr<HttpStreamRequest> HttpStreamPool::Job::RequestStream(
    HttpStreamRequest::Delegate* delegate,
    RequestPriority priority,
    const NetLogWithSource& net_log) {
  auto entry = std::make_unique<RequestEntry>(this);
  std::unique_ptr<HttpStreamRequest> request =
      entry->CreateRequest(delegate, net_log);
  requests_.Insert(std::move(entry), priority);
  MaybeChangeServiceEndpointRequestPriority();

  // Check idle streams first. If found, notify the request that an HttpStream
  // is ready. Use PostTask() since `delegate` doesn't expect the request
  // finishes synchronously.
  std::unique_ptr<StreamSocket> stream_socket = group_->GetIdleStreamSocket();
  if (stream_socket) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&Job::CreateTextBasedStreamAndNotify,
                       base::Unretained(this), std::move(stream_socket)));
    return request;
  }

  if (service_endpoint_request_ || service_endpoint_request_finished_) {
    MaybeAttemptConnection();
  } else {
    ResolveServiceEndpoint(priority);
  }

  return request;
}

void HttpStreamPool::Job::OnServiceEndpointsUpdated() {
  MaybeAttemptConnection();
}

void HttpStreamPool::Job::OnServiceEndpointRequestFinished(int rv) {
  CHECK(!service_endpoint_request_finished_);
  CHECK(service_endpoint_request_);
  service_endpoint_request_finished_ = true;
  resolve_error_info_ = service_endpoint_request_->GetResolveErrorInfo();

  if (rv != OK) {
    NotifyFailure(rv);
    return;
  }

  CHECK(!service_endpoint_request_->GetEndpointResults().empty());
  MaybeAttemptConnection();
}

void HttpStreamPool::Job::ProcessPendingRequest() {
  if (PendingRequestCount() == 0) {
    return;
  }

  std::unique_ptr<StreamSocket> stream_socket = group_->GetIdleStreamSocket();
  if (stream_socket) {
    CreateTextBasedStreamAndNotify(std::move(stream_socket));
    return;
  }

  MaybeAttemptConnection(/*max_attempts=*/1);
}

size_t HttpStreamPool::Job::PendingRequestCount() const {
  // The number of in-flight attempts could be larger than the number of
  // requests (e.g. a request was cancelled in the middle of an attempt).
  if (requests_.size() <= in_flight_attempts_.size()) {
    return 0;
  }
  return requests_.size() - in_flight_attempts_.size();
}

const HttpStreamKey& HttpStreamPool::Job::stream_key() const {
  return group_->stream_key();
}

HttpNetworkSession* HttpStreamPool::Job::http_network_session() {
  return group_->http_network_session();
}

HttpStreamPool* HttpStreamPool::Job::pool() {
  return group_->pool();
}

const HttpStreamPool* HttpStreamPool::Job::pool() const {
  return group_->pool();
}

bool HttpStreamPool::Job::UsingTls() const {
  return GURL::SchemeIsCryptographic(stream_key().destination().scheme());
}

LoadState HttpStreamPool::Job::GetLoadState() const {
  if (service_endpoint_request_ && !service_endpoint_request_finished_) {
    return LOAD_STATE_RESOLVING_HOST;
  }

  // TODO(crbug.com/346835898): Add more load state as we implement this class.
  return LOAD_STATE_IDLE;
}

RequestPriority HttpStreamPool::Job::GetPriority() const {
  CHECK(!requests_.empty());
  return static_cast<RequestPriority>(requests_.FirstMax().priority());
}

bool HttpStreamPool::Job::ReachedMaxStreamLimit() const {
  if (pool()->ReachedMaxStreamLimit()) {
    return true;
  }

  if (group_->ReachedMaxStreamLimit()) {
    return true;
  }

  return false;
}

void HttpStreamPool::Job::ResolveServiceEndpoint(
    RequestPriority initial_priority) {
  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = initial_priority;
  parameters.secure_dns_policy = stream_key().secure_dns_policy();
  service_endpoint_request_ =
      http_network_session()->host_resolver()->CreateServiceEndpointRequest(
          HostResolver::Host(stream_key().destination()),
          stream_key().network_anonymization_key(), net_log_,
          std::move(parameters));
  int rv = service_endpoint_request_->Start(this);
  if (rv != ERR_IO_PENDING) {
    OnServiceEndpointRequestFinished(rv);
  }
}

void HttpStreamPool::Job::MaybeChangeServiceEndpointRequestPriority() {
  if (service_endpoint_request_ && !service_endpoint_request_finished_) {
    service_endpoint_request_->ChangeRequestPriority(GetPriority());
  }
}

void HttpStreamPool::Job::MaybeAttemptConnection(
    std::optional<size_t> max_attempts) {
  CHECK_EQ(group_->IdleStreamSocketCount(), 0u);
  // TODO(crbug.com/346835898): Support https scheme.
  CHECK(!UsingTls()) << "https scheme is not supported yet";

  if (PendingRequestCount() == 0) {
    // There are no requests waiting for streams.
    return;
  }

  std::optional<IPEndPoint> ip_endpoint = GetIPEndPointToAttempt();
  if (!ip_endpoint.has_value()) {
    if (service_endpoint_request_finished_ && in_flight_attempts_.empty()) {
      // Tried all endpoints.
      NotifyFailure(last_attempt_error_);
    }
    return;
  }

  // There might be multiple pending requests. Make attempts as much as needed
  // and allowed.
  size_t num_attempts = 0;
  while (PendingRequestCount() > 0 && !ReachedMaxStreamLimit()) {
    std::unique_ptr<StreamAttempt> attempt = std::make_unique<TcpStreamAttempt>(
        &attempt_params_, *ip_endpoint, &net_log_);
    auto in_flight_attempt =
        std::make_unique<InFlightAttempt>(std::move(attempt));
    InFlightAttempt* raw_attempt = in_flight_attempt.get();
    in_flight_attempts_.emplace(std::move(in_flight_attempt));
    pool()->IncrementTotalConnectingStreamCount();

    int rv = raw_attempt->attempt->Start(base::BindOnce(
        &Job::OnInFlightAttemptComplete, base::Unretained(this), raw_attempt));
    if (rv != ERR_IO_PENDING) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Job::OnInFlightAttemptComplete,
                                    base::Unretained(this), raw_attempt, rv));
    }

    ++num_attempts;
    if (max_attempts.has_value() && num_attempts >= *max_attempts) {
      break;
    }
  }
}

std::optional<IPEndPoint> HttpStreamPool::Job::GetIPEndPointToAttempt() {
  CHECK(service_endpoint_request_);
  if (service_endpoint_request_->GetEndpointResults().empty()) {
    return std::nullopt;
  }

  // TODO(crbug.com/346835898): Stagger IPv4/IPv6.
  for (auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    std::optional<IPEndPoint> ip_endpoint =
        FindUnattemptedIPEndPoint(endpoint.ipv6_endpoints);
    if (ip_endpoint.has_value()) {
      return ip_endpoint;
    }
  }

  for (auto& endpoint : service_endpoint_request_->GetEndpointResults()) {
    std::optional<IPEndPoint> ip_endpoint =
        FindUnattemptedIPEndPoint(endpoint.ipv4_endpoints);
    if (ip_endpoint.has_value()) {
      return ip_endpoint;
    }
  }

  return std::nullopt;
}

std::optional<IPEndPoint> HttpStreamPool::Job::FindUnattemptedIPEndPoint(
    const std::vector<IPEndPoint>& ip_endpoints) {
  for (const auto& ip_endpoint : ip_endpoints) {
    if (!base::Contains(failed_ip_endpoints_, ip_endpoint)) {
      return ip_endpoint;
    }
  }
  return std::nullopt;
}

void HttpStreamPool::Job::NotifyFailure(int rv) {
  RequestEntry* entry = ExtractFirstRequestToNotify();
  if (!entry) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::NotifyFailure, weak_ptr_factory_.GetWeakPtr(), rv));

  entry->delegate()->OnStreamFailed(rv, net_error_details_, proxy_info_,
                                    resolve_error_info_);
  // `this` may be deleted.
}

void HttpStreamPool::Job::CreateTextBasedStreamAndNotify(
    std::unique_ptr<StreamSocket> stream_socket) {
  NextProto negotiated_protocol = stream_socket->GetNegotiatedProtocol();
  CHECK_NE(negotiated_protocol, NextProto::kProtoHTTP2);

  std::unique_ptr<HttpStream> http_stream =
      group_->CreateTextBasedStream(std::move(stream_socket));

  RequestEntry* entry = ExtractFirstRequestToNotify();
  if (!entry) {
    // The ownership of the stream will be moved to the group as `http_stream`
    // is going to be destructed.
    return;
  }

  entry->request()->Complete(negotiated_protocol,
                             ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON);
  entry->delegate()->OnStreamReady(proxy_info_, std::move(http_stream));
}

HttpStreamPool::Job::RequestEntry*
HttpStreamPool::Job::ExtractFirstRequestToNotify() {
  if (requests_.empty()) {
    return nullptr;
  }

  std::unique_ptr<RequestEntry> entry = requests_.Erase(requests_.FirstMax());
  RequestEntry* raw_entry = entry.get();
  notified_requests_.emplace(std::move(entry));
  return raw_entry;
}

void HttpStreamPool::Job::SetRequestPriority(HttpStreamRequest* request,
                                             RequestPriority priority) {
  for (RequestQueue::Pointer pointer = requests_.FirstMax(); !pointer.is_null();
       pointer = requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->request() == request) {
      if (pointer.priority() == priority) {
        break;
      }

      std::unique_ptr<RequestEntry> entry = requests_.Erase(pointer);
      requests_.Insert(std::move(entry), priority);
      break;
    }
  }

  MaybeChangeServiceEndpointRequestPriority();
}

void HttpStreamPool::Job::OnRequestComplete(RequestEntry* entry) {
  auto notified_it = notified_requests_.find(entry);
  if (notified_it != notified_requests_.end()) {
    notified_requests_.erase(notified_it);
  } else {
    for (RequestQueue::Pointer pointer = requests_.FirstMax();
         !pointer.is_null();
         pointer = requests_.GetNextTowardsLastMin(pointer)) {
      if (pointer.value()->request() == entry->request()) {
        requests_.Erase(pointer);
        break;
      }
    }
  }
  MaybeComplete();
}

void HttpStreamPool::Job::OnInFlightAttemptComplete(
    InFlightAttempt* raw_attempt,
    int rv) {
  auto it = in_flight_attempts_.find(raw_attempt);
  CHECK(it != in_flight_attempts_.end());
  std::unique_ptr<InFlightAttempt> in_flight_attempt =
      std::move(in_flight_attempts_.extract(it).value());
  pool()->DecrementTotalConnectingStreamCount();

  if (rv != OK) {
    CHECK_NE(rv, ERR_IO_PENDING);
    failed_ip_endpoints_.emplace(in_flight_attempt->attempt->ip_endpoint());
    last_attempt_error_ = rv;
    in_flight_attempt.reset();
    MaybeAttemptConnection();
    return;
  }

  // TODO(crbug.com/346835898): Support preconnect.

  std::unique_ptr<StreamSocket> stream_socket =
      in_flight_attempt->attempt->ReleaseStreamSocket();
  // TODO(crbug.com/346835898): Support HTTP/2.
  CHECK_NE(stream_socket->GetNegotiatedProtocol(), NextProto::kProtoHTTP2);
  CreateTextBasedStreamAndNotify(std::move(stream_socket));
}

void HttpStreamPool::Job::MaybeComplete() {
  // TODO(crbug.com/346835898): Complete `this` when there is no request.
}

}  // namespace net

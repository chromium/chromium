// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_job.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "net/base/load_states.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_with_source.h"

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

HttpStreamRequest::Delegate*
HttpStreamPool::Job::RequestEntry::ExtractDelegate() {
  return std::exchange(delegate_, nullptr);
}

LoadState HttpStreamPool::Job::RequestEntry::GetLoadState() const {
  return job_->GetLoadState();
}

void HttpStreamPool::Job::RequestEntry::OnRequestComplete() {
  CHECK(request_);
  job_->OnRequestComplete(request_);
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

HttpStreamPool::Job::Job(Group* group, NetLog* net_log)
    : group_(group),
      net_log_(NetLogWithSource::Make(net_log,
                                      NetLogSourceType::HTTP_STREAM_POOL_JOB)),
      requests_(NUM_PRIORITIES) {
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

  if (service_endpoint_request_ || service_endpoint_request_finished_) {
    MaybeAttemptConnection();
  } else {
    ResolveServiceEndpoint(priority);
  }

  return request;
}

void HttpStreamPool::Job::OnServiceEndpointsUpdated() {
  // TODO(crbug.com/346835898): Implement.
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

  // TODO(crbug.com/346835898): Implement the success case.
  CHECK(false) << "Not implemented yet";
}

const HttpStreamKey& HttpStreamPool::Job::stream_key() const {
  return group_->stream_key();
}

HttpNetworkSession* HttpStreamPool::Job::http_network_session() {
  return group_->http_network_session();
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

void HttpStreamPool::Job::MaybeAttemptConnection() {
  // TODO(crbug.com/346835898): Implement.
}

void HttpStreamPool::Job::NotifyFailure(int rv) {
  HttpStreamRequest::Delegate* delegate = ExtractFirstRequestDelegate();
  if (!delegate) {
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&Job::NotifyFailure, weak_ptr_factory_.GetWeakPtr(), rv));

  ProxyInfo proxy_info;
  proxy_info.UseDirect();
  delegate->OnStreamFailed(rv, net_error_details_, proxy_info,
                           resolve_error_info_);
  // `this` may be deleted.
}

HttpStreamRequest::Delegate*
HttpStreamPool::Job::ExtractFirstRequestDelegate() {
  for (RequestQueue::Pointer pointer = requests_.FirstMax(); !pointer.is_null();
       pointer = requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->delegate()) {
      return pointer.value()->ExtractDelegate();
    }
  }
  return nullptr;
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

void HttpStreamPool::Job::OnRequestComplete(HttpStreamRequest* request) {
  for (RequestQueue::Pointer pointer = requests_.FirstMax(); !pointer.is_null();
       pointer = requests_.GetNextTowardsLastMin(pointer)) {
    if (pointer.value()->request() == request) {
      requests_.Erase(pointer);
      break;
    }
  }
  MaybeComplete();
}

void HttpStreamPool::Job::MaybeComplete() {
  // TODO(crbug.com/346835898): Complete `this` when there is no request.
}

}  // namespace net

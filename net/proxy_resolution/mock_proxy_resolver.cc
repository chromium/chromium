// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/mock_proxy_resolver.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"

namespace net {

MockAsyncProxyResolver::RequestImpl::RequestImpl(std::unique_ptr<Job> job)
    : job_(std::move(job)) {
  DCHECK(job_);
}

MockAsyncProxyResolver::RequestImpl::~RequestImpl() {
  MockAsyncProxyResolver* resolver = job_->Resolver();
  // AddCancelledJob will check if request is already cancelled
  resolver->AddCancelledJob(std::move(job_));
}

LoadState MockAsyncProxyResolver::RequestImpl::GetLoadState() {
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

MockAsyncProxyResolver::Job::Job(MockAsyncProxyResolver* resolver,
                                 const GURL& url,
                                 ProxyInfo* results,
                                 CompletionOnceCallback callback)
    : resolver_(resolver),
      url_(url),
      results_(results),
      callback_(std::move(callback)) {}

MockAsyncProxyResolver::Job::~Job() = default;

void MockAsyncProxyResolver::Job::CompleteNow(int rv) {
  CompletionOnceCallback callback = std::move(callback_);

  resolver_->RemovePendingJob(this);

  std::move(callback).Run(rv);
}

MockAsyncProxyResolver::~MockAsyncProxyResolver() = default;

int MockAsyncProxyResolver::GetProxyForURL(
    const GURL& url,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request,
    const NetLogWithSource& /*net_log*/) {
  auto job = std::make_unique<Job>(this, url, results, std::move(callback));

  pending_jobs_.push_back(job.get());
  *request = std::make_unique<RequestImpl>(std::move(job));

  // Test code completes the request by calling job->CompleteNow().
  return ERR_IO_PENDING;
}

void MockAsyncProxyResolver::AddCancelledJob(std::unique_ptr<Job> job) {
  auto it = base::ranges::find(pending_jobs_, job.get());
  // Because this is called always when RequestImpl is destructed,
  // we need to check if it is still in pending jobs.
  if (it != pending_jobs_.end()) {
    cancelled_jobs_.push_back(std::move(job));
    pending_jobs_.erase(it);
  }
}

void MockAsyncProxyResolver::RemovePendingJob(Job* job) {
  DCHECK(job);
  auto it = base::ranges::find(pending_jobs_, job);
  CHECK(it != pending_jobs_.end(), base::NotFatalUntil::M130);
  pending_jobs_.erase(it);
}

MockAsyncProxyResolver::MockAsyncProxyResolver() = default;

MockAsyncProxyResolverFactory::Request::Request(
    MockAsyncProxyResolverFactory* factory,
    const scoped_refptr<PacFileData>& script_data,
    std::unique_ptr<ProxyResolver>* resolver,
    CompletionOnceCallback callback)
    : factory_(factory),
      script_data_(script_data),
      resolver_(resolver),
      callback_(std::move(callback)) {}

MockAsyncProxyResolverFactory::Request::~Request() = default;

void MockAsyncProxyResolverFactory::Request::CompleteNow(
    int rv,
    std::unique_ptr<ProxyResolver> resolver) {
  *resolver_ = std::move(resolver);

  // RemovePendingRequest may remove the last external reference to |this|.
  scoped_refptr<MockAsyncProxyResolverFactory::Request> keep_alive(this);
  factory_->RemovePendingRequest(this);
  factory_ = nullptr;
  std::move(callback_).Run(rv);
}

void MockAsyncProxyResolverFactory::Request::CompleteNowWithForwarder(
    int rv,
    ProxyResolver* resolver) {
  DCHECK(resolver);
  CompleteNow(rv, std::make_unique<ForwardingProxyResolver>(resolver));
}

void MockAsyncProxyResolverFactory::Request::FactoryDestroyed() {
  factory_ = nullptr;
}

class MockAsyncProxyResolverFactory::Job
    : public ProxyResolverFactory::Request {
 public:
  explicit Job(
      const scoped_refptr<MockAsyncProxyResolverFactory::Request>& request)
      : request_(request) {}
  ~Job() override {
    if (request_->factory_) {
      request_->factory_->cancelled_requests_.push_back(request_);
      request_->factory_->RemovePendingRequest(request_.get());
    }
  }

 private:
  scoped_refptr<MockAsyncProxyResolverFactory::Request> request_;
};

MockAsyncProxyResolverFactory::MockAsyncProxyResolverFactory(
    bool resolvers_expect_pac_bytes)
    : ProxyResolverFactory(resolvers_expect_pac_bytes) {
}

int MockAsyncProxyResolverFactory::CreateProxyResolver(
    const scoped_refptr<PacFileData>& pac_script,
    std::unique_ptr<ProxyResolver>* resolver,
    CompletionOnceCallback callback,
    std::unique_ptr<ProxyResolverFactory::Request>* request_handle) {
  auto request = base::MakeRefCounted<Request>(this, pac_script, resolver,
                                               std::move(callback));
  pending_requests_.push_back(request);

  *request_handle = std::make_unique<Job>(request);

  // Test code completes the request by calling request->CompleteNow().
  return ERR_IO_PENDING;
}

void MockAsyncProxyResolverFactory::RemovePendingRequest(Request* request) {
  auto it = base::ranges::find(pending_requests_, request);
  CHECK(it != pending_requests_.end(), base::NotFatalUntil::M130);
  pending_requests_.erase(it);
}

MockAsyncProxyResolverFactory::~MockAsyncProxyResolverFactory() {
  for (auto& request : pending_requests_) {
    request->FactoryDestroyed();
  }
}

ForwardingProxyResolver::ForwardingProxyResolver(ProxyResolver* impl)
    : impl_(impl) {
}

int ForwardingProxyResolver::GetProxyForURL(
    const GURL& query_url,
    const NetworkAnonymizationKey& network_anonymization_key,
    ProxyInfo* results,
    CompletionOnceCallback callback,
    std::unique_ptr<Request>* request,
    const NetLogWithSource& net_log) {
  return impl_->GetProxyForURL(query_url, network_anonymization_key, results,
                               std::move(callback), request, net_log);
}

}  // namespace net

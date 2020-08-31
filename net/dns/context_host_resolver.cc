// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/context_host_resolver.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/time/tick_clock.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/resolve_context.h"
#include "net/url_request/url_request_context.h"

namespace net {

// Wrapper of ResolveHostRequests that on destruction will remove itself from
// |ContextHostResolver::handed_out_requests_|.
class ContextHostResolver::WrappedRequest {
 public:
  WrappedRequest(ContextHostResolver* resolver, bool shutting_down)
      : resolver_(resolver), shutting_down_(shutting_down) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(resolver_->sequence_checker_);
  }

  WrappedRequest(const WrappedRequest&) = delete;
  WrappedRequest& operator=(const WrappedRequest&) = delete;

  virtual ~WrappedRequest() { DetachFromResolver(); }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    OnShutdown();
    DetachFromResolver();
  }

  void OnShutdown() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Cannot destroy |inner_request_| because it is still allowed to call
    // Get...Results() methods if the request was already complete.
    if (inner_request())
      inner_request()->Cancel();

    shutting_down_ = true;

    // Not clearing |resolver_| so that early shutdown can be differentiated in
    // Start() from full cancellation on resolver destruction.
  }

  virtual HostResolverManager::CancellableRequest* inner_request() = 0;

  ContextHostResolver* resolver() { return resolver_; }
  bool shutting_down() { return shutting_down_; }

 private:
  void DetachFromResolver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (resolver_) {
      DCHECK_EQ(1u, resolver_->handed_out_requests_.count(this));
      resolver_->handed_out_requests_.erase(this);
      resolver_ = nullptr;
    }
  }

  // Resolver is expected to call Cancel() on destruction, clearing the pointer
  // before it becomes invalid.
  ContextHostResolver* resolver_;
  bool shutting_down_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

class ContextHostResolver::WrappedResolveHostRequest
    : public WrappedRequest,
      public HostResolver::ResolveHostRequest {
 public:
  WrappedResolveHostRequest(
      std::unique_ptr<HostResolverManager::CancellableResolveHostRequest>
          request,
      ContextHostResolver* resolver,
      bool shutting_down)
      : WrappedRequest(resolver, shutting_down),
        inner_request_(std::move(request)) {}

  WrappedResolveHostRequest(const WrappedResolveHostRequest&) = delete;
  WrappedResolveHostRequest& operator=(const WrappedResolveHostRequest&) =
      delete;

  int Start(CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!resolver()) {
      // Parent resolver has been destroyed. HostResolver generally disallows
      // calling Start() in this case, but this implementation returns
      // ERR_FAILED to allow testing the case.
      inner_request_ = nullptr;
      resolve_error_info_ = ResolveErrorInfo(ERR_FAILED);
      return ERR_NAME_NOT_RESOLVED;
    }

    if (shutting_down()) {
      // Shutting down but the resolver is not yet destroyed.
      inner_request_ = nullptr;
      resolve_error_info_ = ResolveErrorInfo(ERR_CONTEXT_SHUT_DOWN);
      return ERR_NAME_NOT_RESOLVED;
    }

    DCHECK(inner_request_);
    return inner_request_->Start(std::move(callback));
  }

  const base::Optional<AddressList>& GetAddressResults() const override {
    if (!inner_request_) {
      static base::NoDestructor<base::Optional<AddressList>> nullopt_result;
      return *nullopt_result;
    }

    return inner_request_->GetAddressResults();
  }

  const base::Optional<std::vector<std::string>>& GetTextResults()
      const override {
    if (!inner_request_) {
      static const base::NoDestructor<base::Optional<std::vector<std::string>>>
          nullopt_result;
      return *nullopt_result;
    }

    return inner_request_->GetTextResults();
  }

  const base::Optional<std::vector<HostPortPair>>& GetHostnameResults()
      const override {
    if (!inner_request_) {
      static const base::NoDestructor<base::Optional<std::vector<HostPortPair>>>
          nullopt_result;
      return *nullopt_result;
    }

    return inner_request_->GetHostnameResults();
  }

  net::ResolveErrorInfo GetResolveErrorInfo() const override {
    if (!inner_request_) {
      return resolve_error_info_;
    }
    return inner_request_->GetResolveErrorInfo();
  }

  const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
      const override {
    if (!inner_request_) {
      static const base::NoDestructor<base::Optional<HostCache::EntryStaleness>>
          nullopt_result;
      return *nullopt_result;
    }

    return inner_request_->GetStaleInfo();
  }

  void ChangeRequestPriority(RequestPriority priority) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(inner_request_);

    inner_request_->ChangeRequestPriority(priority);
  }

  HostResolverManager::CancellableRequest* inner_request() override {
    return inner_request_.get();
  }

 private:
  std::unique_ptr<HostResolverManager::CancellableResolveHostRequest>
      inner_request_;

  // Error info for a |inner_request_| that was destroyed before it started.
  ResolveErrorInfo resolve_error_info_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class ContextHostResolver::WrappedProbeRequest
    : public WrappedRequest,
      public HostResolver::ProbeRequest {
 public:
  WrappedProbeRequest(
      std::unique_ptr<HostResolverManager::CancellableProbeRequest>
          inner_request,
      ContextHostResolver* resolver,
      bool shutting_down)
      : WrappedRequest(resolver, shutting_down),
        inner_request_(std::move(inner_request)) {}

  WrappedProbeRequest(const WrappedProbeRequest&) = delete;
  WrappedProbeRequest& operator=(const WrappedProbeRequest&) = delete;

  int Start() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!resolver()) {
      // Parent resolver has been destroyed. HostResolver generally disallows
      // calling Start() in this case, but this implementation returns
      // ERR_FAILED to allow testing the case.
      inner_request_ = nullptr;
      return ERR_FAILED;
    }

    if (shutting_down()) {
      // Shutting down but the resolver is not yet destroyed.
      inner_request_ = nullptr;
      return ERR_CONTEXT_SHUT_DOWN;
    }

    DCHECK(inner_request_);
    return inner_request_->Start();
  }

  HostResolverManager::CancellableRequest* inner_request() override {
    return inner_request_.get();
  }

 private:
  std::unique_ptr<HostResolverManager::CancellableProbeRequest> inner_request_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ContextHostResolver::ContextHostResolver(
    HostResolverManager* manager,
    std::unique_ptr<ResolveContext> resolve_context)
    : manager_(manager), resolve_context_(std::move(resolve_context)) {
  DCHECK(manager_);
  DCHECK(resolve_context_);

  manager_->RegisterResolveContext(resolve_context_.get());
}

ContextHostResolver::ContextHostResolver(
    std::unique_ptr<HostResolverManager> owned_manager,
    std::unique_ptr<ResolveContext> resolve_context)
    : ContextHostResolver(owned_manager.get(), std::move(resolve_context)) {
  owned_manager_ = std::move(owned_manager);
}

ContextHostResolver::~ContextHostResolver() {
  if (owned_manager_)
    DCHECK_EQ(owned_manager_.get(), manager_);

  // No |resolve_context_| to deregister if OnShutdown() was already called.
  if (resolve_context_)
    manager_->DeregisterResolveContext(resolve_context_.get());

  // Silently cancel all requests associated with this resolver.
  while (!handed_out_requests_.empty())
    (*handed_out_requests_.begin())->Cancel();
}

void ContextHostResolver::OnShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* active_request : handed_out_requests_)
    active_request->OnShutdown();

  DCHECK(resolve_context_);
  manager_->DeregisterResolveContext(resolve_context_.get());
  resolve_context_.reset();

  DCHECK(!shutting_down_);
  shutting_down_ = true;
}

std::unique_ptr<HostResolver::ResolveHostRequest>
ContextHostResolver::CreateRequest(
    const HostPortPair& host,
    const NetworkIsolationKey& network_isolation_key,
    const NetLogWithSource& source_net_log,
    const base::Optional<ResolveHostParameters>& optional_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<HostResolverManager::CancellableResolveHostRequest>
      inner_request;
  if (!shutting_down_) {
    inner_request = manager_->CreateRequest(
        host, network_isolation_key, source_net_log, optional_parameters,
        resolve_context_.get(), resolve_context_->host_cache());
  }

  auto request = std::make_unique<WrappedResolveHostRequest>(
      std::move(inner_request), this, shutting_down_);
  handed_out_requests_.insert(request.get());
  return request;
}

std::unique_ptr<HostResolver::ProbeRequest>
ContextHostResolver::CreateDohProbeRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<HostResolverManager::CancellableProbeRequest> inner_request;
  if (!shutting_down_) {
    inner_request = manager_->CreateDohProbeRequest(resolve_context_.get());
  }

  auto request = std::make_unique<WrappedProbeRequest>(std::move(inner_request),
                                                       this, shutting_down_);
  handed_out_requests_.insert(request.get());
  return request;
}

std::unique_ptr<HostResolver::MdnsListener>
ContextHostResolver::CreateMdnsListener(const HostPortPair& host,
                                        DnsQueryType query_type) {
  return manager_->CreateMdnsListener(host, query_type);
}

HostCache* ContextHostResolver::GetHostCache() {
  return resolve_context_->host_cache();
}

base::Value ContextHostResolver::GetDnsConfigAsValue() const {
  return manager_->GetDnsConfigAsValue();
}

void ContextHostResolver::SetRequestContext(
    URLRequestContext* request_context) {
  DCHECK(handed_out_requests_.empty());
  DCHECK(!shutting_down_);
  DCHECK(resolve_context_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  resolve_context_->set_url_request_context(request_context);
}

HostResolverManager* ContextHostResolver::GetManagerForTesting() {
  return manager_;
}

const URLRequestContext* ContextHostResolver::GetContextForTesting() const {
  return resolve_context_ ? resolve_context_->url_request_context() : nullptr;
}

size_t ContextHostResolver::LastRestoredCacheSize() const {
  return resolve_context_->host_cache()
             ? resolve_context_->host_cache()->last_restore_size()
             : 0;
}

size_t ContextHostResolver::CacheSize() const {
  return resolve_context_->host_cache() ? resolve_context_->host_cache()->size()
                                        : 0;
}

void ContextHostResolver::SetProcParamsForTesting(
    const ProcTaskParams& proc_params) {
  manager_->set_proc_params_for_test(proc_params);
}

void ContextHostResolver::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  manager_->SetTickClockForTesting(tick_clock);
  if (resolve_context_->host_cache())
    resolve_context_->host_cache()->set_tick_clock_for_testing(tick_clock);
}

}  // namespace net

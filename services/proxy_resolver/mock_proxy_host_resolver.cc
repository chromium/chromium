// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/mock_proxy_host_resolver.h"

#include "base/callback.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"

namespace proxy_resolver {

class MockProxyHostResolver::RequestImpl
    : public Request,
      public base::SupportsWeakPtr<RequestImpl> {
 public:
  RequestImpl(std::vector<net::IPAddress> results, bool synchronous_mode)
      : results_(std::move(results)), synchronous_mode_(synchronous_mode) {}
  ~RequestImpl() override = default;

  int Start(net::CompletionOnceCallback callback) override {
    if (!synchronous_mode_) {
      callback_ = std::move(callback);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&RequestImpl::SendResults, AsWeakPtr()));
      return net::ERR_IO_PENDING;
    }

    if (results_.empty())
      return net::ERR_NAME_NOT_RESOLVED;

    return net::OK;
  }

  const std::vector<net::IPAddress>& GetResults() const override {
    DCHECK(!callback_);
    return results_;
  }

 private:
  void SendResults() {
    if (results_.empty())
      std::move(callback_).Run(net::ERR_NAME_NOT_RESOLVED);
    else
      std::move(callback_).Run(net::OK);
  }

  const std::vector<net::IPAddress> results_;
  const bool synchronous_mode_;

  net::CompletionOnceCallback callback_;
};

MockProxyHostResolver::MockProxyHostResolver(bool synchronous_mode)
    : num_resolve_(0), fail_all_(false), synchronous_mode_(synchronous_mode) {}

MockProxyHostResolver::~MockProxyHostResolver() = default;

std::unique_ptr<ProxyHostResolver::Request>
MockProxyHostResolver::CreateRequest(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    const net::NetworkIsolationKey& network_isolation_key) {
  ++num_resolve_;

  if (fail_all_)
    return std::make_unique<RequestImpl>(std::vector<net::IPAddress>(),
                                         synchronous_mode_);

  auto match = results_.find({hostname, operation, network_isolation_key});
  if (match == results_.end())
    return std::make_unique<RequestImpl>(
        std::vector<net::IPAddress>({net::IPAddress(127, 0, 0, 1)}),
        synchronous_mode_);

  return std::make_unique<RequestImpl>(match->second, synchronous_mode_);
}

void MockProxyHostResolver::SetError(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    const net::NetworkIsolationKey& network_isolation_key) {
  fail_all_ = false;
  results_[{hostname, operation, network_isolation_key}].clear();
}

void MockProxyHostResolver::SetResult(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    const net::NetworkIsolationKey& network_isolation_key,
    std::vector<net::IPAddress> result) {
  DCHECK(!result.empty());
  fail_all_ = false;
  results_[{hostname, operation, network_isolation_key}] = std::move(result);
}

void MockProxyHostResolver::FailAll() {
  results_.clear();
  fail_all_ = true;
}

class HangingProxyHostResolver::RequestImpl : public Request {
 public:
  explicit RequestImpl(HangingProxyHostResolver* resolver)
      : resolver_(resolver) {}
  ~RequestImpl() override { ++resolver_->num_cancelled_requests_; }

  int Start(net::CompletionOnceCallback callback) override {
    if (resolver_->hang_callback_)
      resolver_->hang_callback_.Run();
    return net::ERR_IO_PENDING;
  }

  const std::vector<net::IPAddress>& GetResults() const override {
    IMMEDIATE_CRASH();
  }

 private:
  HangingProxyHostResolver* resolver_;
};

HangingProxyHostResolver::HangingProxyHostResolver(
    base::RepeatingClosure hang_callback)
    : num_cancelled_requests_(0), hang_callback_(std::move(hang_callback)) {}

HangingProxyHostResolver::~HangingProxyHostResolver() = default;

std::unique_ptr<ProxyHostResolver::Request>
HangingProxyHostResolver::CreateRequest(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    const net::NetworkIsolationKey& network_isolation_key) {
  return std::make_unique<RequestImpl>(this);
}

}  // namespace proxy_resolver

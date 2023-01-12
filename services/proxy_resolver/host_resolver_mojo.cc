// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/host_resolver_mojo.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"
#include "services/proxy_resolver/proxy_host_resolver_cache.h"

namespace proxy_resolver {

namespace {

bool IsExOperation(net::ProxyResolveDnsOperation operation) {
  return operation == net::ProxyResolveDnsOperation::DNS_RESOLVE_EX ||
         operation == net::ProxyResolveDnsOperation::MY_IP_ADDRESS_EX;
}

}  // namespace

class HostResolverMojo::RequestImpl : public ProxyHostResolver::Request,
                                      public mojom::HostResolverRequestClient {
 public:
  RequestImpl(const std::string& hostname,
              net::ProxyResolveDnsOperation operation,
              const net::NetworkAnonymizationKey& network_anonymization_key,
              base::WeakPtr<ProxyHostResolverCache> host_cache,
              Impl* impl)
      : hostname_(hostname),
        operation_(operation),
        network_anonymization_key_(network_anonymization_key),
        host_cache_(std::move(host_cache)),
        impl_(impl) {}

  ~RequestImpl() override = default;

  // ProxyHostResolver::Request override
  int Start(net::CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(1) << "Resolve " << hostname_;

    // Get from a local cache if able. Even though the network process does its
    // own HostResolver caching, that would still require an async mojo call.
    // Async returns are particularly expensive here, so the local cache
    // maximizes ability to return synchronously.
    //
    // TODO(ericorth@chromium.org): Consider some small refactors to allow
    // direct non-mojo access to the fast, synchronous, and self-contained logic
    // from net::HostResolver (e.g. IP-literal and "localhost" resolution). That
    // could allow reducing async returns even further.
    DCHECK(host_cache_);
    const std::vector<net::IPAddress>* cached_result = host_cache_->LookupEntry(
        hostname_, network_anonymization_key_, IsExOperation(operation_));
    if (cached_result) {
      results_ = *cached_result;
      DVLOG(1) << "Resolved " << hostname_ << " from cache";
      return net::OK;
    }

    callback_ = std::move(callback);
    impl_->ResolveDns(hostname_, operation_, network_anonymization_key_,
                      receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&RequestImpl::OnDisconnect, base::Unretained(this)));
    return net::ERR_IO_PENDING;
  }

  const std::vector<net::IPAddress>& GetResults() const override {
    return results_;
  }

  // mojom::HostResolverRequestClient override
  void ReportResult(int32_t error,
                    const std::vector<net::IPAddress>& result) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (error == net::OK) {
      results_ = result;
      if (host_cache_) {
        host_cache_->StoreEntry(hostname_, network_anonymization_key_,
                                IsExOperation(operation_), result);
      }
    }
    receiver_.reset();
    std::move(callback_).Run(error);
  }

 private:
  void OnDisconnect() { ReportResult(net::ERR_FAILED, {} /* result */); }

  const std::string hostname_;
  const net::ProxyResolveDnsOperation operation_;
  const net::NetworkAnonymizationKey network_anonymization_key_;

  mojo::Receiver<mojom::HostResolverRequestClient> receiver_{this};
  net::CompletionOnceCallback callback_;

  base::WeakPtr<ProxyHostResolverCache> host_cache_;
  const raw_ptr<Impl> impl_;
  std::vector<net::IPAddress> results_;

  THREAD_CHECKER(thread_checker_);
};

HostResolverMojo::HostResolverMojo(Impl* impl) : impl_(impl) {}

HostResolverMojo::~HostResolverMojo() = default;

std::unique_ptr<ProxyHostResolver::Request> HostResolverMojo::CreateRequest(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<RequestImpl>(
      hostname, operation, network_anonymization_key,
      host_cache_weak_factory_.GetWeakPtr(), impl_);
}

}  // namespace proxy_resolver

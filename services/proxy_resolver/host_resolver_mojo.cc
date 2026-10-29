// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/host_resolver_mojo.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"

namespace proxy_resolver {

class HostResolverMojo::RequestImpl : public ProxyHostResolver::Request,
                                      public mojom::HostResolverRequestClient {
 public:
  RequestImpl(const std::string& hostname,
              net::ProxyResolveDnsOperation operation,
              Impl* impl)
      : hostname_(hostname), operation_(operation), impl_(impl) {}

  ~RequestImpl() override = default;

  // ProxyHostResolver::Request override
  int Start(net::CompletionOnceCallback callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(1) << "Resolve " << hostname_;

    callback_ = std::move(callback);
    impl_->ResolveDns(hostname_, operation_,
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
    }
    receiver_.reset();
    std::move(callback_).Run(error);
  }

 private:
  void OnDisconnect() { ReportResult(net::ERR_FAILED, {} /* result */); }

  const std::string hostname_;
  const net::ProxyResolveDnsOperation operation_;

  mojo::Receiver<mojom::HostResolverRequestClient> receiver_{this};
  net::CompletionOnceCallback callback_;

  const raw_ptr<Impl> impl_;
  std::vector<net::IPAddress> results_;

  THREAD_CHECKER(thread_checker_);
};

HostResolverMojo::HostResolverMojo(Impl* impl) : impl_(impl) {}

HostResolverMojo::~HostResolverMojo() = default;

std::unique_ptr<ProxyHostResolver::Request> HostResolverMojo::CreateRequest(
    const std::string& hostname,
    net::ProxyResolveDnsOperation operation) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<RequestImpl>(hostname, operation, impl_);
}

}  // namespace proxy_resolver

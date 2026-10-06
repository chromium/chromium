// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/linux_system_proxy_resolver_mojo.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/proxy_resolution/linux/linux_proxy_resolution_status.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolver.h"
#include "net/proxy_resolution/proxy_list.h"

namespace network {

namespace {

net::LinuxProxyResolutionStatus ToNetStatus(
    proxy_resolver::mojom::LinuxProxyStatus status) {
  using proxy_resolver::mojom::LinuxProxyStatus;
  switch (status) {
    case LinuxProxyStatus::kOk:
      return net::LinuxProxyResolutionStatus::kOk;
    case LinuxProxyStatus::kPortalUnavailable:
      return net::LinuxProxyResolutionStatus::kPortalUnavailable;
    case LinuxProxyStatus::kDBusError:
      return net::LinuxProxyResolutionStatus::kDBusError;
    case LinuxProxyStatus::kEmptyProxyList:
      return net::LinuxProxyResolutionStatus::kEmptyProxyList;
    case LinuxProxyStatus::kInvalidResponse:
      return net::LinuxProxyResolutionStatus::kInvalidResponse;
    case LinuxProxyStatus::kAborted:
      return net::LinuxProxyResolutionStatus::kAborted;
  }
  NOTREACHED();
}

proxy_resolver::mojom::SystemProxyResolutionStatusPtr MakeAbortedStatus() {
  auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
  status->is_success = false;
  status->os_error = 0;
  status->linux_proxy_status =
      proxy_resolver::mojom::LinuxProxyStatus::kAborted;
  return status;
}

}  // namespace

class LinuxSystemProxyResolverMojo::RequestImpl final
    : public net::LinuxSystemProxyResolver::Request {
 public:
  RequestImpl(LinuxSystemProxyResolverMojo* resolver,
              const GURL& url,
              net::LinuxSystemProxyResolver::ResultCallback callback);
  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;
  ~RequestImpl() override;

 private:
  // Implements the callback for `GetProxyForUrl()`.
  void ReportResult(
      const net::ProxyList& proxy_list,
      proxy_resolver::mojom::SystemProxyResolutionStatusPtr status);

  net::LinuxSystemProxyResolver::ResultCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LinuxSystemProxyResolverMojo::RequestImpl>
      weak_ptr_factory_{this};
};

LinuxSystemProxyResolverMojo::RequestImpl::RequestImpl(
    LinuxSystemProxyResolverMojo* resolver,
    const GURL& url,
    net::LinuxSystemProxyResolver::ResultCallback callback)
    : callback_(std::move(callback)) {
  CHECK(callback_);
  // If the browser end of the pipe goes away before replying, complete the
  // request as aborted instead of leaving it pending forever.
  resolver->mojo_linux_system_proxy_resolver_->GetProxyForUrl(
      url, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
               base::BindOnce(
                   &LinuxSystemProxyResolverMojo::RequestImpl::ReportResult,
                   weak_ptr_factory_.GetWeakPtr()),
               net::ProxyList(), MakeAbortedStatus()));
}

LinuxSystemProxyResolverMojo::RequestImpl::~RequestImpl() {
  // Destroying the `RequestImpl` is the intended way of "canceling" a proxy
  // resolution. The weak pointer bound into the Mojo reply ensures `callback_`
  // is never run afterwards.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LinuxSystemProxyResolverMojo::RequestImpl::ReportResult(
    const net::ProxyList& proxy_list,
    proxy_resolver::mojom::SystemProxyResolutionStatusPtr status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(status);
  net::LinuxProxyResolutionStatus linux_status =
      status->is_success
          ? net::LinuxProxyResolutionStatus::kOk
          : ToNetStatus(status->linux_proxy_status.value_or(
                proxy_resolver::mojom::LinuxProxyStatus::kAborted));

  // Running the callback may destroy `this`.
  std::move(callback_).Run(proxy_list, linux_status);
}

LinuxSystemProxyResolverMojo::LinuxSystemProxyResolverMojo(
    mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
        mojo_linux_system_proxy_resolver)
    : mojo_linux_system_proxy_resolver_(
          std::move(mojo_linux_system_proxy_resolver)) {}

LinuxSystemProxyResolverMojo::~LinuxSystemProxyResolverMojo() = default;

std::unique_ptr<net::LinuxSystemProxyResolver::Request>
LinuxSystemProxyResolverMojo::GetProxyForUrl(const GURL& url,
                                             ResultCallback callback) {
  return std::make_unique<RequestImpl>(this, url, std::move(callback));
}

}  // namespace network

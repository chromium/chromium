// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mac_system_proxy_resolver_mojo.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolution_request.h"
#include "net/proxy_resolution/proxy_list.h"

namespace network {

class MacSystemProxyResolverMojo::RequestImpl final
    : public net::MacSystemProxyResolver::Request {
 public:
  RequestImpl(MacSystemProxyResolverMojo* resolver,
              const GURL& url,
              net::MacSystemProxyResolutionRequest* callback_target);
  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;
  ~RequestImpl() override;

 private:
  // Implements the callback for GetProxyForUrl()
  void ReportResult(
      const net::ProxyList& proxy_list,
      proxy_resolver::mojom::SystemProxyResolutionStatusPtr status);

  // As described at MacSystemProxyResolver::GetProxyForUrl,
  // `callback_target_` must outlive `this`.
  raw_ptr<net::MacSystemProxyResolutionRequest> callback_target_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MacSystemProxyResolverMojo::RequestImpl>
      weak_ptr_factory_{this};
};

MacSystemProxyResolverMojo::RequestImpl::RequestImpl(
    MacSystemProxyResolverMojo* resolver,
    const GURL& url,
    net::MacSystemProxyResolutionRequest* callback_target)
    : callback_target_(callback_target) {
  DCHECK(callback_target_);
  resolver->mojo_mac_system_proxy_resolver_->GetProxyForUrl(
      url,
      base::BindOnce(&MacSystemProxyResolverMojo::RequestImpl::ReportResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

MacSystemProxyResolverMojo::RequestImpl::~RequestImpl() {
  // This does not need to check if there is an ongoing proxy resolution.
  // Destroying the RequestImpl is the intended way of "canceling" a proxy
  // resolution.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MacSystemProxyResolverMojo::RequestImpl::ReportResult(
    const net::ProxyList& proxy_list,
    proxy_resolver::mojom::SystemProxyResolutionStatusPtr status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(status);
  net::MacProxyResolutionStatus mac_status =
      status->is_success ? net::MacProxyResolutionStatus::kOk
                         : status->mac_proxy_status.value_or(
                               net::MacProxyResolutionStatus::kAborted);
  int os_error = status->os_error;

  callback_target_->ProxyResolutionComplete(proxy_list, mac_status, os_error);
}

MacSystemProxyResolverMojo::MacSystemProxyResolverMojo(
    mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
        mojo_mac_system_proxy_resolver)
    : mojo_mac_system_proxy_resolver_(
          std::move(mojo_mac_system_proxy_resolver)) {}

MacSystemProxyResolverMojo::~MacSystemProxyResolverMojo() = default;

std::unique_ptr<net::MacSystemProxyResolver::Request>
MacSystemProxyResolverMojo::GetProxyForUrl(
    const GURL& url,
    net::MacSystemProxyResolutionRequest* callback_target) {
  return std::make_unique<RequestImpl>(this, url, callback_target);
}

}  // namespace network

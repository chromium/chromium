// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/windows_system_proxy_resolver_mojo.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/proxy_resolution/proxy_list.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_request.h"
#include "net/proxy_resolution/win/winhttp_status.h"

namespace network {

class WindowsSystemProxyResolverMojo::RequestImpl final
    : public net::WindowsSystemProxyResolver::Request {
 public:
  RequestImpl(WindowsSystemProxyResolverMojo* resolver,
              const GURL& url,
              net::WindowsSystemProxyResolutionRequest* callback_target);
  RequestImpl(const RequestImpl&) = delete;
  RequestImpl& operator=(const RequestImpl&) = delete;
  ~RequestImpl() override;

 private:
  // Implements the callback for GetProxyForUrl()
  void ReportResult(const net::ProxyList& proxy_list,
                    net::WinHttpStatus winhttp_status,
                    int windows_error);

  // As described at WindowsSystemProxyResolutionRequest::GetProxyForUrl,
  // `callback_target_` must outlive `this`.
  raw_ptr<net::WindowsSystemProxyResolutionRequest> callback_target_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WindowsSystemProxyResolverMojo::RequestImpl>
      weak_ptr_factory_{this};
};

WindowsSystemProxyResolverMojo::RequestImpl::RequestImpl(
    WindowsSystemProxyResolverMojo* resolver,
    const GURL& url,
    net::WindowsSystemProxyResolutionRequest* callback_target)
    : callback_target_(callback_target) {
  DCHECK(callback_target_);
  resolver->mojo_windows_system_proxy_resolver_->GetProxyForUrl(
      url,
      base::BindOnce(&WindowsSystemProxyResolverMojo::RequestImpl::ReportResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

WindowsSystemProxyResolverMojo::RequestImpl::~RequestImpl() {
  // This does not need to check if there is an ongoing proxy resolution.
  // Destroying the RequestImpl is the intended way of "canceling" a proxy
  // resolution.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WindowsSystemProxyResolverMojo::RequestImpl::ReportResult(
    const net::ProxyList& proxy_list,
    net::WinHttpStatus winhttp_status,
    int windows_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_target_->ProxyResolutionComplete(proxy_list, winhttp_status,
                                            windows_error);
}

WindowsSystemProxyResolverMojo::WindowsSystemProxyResolverMojo(
    mojo::PendingRemote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
        mojo_windows_system_proxy_resolver)
    : mojo_windows_system_proxy_resolver_(
          std::move(mojo_windows_system_proxy_resolver)) {}

WindowsSystemProxyResolverMojo::~WindowsSystemProxyResolverMojo() = default;

std::unique_ptr<net::WindowsSystemProxyResolver::Request>
WindowsSystemProxyResolverMojo::GetProxyForUrl(
    const GURL& url,
    net::WindowsSystemProxyResolutionRequest* callback_target) {
  return std::make_unique<RequestImpl>(this, url, callback_target);
}

}  // namespace network

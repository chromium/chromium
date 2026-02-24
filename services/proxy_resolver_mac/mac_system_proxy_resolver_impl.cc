// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_mac/mac_system_proxy_resolver_impl.h"

#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "net/proxy_resolution/mac/mac_proxy_resolution_status.h"
#include "net/proxy_resolution/proxy_chain_util_apple.h"
#include "net/proxy_resolution/proxy_list.h"

namespace proxy_resolver_mac {

MacSystemProxyResolverImpl::MacSystemProxyResolverImpl(
    mojo::PendingReceiver<proxy_resolver::mojom::SystemProxyResolver> receiver)
    : MacSystemProxyResolverImpl(std::move(receiver), MacAPIWrapper::Create()) {
}

MacSystemProxyResolverImpl::MacSystemProxyResolverImpl(
    mojo::PendingReceiver<proxy_resolver::mojom::SystemProxyResolver> receiver,
    std::unique_ptr<MacAPIWrapper> mac_api_wrapper)
    : receiver_(this, std::move(receiver)),
      mac_api_wrapper_(std::move(mac_api_wrapper)) {}

MacSystemProxyResolverImpl::~MacSystemProxyResolverImpl() = default;

void MacSystemProxyResolverImpl::GetProxyForUrl(
    const GURL& url,
    GetProxyForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto make_status = [](net::MacProxyResolutionStatus mac_status) {
    auto status = proxy_resolver::mojom::SystemProxyResolutionStatus::New();
    status->is_success = (mac_status == net::MacProxyResolutionStatus::kOk);
    status->os_error = 0;
    status->mac_proxy_status = mac_status;
    return status;
  };

  // Step 1: Get the system proxy configuration dictionary.
  auto proxy_settings = mac_api_wrapper_->CopyProxies();
  if (!proxy_settings) {
    std::move(callback).Run(
        net::ProxyList(),
        make_status(net::MacProxyResolutionStatus::kSystemConfigurationError));
    return;
  }

  // Step 2: Resolve proxies for the given URL using the system configuration.
  auto proxy_array =
      mac_api_wrapper_->CopyProxiesForURL(url, proxy_settings.get());
  if (!proxy_array) {
    std::move(callback).Run(
        net::ProxyList(),
        make_status(net::MacProxyResolutionStatus::kCFNetworkResolutionError));
    return;
  }

  // Step 3: Iterate the array of proxy dictionaries and build a ProxyList.
  net::ProxyList proxy_list;
  CFIndex proxy_array_count = CFArrayGetCount(proxy_array.get());
  for (CFIndex i = 0; i < proxy_array_count; ++i) {
    CFDictionaryRef proxy_dictionary =
        base::apple::CFCastStrict<CFDictionaryRef>(
            CFArrayGetValueAtIndex(proxy_array.get(), i));
    if (!proxy_dictionary) {
      continue;
    }

    CFStringRef proxy_type = base::apple::GetValueFromDictionary<CFStringRef>(
        proxy_dictionary, kCFProxyTypeKey);
    if (!proxy_type) {
      continue;
    }

    // PAC entries (both URL and inline JavaScript) require script execution
    // which is not yet supported.
    // TODO(crbug.com/442313607): Add PAC support to the utility process proxy
    // resolver.
    if (CFEqual(proxy_type, kCFProxyTypeAutoConfigurationURL) ||
        CFEqual(proxy_type, kCFProxyTypeAutoConfigurationJavaScript)) {
      std::move(callback).Run(
          net::ProxyList(),
          make_status(
              net::MacProxyResolutionStatus::kCFNetworkExecutePacScriptFailed));
      return;
    }

    net::ProxyChain proxy_chain = net::ProxyDictionaryToProxyChain(
        proxy_type, proxy_dictionary, kCFProxyHostNameKey,
        kCFProxyPortNumberKey);
    proxy_list.AddProxyChain(proxy_chain);
  }

  if (proxy_list.IsEmpty()) {
    std::move(callback).Run(
        net::ProxyList(),
        make_status(net::MacProxyResolutionStatus::kEmptyProxyList));
    return;
  }

  std::move(callback).Run(std::move(proxy_list),
                          make_status(net::MacProxyResolutionStatus::kOk));
}

}  // namespace proxy_resolver_mac

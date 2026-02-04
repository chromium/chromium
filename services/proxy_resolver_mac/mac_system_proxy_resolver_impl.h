// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_MAC_MAC_SYSTEM_PROXY_RESOLVER_IMPL_H_
#define SERVICES_PROXY_RESOLVER_MAC_MAC_SYSTEM_PROXY_RESOLVER_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/proxy_resolver_mac/mac_api_wrapper/mac_api_wrapper.h"

namespace proxy_resolver_mac {

// Implementation of the SystemProxyResolver Mojo interface for macOS.
// This class runs in a sandboxed utility process and uses the MacAPIWrapper
// abstraction to make SystemConfiguration/CFNetwork API calls.
class COMPONENT_EXPORT(PROXY_RESOLVER_MAC) MacSystemProxyResolverImpl
    : public proxy_resolver::mojom::SystemProxyResolver {
 public:
  // Creates an instance with the production MacAPIWrapper.
  explicit MacSystemProxyResolverImpl(
      mojo::PendingReceiver<proxy_resolver::mojom::SystemProxyResolver>
          receiver);

  // Creates an instance with a custom MacAPIWrapper for testing.
  MacSystemProxyResolverImpl(
      mojo::PendingReceiver<proxy_resolver::mojom::SystemProxyResolver>
          receiver,
      std::unique_ptr<MacAPIWrapper> mac_api_wrapper);

  ~MacSystemProxyResolverImpl() override;

  MacSystemProxyResolverImpl(const MacSystemProxyResolverImpl&) = delete;
  MacSystemProxyResolverImpl& operator=(const MacSystemProxyResolverImpl&) =
      delete;

  // proxy_resolver::mojom::SystemProxyResolver implementation:
  void GetProxyForUrl(const GURL& url,
                      GetProxyForUrlCallback callback) override;

 private:
  mojo::Receiver<proxy_resolver::mojom::SystemProxyResolver> receiver_;
  std::unique_ptr<MacAPIWrapper> mac_api_wrapper_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace proxy_resolver_mac

#endif  // SERVICES_PROXY_RESOLVER_MAC_MAC_SYSTEM_PROXY_RESOLVER_IMPL_H_

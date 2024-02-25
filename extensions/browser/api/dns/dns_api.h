// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DNS_DNS_API_H_
#define EXTENSIONS_BROWSER_API_DNS_DNS_API_H_

#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

class DnsResolveFunction : public ExtensionFunction,
                           public network::ResolveHostClientBase {
 public:
  DECLARE_EXTENSION_FUNCTION("dns.resolve", DNS_RESOLVE)

  DnsResolveFunction();

 protected:
  ~DnsResolveFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  // A reference to |this| must be taken while the request is being made on this
  // receiver so the object is alive when the request completes.
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DNS_DNS_API_H_

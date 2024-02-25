// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SYSTEM_DNS_RESOLUTION_SYSTEM_DNS_RESOLVER_H_
#define CONTENT_BROWSER_SYSTEM_DNS_RESOLUTION_SYSTEM_DNS_RESOLVER_H_

#include <optional>

#include "services/network/public/mojom/system_dns_resolution.mojom.h"

namespace content {

// An implementation of network::mojom::SystemDnsResolver that just calls
// //net/dns's HostResolverSystemTask.
class SystemDnsResolverMojoImpl : public network::mojom::SystemDnsResolver {
 public:
  SystemDnsResolverMojoImpl();

  SystemDnsResolverMojoImpl(const SystemDnsResolverMojoImpl&) = delete;
  SystemDnsResolverMojoImpl& operator=(const SystemDnsResolverMojoImpl&) =
      delete;

  ~SystemDnsResolverMojoImpl() override = default;

  // network::mojom::SystemDnsResolver impl:
  void Resolve(const std::optional<std::string>& hostname,
               net::AddressFamily addr_family,
               int32_t flags,
               uint64_t network,
               ResolveCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SYSTEM_DNS_RESOLUTION_SYSTEM_DNS_RESOLVER_H_

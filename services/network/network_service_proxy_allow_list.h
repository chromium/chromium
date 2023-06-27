// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_PROXY_ALLOW_LIST_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_PROXY_ALLOW_LIST_H_

#include <memory>
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// Class NetworkServiceProxyAllowlist is a pseudo-singleton owned by the
// NetworkService. It uses the MaskedDomainList to generate the
// CustomProxyConfigPtr needed for NetworkContexts that are using the Privacy
// Proxy and determines if pairs of request and top_frame URLs are eligible.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceProxyAllowList {
 public:
  NetworkServiceProxyAllowList();
  ~NetworkServiceProxyAllowList();

  static NetworkServiceProxyAllowList CreateForTesting(
      std::map<std::string, std::set<std::string>> first_party_map);

  mojom::CustomProxyConfigPtr GetCustomProxyConfig();

  bool IsEnabled();

  // Determines if the pair of URLs are eligible for the proxy by determining
  // if the request is an eligible domain and if the top frame domain is
  // considered a first or third party.
  bool Matches(const GURL& request_url, const GURL& top_frame_url);

  // Use the Masked Domain List to generate the allow list and the 1P bypass
  // rules.
  void UseMaskedDomainList(const masked_domain_list::MaskedDomainList& mdl);

 private:
  explicit NetworkServiceProxyAllowList(
      std::map<std::string, net::ProxyBypassRules> first_party_exclusion_map);

  mojom::CustomProxyConfigPtr custom_proxy_config_;

  // Maps each domain eligible for the proxy to the top frame domains that allow
  // the proxy to be bypassed.
  std::map<std::string, net::ProxyBypassRules> allow_list_with_bypass_map_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_PROXY_ALLOW_LIST_H_

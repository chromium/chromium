// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MASKED_DOMAIN_LIST_NETWORK_SERVICE_PROXY_ALLOW_LIST_H_
#define SERVICES_NETWORK_MASKED_DOMAIN_LIST_NETWORK_SERVICE_PROXY_ALLOW_LIST_H_

#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "services/network/masked_domain_list/url_matcher_with_bypass.h"
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
  NetworkServiceProxyAllowList(const NetworkServiceProxyAllowList&);

  static NetworkServiceProxyAllowList CreateForTesting(
      std::map<std::string, std::set<std::string>> first_party_map);

  // Create a custom proxy config that instructs NetworkServiceProxyDelegate to
  // handle IP protection.
  static mojom::CustomProxyConfigPtr MakeIpProtectionCustomProxyConfig();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns true if the allow list is eligible to be used but does not indicate
  // that allow list is currently populated.
  bool IsEnabled();

  // Returns true if there are entries in the allow list and it is possible to
  // match on them. If false, `Matches` will always return false.
  bool IsPopulated();

  // Determines if the pair of URLs are eligible for the proxy by determining
  // if the request is an eligible domain and if the top frame domain is
  // considered a first or third party.
  bool Matches(const GURL& request_url, const GURL& top_frame_url);

  // Use the Masked Domain List to generate the allow list and the 1P bypass
  // rules.
  void UseMaskedDomainList(const masked_domain_list::MaskedDomainList& mdl);

 private:
  void AddDomainWithBypass(const std::string& domain,
                           net::SchemeHostPortMatcher bypass_matcher);

  // Contains match rules from the Masked Domain List.
  UrlMatcherWithBypass url_matcher_with_bypass_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MASKED_DOMAIN_LIST_NETWORK_SERVICE_PROXY_ALLOW_LIST_H_

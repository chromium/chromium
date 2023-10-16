// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MASKED_DOMAIN_LIST_NETWORK_SERVICE_RESOURCE_BLOCK_LIST_H_
#define SERVICES_NETWORK_MASKED_DOMAIN_LIST_NETWORK_SERVICE_RESOURCE_BLOCK_LIST_H_

#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/isolation_info.h"
#include "services/network/masked_domain_list/url_matcher_with_bypass.h"
#include "url/gurl.h"

namespace network {

// Class NetworkServiceResourceBlockList is a pseudo-singleton owned by the
// NetworkService. It uses the MaskedDomainList (for the initial experiment) to
// generate a block list that only blocks eligible resources that are used in a
// 3rd party context.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceResourceBlockList {
 public:
  NetworkServiceResourceBlockList();
  ~NetworkServiceResourceBlockList();

  void AddDomainWithBypassForTesting(const std::string& domain,
                                     net::SchemeHostPortMatcher bypass_matcher);

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns true if there are entries in the block list and it is possible to
  // match on them. If false, `Matches` will always return false.
  bool IsPopulated();

  // Returns true if request_url is in the block list and the top frame URL from
  // the IsolationInfo is not a first party domain (first parties have bypass
  // rules).
  bool Matches(const GURL& request_url,
               const absl::optional<net::IsolationInfo>& isolation_info);

  // Use the Masked Domain List to generate the block list and the 1P bypass
  // rules.
  void UseMaskedDomainList(const masked_domain_list::MaskedDomainList& mdl);

 private:
  // Contains experimental match rules from the Masked Domain List.
  UrlMatcherWithBypass url_matcher_with_bypass_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MASKED_DOMAIN_LIST_NETWORK_SERVICE_RESOURCE_BLOCK_LIST_H_

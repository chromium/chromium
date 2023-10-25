// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/network_service_resource_block_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {

bool ResourceIsEligibleForBlockList(
    const masked_domain_list::Resource& resource) {
  return std::find(resource.experiments().begin(), resource.experiments().end(),
                   masked_domain_list::Resource_Experiment_EXPERIMENT_AFP) !=
         resource.experiments().end();
}

}  // namespace

NetworkServiceResourceBlockList::NetworkServiceResourceBlockList() = default;
NetworkServiceResourceBlockList::~NetworkServiceResourceBlockList() = default;

void NetworkServiceResourceBlockList::AddDomainWithBypassForTesting(
    const std::string& domain,
    net::SchemeHostPortMatcher bypass_matcher) {
  url_matcher_with_bypass_.AddDomainWithBypass(domain,
                                               std::move(bypass_matcher));
}

size_t NetworkServiceResourceBlockList::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_matcher_with_bypass_);
}

bool NetworkServiceResourceBlockList::IsPopulated() {
  return url_matcher_with_bypass_.IsPopulated();
}

bool NetworkServiceResourceBlockList::Matches(
    const GURL& request_url,
    const absl::optional<net::IsolationInfo>& isolation_info) {
  // If there is no isolation_info, it is not possible to determine if the
  // request is in a 3rd party context and it should not be blocked.
  if (!isolation_info || isolation_info->IsEmpty() ||
      isolation_info->top_frame_origin()->GetURL().is_empty()) {
    VLOG(3) << "NSRBL::Matches(" << request_url
            << ") - false (empty isolation_info)";
    return false;
  }

  VLOG(3) << "NSRBL::Matches(" << request_url << ", "
          << isolation_info->top_frame_origin()->GetURL() << ")";
  UrlMatcherWithBypass::MatchResult result = url_matcher_with_bypass_.Matches(
      request_url, isolation_info->top_frame_origin()->GetURL());
  return result.matches && result.is_third_party;
}

void NetworkServiceResourceBlockList::UseMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl) {
  url_matcher_with_bypass_.Clear();
  for (const masked_domain_list::ResourceOwner& resource_owner :
       mdl.resource_owners()) {
    for (const masked_domain_list::Resource& resource :
         resource_owner.owned_resources()) {
      if (ResourceIsEligibleForBlockList(resource)) {
        url_matcher_with_bypass_.AddMaskedDomainListRules(resource.domain(),
                                                          resource_owner);
      }
    }
  }
  base::UmaHistogramMemoryKB(
      "NetworkService.MaskedDomainList.NetworkServiceResourceBlockList."
      "EstimatedMemoryUsageInKB",
      EstimateMemoryUsage() / 1024);
}

}  // namespace network

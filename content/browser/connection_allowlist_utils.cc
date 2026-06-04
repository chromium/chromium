// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/connection_allowlist_utils.h"

#include <optional>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"

namespace content {

bool ResponseContainsConnectionAllowlist(
    const network::mojom::URLResponseHead* response_head) {
  return response_head && response_head->headers &&
         response_head->parsed_headers &&
         (response_head->parsed_headers->connection_allowlists.enforced
              .has_value() ||
          response_head->parsed_headers->connection_allowlists.report_only
              .has_value());
}

bool ResponseEnablesConnectionAllowlistsOriginTrial(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers) {
  return base::FeatureList::IsEnabled(
             blink::features::kOverrideConnectionAllowlistOriginTrial) ||
         blink::TrialTokenValidator().RequestEnablesFeature(
             request_url, response_headers, "ConnectionAllowlist",
             base::Time::Now());
}

bool EnforcesConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies) {
  // The connection allowlist base feature is the kill switch for the feature.
  // It is checked first. Then connection allowlist also requires origin trial
  // enabled. In order to check the origin trial status, the initiator policy
  // container policies need to be retrieved.
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return false;
  }

  // The origin trial status is tied to the existence of allowlists in policy
  // container. If the initiator doesn't have an enforced allowlist in its
  // policies, it means either:
  // 1. the trial was not active for that context.
  // 2. or the parsed enforced allowlist is null. For example, the
  // "Connection-Allowlist" header has an empty field value.
  // The connection allowlist is not enforced in both cases.
  return initiator_policies.connection_allowlists.enforced.has_value();
}

bool IsRedirectAllowedByConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies) {
  // Note: redirect_behavior defaults to kBlock if not explicitly set in the
  // Connection-Allowlist header.
  if (initiator_policies.connection_allowlists.enforced->redirect_behavior ==
      network::ConnectionAllowlist::RedirectBehavior::kBlock) {
    // TODO(crbug.com/447954811): Implement reporting.
    return false;
  }

  return true;
}

bool ConnectionAllowlistAllowsUrlAndReportIfNeeded(
    const PolicyContainerPolicies& policies,
    const GURL& url) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists) ||
      !policies.connection_allowlists.enforced.has_value()) {
    return true;
  }
  if (network::ConnectionAllowlistMatchesUrl(
          policies.connection_allowlists.enforced.value(), url)) {
    return true;
  }
  // TODO(crbug.com/482728970): Implement reporting.
  return false;
}

network::ConnectionAllowlists GetConnectionAllowlistsForWorker(
    const GURL& response_url,
    const network::mojom::URLResponseHead* response_head,
    const PolicyContainerPolicies* creator_policies,
    bool inherit_from_creator) {
  if (!response_head ||
      !base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return network::ConnectionAllowlists();
  }

  // Local schemes inherit connection allowlists from their creator (e.g., the
  // frame that registered the worker).
  if (inherit_from_creator) {
    return creator_policies ? creator_policies->connection_allowlists
                            : network::ConnectionAllowlists();
  }

  // For non-local schemes, the connection allowlist must be provided in the
  // response headers and the origin trial must be enabled for the request
  // URL.
  if (ResponseContainsConnectionAllowlist(response_head) &&
      ResponseEnablesConnectionAllowlistsOriginTrial(
          response_url, response_head->headers.get())) {
    return response_head->parsed_headers->connection_allowlists;
  }

  return network::ConnectionAllowlists();
}

}  // namespace content

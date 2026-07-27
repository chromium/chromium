// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/connection_allowlist_utils.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/connection_allowlist_util.h"
#include "content/public/browser/render_frame_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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

bool EnforcesConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies) {
  // The connection allowlist base feature is the kill switch for the feature.
  // It is checked first.
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return false;
  }

  // If the initiator doesn't have an enforced allowlist in its
  // policies, it means the parsed enforced allowlist is null. For example, the
  // "Connection-Allowlist" header has an empty field value.
  // The connection allowlist is not enforced in this case.
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
  // response headers.
  if (ResponseContainsConnectionAllowlist(response_head)) {
    return response_head->parsed_headers->connection_allowlists;
  }

  return network::ConnectionAllowlists();
}

bool FrameConnectionAllowlistAllowsRequestAndReportIfNeeded(
    const RenderFrameHost* render_frame_host,
    const GURL& url,
    bool is_redirect) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return true;
  }

  if (!render_frame_host) {
    return true;
  }

  // The feature currently does not impact fenced frames.
  // TODO(crbug.com/447954811): Revisit this if the feature needs to be enabled
  // and fenced frames need to be supported.
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    return true;
  }

  const auto* rfh_impl =
      static_cast<const RenderFrameHostImpl*>(render_frame_host);
  if (!rfh_impl->HasPolicyContainerHost()) {
    return true;
  }

  const PolicyContainerPolicies& policies =
      rfh_impl->policy_container_host()->policies();
  if (!EnforcesConnectionAllowlist(policies)) {
    return true;
  }

  if (is_redirect) {
    // For redirects, the connection allowlist either allows or blocks the
    // redirect request based on its `redirects` directive. The request URL is
    // irrelevant to the decision.
    return IsRedirectAllowedByConnectionAllowlist(policies);
  }

  return ConnectionAllowlistAllowsUrlAndReportIfNeeded(policies, url);
}

}  // namespace content

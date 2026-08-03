// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/connection_allowlist_utils.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/connection_allowlist_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

void QueueConnectionAllowlistReport(
    network::mojom::NetworkContext* network_context,
    const GURL& url,
    const GURL& context_url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<base::UnguessableToken>& reporting_source,
    const std::string& group,
    bool enforced) {
  if (!network_context) {
    return;
  }

  base::DictValue body;
  body.Set("connection", url.GetAsReferrer().spec());
  body.Set("disposition", enforced ? "enforce" : "report");

  network_context->QueueReport("connection-allowlist", group, context_url,
                               reporting_source, network_anonymization_key,
                               std::move(body));
}

}  // namespace

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

bool HasActiveConnectionAllowlists(
    const PolicyContainerPolicies& initiator_policies) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return false;
  }
  return initiator_policies.connection_allowlists.enforced.has_value() ||
         initiator_policies.connection_allowlists.report_only.has_value();
}

bool IsRedirectAllowedByConnectionAllowlist(
    const PolicyContainerPolicies& initiator_policies,
    const GURL& original_url,
    network::mojom::NetworkContext* network_context,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<base::UnguessableToken>& reporting_source) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return true;
  }

  std::optional<base::UnguessableToken> resolved_reporting_source =
      reporting_source.has_value()
          ? reporting_source
          : initiator_policies.connection_allowlists.reporting_source;

  // 1. Check report-only redirect behavior
  if (initiator_policies.connection_allowlists.report_only.has_value() &&
      initiator_policies.connection_allowlists.report_only->redirect_behavior ==
          network::ConnectionAllowlist::RedirectBehavior::kBlock &&
      initiator_policies.connection_allowlists.report_only->reporting_endpoint
          .has_value()) {
    QueueConnectionAllowlistReport(
        network_context, original_url,
        initiator_policies.connection_allowlists.response_url,
        network_anonymization_key, resolved_reporting_source,
        *initiator_policies.connection_allowlists.report_only
             ->reporting_endpoint,
        /*enforced=*/false);
  }

  // 2. Check enforced behavior.
  if (initiator_policies.connection_allowlists.enforced.has_value() &&
      initiator_policies.connection_allowlists.enforced->redirect_behavior ==
          network::ConnectionAllowlist::RedirectBehavior::kBlock) {
    if (initiator_policies.connection_allowlists.enforced->reporting_endpoint
            .has_value()) {
      QueueConnectionAllowlistReport(
          network_context, original_url,
          initiator_policies.connection_allowlists.response_url,
          network_anonymization_key, resolved_reporting_source,
          initiator_policies.connection_allowlists.enforced->reporting_endpoint
              .value(),
          /*enforced=*/true);
    }
    return false;
  }

  return true;
}

bool ConnectionAllowlistAllowsUrlAndReportIfNeeded(
    const PolicyContainerPolicies& policies,
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<base::UnguessableToken>& reporting_source) {
  if (!base::FeatureList::IsEnabled(network::features::kConnectionAllowlists)) {
    return true;
  }

  std::optional<base::UnguessableToken> resolved_reporting_source =
      reporting_source.has_value()
          ? reporting_source
          : policies.connection_allowlists.reporting_source;

  // 1. Check report-only allowlist
  if (policies.connection_allowlists.report_only.has_value()) {
    if (!network::ConnectionAllowlistMatchesUrl(
            policies.connection_allowlists.report_only.value(), url)) {
      if (policies.connection_allowlists.report_only->reporting_endpoint
              .has_value()) {
        QueueConnectionAllowlistReport(
            network_context, url, policies.connection_allowlists.response_url,
            network_anonymization_key, resolved_reporting_source,
            *policies.connection_allowlists.report_only->reporting_endpoint,
            /*enforced=*/false);
      }
    }
  }

  // 2. Check enforced allowlist
  if (policies.connection_allowlists.enforced.has_value()) {
    if (network::ConnectionAllowlistMatchesUrl(
            policies.connection_allowlists.enforced.value(), url)) {
      return true;
    }
    if (policies.connection_allowlists.enforced->reporting_endpoint
            .has_value()) {
      QueueConnectionAllowlistReport(
          network_context, url, policies.connection_allowlists.response_url,
          network_anonymization_key, resolved_reporting_source,
          *policies.connection_allowlists.enforced->reporting_endpoint,
          /*enforced=*/true);
    }
    return false;
  }

  return true;
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
    RenderFrameHost* render_frame_host,
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
  if (!HasActiveConnectionAllowlists(policies)) {
    return true;
  }

  network::mojom::NetworkContext* network_context =
      render_frame_host->GetProcess()
          ->GetStoragePartition()
          ->GetNetworkContext();
  net::NetworkAnonymizationKey network_anonymization_key =
      render_frame_host->GetIsolationInfoForSubresources()
          .network_anonymization_key();
  std::optional<base::UnguessableToken> reporting_source =
      render_frame_host->GetReportingSource();

  if (is_redirect) {
    // For redirects, the connection allowlist either allows or blocks the
    // redirect request based on its `redirects` directive. The request URL is
    // irrelevant to the decision.
    return IsRedirectAllowedByConnectionAllowlist(
        policies, url, network_context, network_anonymization_key,
        reporting_source);
  }

  return ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, url, network_context, network_anonymization_key,
      reporting_source);
}

}  // namespace content

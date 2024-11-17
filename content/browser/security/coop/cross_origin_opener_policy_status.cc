// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/coop/cross_origin_opener_policy_status.h"

#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/security/coop/cross_origin_opener_policy_access_report_manager.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace {

void SanitizeCoopRestrictPropertiesHeadersIfDisabled(
    network::CrossOriginOpenerPolicy& coop,
    const GURL& url,
    net::HttpResponseHeaders* headers) {
  if (base::FeatureList::IsEnabled(
          network::features::kCoopRestrictProperties)) {
    return;
  }

  bool is_coop_restrict_properties_origin_trial_enabled =
      base::FeatureList::IsEnabled(
          network::features::kCoopRestrictPropertiesOriginTrial);
  if (is_coop_restrict_properties_origin_trial_enabled && headers &&
      blink::TrialTokenValidator().RequestEnablesFeature(
          url, headers, "CoopRestrictProperties", base::Time::Now())) {
    return;
  }

  if (coop.value ==
          network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties ||
      coop.value == network::mojom::CrossOriginOpenerPolicyValue::
                        kRestrictPropertiesPlusCoep) {
    coop.value = network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  }
  if (coop.report_only_value ==
          network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties ||
      coop.report_only_value == network::mojom::CrossOriginOpenerPolicyValue::
                                    kRestrictPropertiesPlusCoep) {
    coop.report_only_value =
        network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  }
  if (coop.soap_by_default_value ==
          network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties ||
      coop.soap_by_default_value ==
          network::mojom::CrossOriginOpenerPolicyValue::
              kRestrictPropertiesPlusCoep) {
    coop.soap_by_default_value =
        network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  }
}

}  // namespace

namespace content {

// This function returns whether the BrowsingInstance should change following
// COOP rules defined in:
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#changes-to-navigation
//
// The SiteInstance selection model heavily relies on this function to be
// correct, and documents these assumptions, many of which would crash the
// browser if incorrect. In particular, it assumes that returning a value of
// CoopSwapResult::kSwapRelated means that the current browsing context group
// will NOT be found suitable for reuse, as that would effectively mean no swap
// has happened.
CoopSwapResult ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
    network::mojom::CrossOriginOpenerPolicyValue initiator_coop,
    const url::Origin& initiator_origin,
    bool is_navigation_from_initial_empty_document,
    network::mojom::CrossOriginOpenerPolicyValue destination_coop,
    const url::Origin& destination_origin) {
  using network::mojom::CrossOriginOpenerPolicyValue;

  switch (initiator_coop) {
    case CrossOriginOpenerPolicyValue::kUnsafeNone:
      switch (destination_coop) {
        case CrossOriginOpenerPolicyValue::kUnsafeNone:
          return CoopSwapResult::kNoSwap;
        case CrossOriginOpenerPolicyValue::kRestrictProperties:
        case CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep:
          return CoopSwapResult::kSwapRelated;
        case CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
        case CrossOriginOpenerPolicyValue::kSameOrigin:
        case CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
        case CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
          return CoopSwapResult::kSwap;
      }

    case CrossOriginOpenerPolicyValue::kRestrictProperties:
    case CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep:
      switch (destination_coop) {
        case CrossOriginOpenerPolicyValue::kUnsafeNone:
          return CoopSwapResult::kSwapRelated;
        case CrossOriginOpenerPolicyValue::kRestrictProperties:
        case CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep:
          return (initiator_coop == destination_coop &&
                  initiator_origin.IsSameOriginWith(destination_origin))
                     ? CoopSwapResult::kNoSwap
                     : CoopSwapResult::kSwapRelated;
        case CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
        case CrossOriginOpenerPolicyValue::kSameOrigin:
        case CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
        case CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
          return CoopSwapResult::kSwap;
      }

    // "same-origin-allow-popups" is used to stay in the same BrowsingInstance
    // despite COOP mismatch during the first navigation the popup.
    case CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
      switch (destination_coop) {
        case CrossOriginOpenerPolicyValue::kUnsafeNone:
          return is_navigation_from_initial_empty_document
                     ? CoopSwapResult::kNoSwap
                     : CoopSwapResult::kSwap;
        case CrossOriginOpenerPolicyValue::kRestrictProperties:
        case CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep:
          return is_navigation_from_initial_empty_document
                     ? CoopSwapResult::kSwapRelated
                     : CoopSwapResult::kSwap;
        case CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
          return initiator_origin.IsSameOriginWith(destination_origin)
                     ? CoopSwapResult::kNoSwap
                     : CoopSwapResult::kSwap;
        case CrossOriginOpenerPolicyValue::kSameOrigin:
        case CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
        case CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
          return CoopSwapResult::kSwap;
      }

    case CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
      switch (destination_coop) {
        case CrossOriginOpenerPolicyValue::kUnsafeNone:
          return CoopSwapResult::kNoSwap;
        case CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
          return (is_navigation_from_initial_empty_document ||
                  !initiator_origin.IsSameOriginWith(destination_origin))
                     ? CoopSwapResult::kSwap
                     : CoopSwapResult::kNoSwap;
        case CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
        case CrossOriginOpenerPolicyValue::kRestrictProperties:
        case CrossOriginOpenerPolicyValue::kRestrictPropertiesPlusCoep:
        case CrossOriginOpenerPolicyValue::kSameOrigin:
        case CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
          return CoopSwapResult::kSwap;
      }

    // "same-origin" requires that both policies and origin match.
    case CrossOriginOpenerPolicyValue::kSameOrigin:
    case CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
      return (initiator_coop == destination_coop &&
              initiator_origin.IsSameOriginWith(destination_origin))
                 ? CoopSwapResult::kNoSwap
                 : CoopSwapResult::kSwap;
  }
}

CrossOriginOpenerPolicyStatus::CrossOriginOpenerPolicyStatus(
    NavigationRequest* navigation_request)
    : navigation_request_(navigation_request),
      frame_tree_node_(navigation_request->frame_tree_node()),
      previous_document_rph_(navigation_request->frame_tree_node()
                                 ->current_frame_host()
                                 ->GetProcess()),
      virtual_browsing_context_group_(frame_tree_node_->current_frame_host()
                                          ->virtual_browsing_context_group()),
      soap_by_default_virtual_browsing_context_group_(
          frame_tree_node_->current_frame_host()
              ->soap_by_default_virtual_browsing_context_group()),
      is_navigation_from_initial_empty_document_(
          frame_tree_node_->is_on_initial_empty_document()),
      current_coop_(
          frame_tree_node_->current_frame_host()->cross_origin_opener_policy()),
      current_origin_(
          frame_tree_node_->current_frame_host()->GetLastCommittedOrigin()),
      current_url_(
          frame_tree_node_->current_frame_host()->GetLastCommittedURL()),
      is_navigation_source_(
          navigation_request->common_params().initiator_origin.has_value() &&
          navigation_request->common_params()
              .initiator_origin->IsSameOriginWith(
                  frame_tree_node_->current_frame_host()
                      ->GetLastCommittedOrigin())) {
  // Use the URL of the opener for reporting purposes when doing an initial
  // navigation in a popup.
  // Note: the origin check is there to avoid leaking the URL of an opener that
  // navigated in the meantime.
  if (is_navigation_from_initial_empty_document_ &&
      frame_tree_node_->opener() &&
      frame_tree_node_->opener()
              ->current_frame_host()
              ->GetLastCommittedOrigin() == current_origin_) {
    current_url_ =
        frame_tree_node_->opener()->current_frame_host()->GetLastCommittedURL();
  }
  previous_document_rph_observation_.Observe(previous_document_rph_.get());
}

CrossOriginOpenerPolicyStatus::~CrossOriginOpenerPolicyStatus() {
  ClearTransientReportingSources();
}

void CrossOriginOpenerPolicyStatus::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  ClearTransientReportingSources();
}

void CrossOriginOpenerPolicyStatus::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  previous_document_rph_ = nullptr;
  previous_document_rph_observation_.Reset();
}

std::optional<network::mojom::BlockedByResponseReason>
CrossOriginOpenerPolicyStatus::SanitizeResponse(
    network::mojom::URLResponseHead* response) {
  const GURL& response_url = navigation_request_->common_params().url;
  SanitizeCoopHeaders(response_url, response);

  network::CrossOriginOpenerPolicy& coop =
      response->parsed_headers->cross_origin_opener_policy;

  // Popups with a sandboxing flag, inherited from their opener, are not
  // allowed to navigate to a document with a Cross-Origin-Opener-Policy that
  // is not "unsafe-none". This ensures a COOP document does not inherit any
  // property from an opener.
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
  if (coop.value != network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone &&
      (frame_tree_node_->pending_frame_policy().sandbox_flags !=
       network::mojom::WebSandboxFlags::kNone)) {
    // Blob and Filesystem documents' cross-origin-opener-policy values are
    // defaulted to the default unsafe-none.
    // Data documents can only be loaded on main documents through browser
    // initiated navigations. These never inherit sandbox flags.
    DCHECK(!response_url.SchemeIsBlob());
    DCHECK(!response_url.SchemeIsFileSystem());
    DCHECK(!response_url.SchemeIs(url::kDataScheme));

    // We should force a COOP browsing instance swap to avoid certain
    // opener+error pages exploits, see https://crbug.com/1256823 and
    // https://github.com/whatwg/html/issues/7345.
    browsing_instance_swap_result_ = CoopSwapResult::kSwap;
    virtual_browsing_context_group_ =
        CrossOriginOpenerPolicyAccessReportManager::
            GetNewVirtualBrowsingContextGroup();

    return network::mojom::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage;
  }

  return std::nullopt;
}

void CrossOriginOpenerPolicyStatus::EnforceCOOP(
    const network::CrossOriginOpenerPolicy& response_coop,
    const url::Origin& response_origin,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  // COOP only applies to top level browsing contexts. Embedded content
  // considered "top-level", that cannot always provide a separate process
  // (Fenced frames) should not be able to specify their own COOP header value.
  // Therefore we use IsOutermostMainFrame.
  if (!frame_tree_node_->IsOutermostMainFrame()) {
    return;
  }

  const GURL& response_url = navigation_request_->common_params().url;
  const GURL& response_referrer_url =
      navigation_request_->common_params().referrer->url;
  CHECK(response_coop.origin.has_value());
  const url::Origin& response_coop_origin = *response_coop.origin;

  StoragePartition* storage_partition = frame_tree_node_->current_frame_host()
                                            ->GetProcess()
                                            ->GetStoragePartition();
  base::UnguessableToken navigation_request_reporting_source =
      base::UnguessableToken::Create();
  // Compute isolation info needed for setting Reporting-Endpoints before
  // navigation commits.
  net::IsolationInfo isolation_info_for_subresources =
      frame_tree_node_->current_frame_host()
          ->ComputeIsolationInfoForSubresourcesForPendingCommit(
              response_origin, navigation_request_->is_credentialless(),
              navigation_request_->ComputeFencedFrameNonce());
  DCHECK(!isolation_info_for_subresources.IsEmpty());

  if (response_origin.IsSameOriginWith(response_coop_origin)) {
    // Set up endpoint if response contains Reporting-Endpoints header.
    SetReportingEndpoints(response_origin, storage_partition,
                          navigation_request_reporting_source,
                          isolation_info_for_subresources);
  }

  auto response_reporter = std::make_unique<CrossOriginOpenerPolicyReporter>(
      storage_partition, response_url, response_referrer_url, response_coop,
      navigation_request_reporting_source, network_anonymization_key);
  CrossOriginOpenerPolicyReporter* previous_reporter =
      use_current_document_coop_reporter_
          ? frame_tree_node_->current_frame_host()
                ->coop_access_report_manager()
                ->coop_reporter()
          : coop_reporter_.get();

  // current_coop_.origin could be nullopt for initial empty documents.
  const url::Origin current_coop_origin =
      current_coop_.origin.value_or(url::Origin());

  CoopSwapResult cross_origin_policy_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.value, current_coop_origin,
          is_navigation_from_initial_empty_document_, response_coop.value,
          response_coop_origin);

  // Over the whole redirect chain, keep track of the "strongest" way the new
  // context must be separated from the previous one.
  // kNoSwap < kSwapRelated < kSwap.
  if (cross_origin_policy_swap > browsing_instance_swap_result_) {
    browsing_instance_swap_result_ = cross_origin_policy_swap;
  }

  // Both report only cases (navigation from and to document) use the following
  // result, computing the need of a browsing context group swap based on both
  // documents' report-only values.
  CoopSwapResult report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.report_only_value, current_coop_origin,
          is_navigation_from_initial_empty_document_,
          response_coop.report_only_value, response_coop_origin);

  CoopSwapResult navigating_to_report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.value, current_coop_origin,
          is_navigation_from_initial_empty_document_,
          response_coop.report_only_value, response_coop_origin);

  CoopSwapResult navigating_from_report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.report_only_value, current_coop_origin,
          is_navigation_from_initial_empty_document_, response_coop.value,
          response_coop_origin);

  bool has_other_window_in_browsing_context_group =
      frame_tree_node_->current_frame_host()
          ->delegate()
          ->GetActiveTopLevelDocumentsInCoopRelatedGroup(
              frame_tree_node_->current_frame_host())
          .size() > 1;

  // If this response's COOP causes a BrowsingInstance swap in another
  // CoopRelatedGroup, report this to the previous COOP reporter and/or the COOP
  // reporter of the response if they exist. Do not report swaps in the same
  // CoopRelatedGroup, as these do not sever the opener.
  if (cross_origin_policy_swap == CoopSwapResult::kSwap) {
    // Using current_origin_ instead of current_coop_.origin here because
    // we only care about whether the actual origin has changed when determining
    // whether to show previous URL.
    if (has_other_window_in_browsing_context_group) {
      if (response_origin.IsSameOriginWith(response_coop_origin)) {
        response_reporter->QueueNavigationToCOOPReport(
            current_url_, current_origin_.IsSameOriginWith(response_origin),
            false /* is_report_only */);
      }

      if (previous_reporter &&
          current_origin_.IsSameOriginWith(current_coop_origin)) {
        previous_reporter->QueueNavigationAwayFromCOOPReport(
            response_url, is_navigation_source_,
            current_origin_.IsSameOriginWith(response_origin),
            false /* is_report_only */);
      }
    }
  }

  // Compare the report-only values against the real values on the opposite side
  // (from/to). If both report-only values match however, skip reports, as the
  // website likely wants to deploy COOP on both pages and is not interested in
  // these kind of reports.
  bool has_matching_report_only =
      report_only_coop_swap == CoopSwapResult::kNoSwap;
  CoopSwapResult virtual_browsing_instance_swap_result =
      CoopSwapResult::kNoSwap;
  if (!has_matching_report_only) {
    // Find the strongest type of swap that would happen, between the previous
    // and next report-only values.
    if (navigating_from_report_only_coop_swap == CoopSwapResult::kSwap ||
        navigating_to_report_only_coop_swap == CoopSwapResult::kSwap) {
      virtual_browsing_instance_swap_result = CoopSwapResult::kSwap;
    } else if (navigating_from_report_only_coop_swap ==
                   CoopSwapResult::kSwapRelated ||
               navigating_to_report_only_coop_swap ==
                   CoopSwapResult::kSwapRelated) {
      virtual_browsing_instance_swap_result = CoopSwapResult::kSwapRelated;
    }
  }

  // If this response's report-only COOP would cause a BrowsingInstance swap
  // in another CoopRelatedGroup, report this to the previous COOP reporter
  // and/or the COOP reporter of the response if they exist. Do not report swaps
  // in the same CoopRelatedGroup, as these do not sever the opener.
  if (virtual_browsing_instance_swap_result == CoopSwapResult::kSwap) {
    // If this response's report-only COOP would cause a BrowsingInstance swap,
    // report this to the previous COOP reporter and/or the COOP reporter of
    // the response if they exist.
    if (has_other_window_in_browsing_context_group) {
      if (response_origin.IsSameOriginWith(response_coop_origin)) {
        response_reporter->QueueNavigationToCOOPReport(
            current_url_, current_origin_.IsSameOriginWith(response_origin),
            true /* is_report_only */);
      }

      if (previous_reporter &&
          current_origin_.IsSameOriginWith(current_coop_origin)) {
        previous_reporter->QueueNavigationAwayFromCOOPReport(
            response_url, is_navigation_source_,
            current_origin_.IsSameOriginWith(response_origin),
            true /* is_report_only */);
      }
    }
  }

  if (browsing_instance_swap_result_ != CoopSwapResult::kNoSwap ||
      virtual_browsing_instance_swap_result != CoopSwapResult::kNoSwap) {
    virtual_browsing_context_group_ =
        CrossOriginOpenerPolicyAccessReportManager::
            GetVirtualBrowsingContextGroup(
                browsing_instance_swap_result_,
                virtual_browsing_instance_swap_result,
                virtual_browsing_context_group_);
  }

  // Check if a COOP of same-origin-allow-popups by default would result in a
  // browsing context group switch.
  if (ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.soap_by_default_value, current_coop_origin,
          is_navigation_from_initial_empty_document_,
          response_coop.soap_by_default_value,
          response_coop_origin) != CoopSwapResult::kNoSwap) {
    soap_by_default_virtual_browsing_context_group_ =
        CrossOriginOpenerPolicyAccessReportManager::
            GetNewVirtualBrowsingContextGroup();
  }

  // Finally, update the current COOP, origin and reporter to those of the
  // response, now that it has been taken into account.
  current_coop_ = response_coop;
  current_origin_ = response_origin;
  current_url_ = response_url;
  coop_reporter_ = std::move(response_reporter);

  // Once a response has been received, reports will be sent to the reporter of
  // the last response received.
  use_current_document_coop_reporter_ = false;

  // Any subsequent response means this response was a redirect, and the source
  // of the navigation to the subsequent response.
  is_navigation_source_ = true;
}

void CrossOriginOpenerPolicyStatus::SetReportingEndpoints(
    const url::Origin& response_origin,
    StoragePartition* storage_partition,
    const base::UnguessableToken& reporting_source,
    const net::IsolationInfo& isolation_info) {
  // Only process Reporting-Endpoints header for secure origin.
  if (!GURL::SchemeIsCryptographic(response_origin.scheme()))
    return;
  if (!navigation_request_->response())
    return;
  if (!navigation_request_->response()->parsed_headers->reporting_endpoints)
    return;
  // Process Reporting-Endpoints header immediately before the document is
  // loaded so COOP reports can be sent to response origin's configured
  // endpoint. The configured endpoints should only be used to send COOP reports
  // for this navigation and will be removed when the navigation finishes.
  base::flat_map<std::string, std::string>& reporting_endpoints =
      *(navigation_request_->response()->parsed_headers->reporting_endpoints);

  if (reporting_endpoints.empty())
    return;

  storage_partition->GetNetworkContext()->SetDocumentReportingEndpoints(
      reporting_source, response_origin, isolation_info, reporting_endpoints);
  // Record the new reporting source.
  transient_reporting_sources_.push_back(reporting_source);
}

void CrossOriginOpenerPolicyStatus::ClearTransientReportingSources() {
  if (!previous_document_rph_ || transient_reporting_sources_.empty())
    return;
  StoragePartition* storage_partition =
      previous_document_rph_->GetStoragePartition();
  for (const base::UnguessableToken& reporting_source :
       transient_reporting_sources_) {
    storage_partition->GetNetworkContext()->SendReportsAndRemoveSource(
        reporting_source);
  }
}

std::unique_ptr<CrossOriginOpenerPolicyReporter>
CrossOriginOpenerPolicyStatus::TakeCoopReporter() {
  return std::move(coop_reporter_);
}

void CrossOriginOpenerPolicyStatus::UpdateReporterStoragePartition(
    StoragePartition* storage_partition) {
  if (coop_reporter_)
    coop_reporter_->set_storage_partition(storage_partition);
}

// We blank out the COOP headers in a number of situations.
// - When the headers were not sent over HTTPS.
// - For subframes.
// - When the feature is disabled.
// We also strip the "reporting" parts when the reporting feature is disabled
// for the |response_url|.
void CrossOriginOpenerPolicyStatus::SanitizeCoopHeaders(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head) const {
  network::CrossOriginOpenerPolicy& coop =
      response_head->parsed_headers->cross_origin_opener_policy;

  network::AugmentCoopWithCoep(
      &coop, response_head->parsed_headers->cross_origin_embedder_policy);

  if (coop == network::CrossOriginOpenerPolicy())
    return;

  if (base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicy) &&
      // https://html.spec.whatwg.org/multipage#the-cross-origin-opener-policy-header
      // ```
      // 1. If reservedEnvironment is a non-secure context, then return
      //    "unsafe-none".
      // ```
      //
      // https://html.spec.whatwg.org/multipage/webappapis.html#secure-contexts
      // ```
      // 2. If the result of Is url potentially trustworthy? given environment's
      // top-level creation URL is "Potentially Trustworthy", then return true.
      // ```
      network::IsUrlPotentiallyTrustworthy(response_url) &&
      // The COOP header must be ignored outside of the top-level context. It is
      // removed as a defensive measure. Embedded content considered
      // "top-level", that cannot always provide a separate process (Fenced
      // frames, portals, etc.) should not be able to specify their own COOP
      // header value. Therefore we use IsOutermostMainFrame.
      frame_tree_node_->IsOutermostMainFrame()) {
    SanitizeCoopRestrictPropertiesHeadersIfDisabled(
        coop, response_url, response_head->headers.get());
    return;
  }

  bool has_coop_header =
      coop.value != network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone ||
      coop.report_only_value !=
          network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone ||
      coop.reporting_endpoint || coop.report_only_reporting_endpoint;

  coop = network::CrossOriginOpenerPolicy();

  if (!network::IsUrlPotentiallyTrustworthy(response_url) && has_coop_header) {
    navigation_request_->AddDeferredConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kError,
        "The Cross-Origin-Opener-Policy header has been ignored, because "
        "the URL's origin was untrustworthy. It was defined either in the "
        "final response or a redirect. Please deliver the response using "
        "the HTTPS protocol. You can also use the 'localhost' origin "
        "instead. See "
        "https://www.w3.org/TR/powerful-features/"
        "#potentially-trustworthy-origin and "
        "https://html.spec.whatwg.org/"
        "#the-cross-origin-opener-policy-header.");
  }
}

}  // namespace content

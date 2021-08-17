// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cross_origin_opener_policy_status.h"

#include <utility>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/cross_origin_embedder_policy.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

namespace {

// This function implements the COOP matching algorithm as detailed in [1].
// Note that COEP is also provided since the COOP enum does not have a
// "same-origin + COEP" value.
// [1] https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
bool CrossOriginOpenerPolicyMatch(
    network::mojom::CrossOriginOpenerPolicyValue initiator_coop,
    const url::Origin& initiator_origin,
    network::mojom::CrossOriginOpenerPolicyValue destination_coop,
    const url::Origin& destination_origin) {
  if (initiator_coop != destination_coop)
    return false;

  if (initiator_coop ==
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone) {
    return true;
  }

  if (!initiator_origin.IsSameOriginWith(destination_origin))
    return false;
  return true;
}

// This function returns whether the BrowsingInstance should change following
// COOP rules defined in:
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#changes-to-navigation
bool ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
    network::mojom::CrossOriginOpenerPolicyValue initiator_coop,
    const url::Origin& initiator_origin,
    bool is_initial_navigation,
    network::mojom::CrossOriginOpenerPolicyValue destination_coop,
    const url::Origin& destination_origin) {
  using network::mojom::CrossOriginOpenerPolicyValue;

  // If policies match there is no reason to switch BrowsingInstances.
  if (CrossOriginOpenerPolicyMatch(initiator_coop, initiator_origin,
                                   destination_coop, destination_origin)) {
    return false;
  }

  // "same-origin-allow-popups" is used to stay in the same BrowsingInstance
  // despite COOP mismatch. This case is defined in the spec [1] as follow.
  // ```
  // If the result of matching currentCOOP, currentOrigin, potentialCOOP, and
  // potentialOrigin is false and one of the following is false:
  //  - doc is the initial about:blank document
  //  - currentCOOP is "same-origin-allow-popups"
  //  - potentialCOOP is "unsafe-none"
  // Then create a new browsing context group.
  // ```
  // [1]
  // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#changes-to-navigation
  if (is_initial_navigation &&
      initiator_coop == CrossOriginOpenerPolicyValue::kSameOriginAllowPopups &&
      destination_coop == CrossOriginOpenerPolicyValue::kUnsafeNone) {
    return false;
  }
  return true;
}

}  // namespace

CrossOriginOpenerPolicyStatus::CrossOriginOpenerPolicyStatus(
    NavigationRequest* navigation_request)
    : navigation_request_(navigation_request),
      frame_tree_node_(navigation_request->frame_tree_node()),
      virtual_browsing_context_group_(frame_tree_node_->current_frame_host()
                                          ->virtual_browsing_context_group()),
      is_initial_navigation_(!frame_tree_node_->has_committed_real_load()),
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
  if (is_initial_navigation_ && frame_tree_node_->opener() &&
      frame_tree_node_->opener()
              ->current_frame_host()
              ->GetLastCommittedOrigin() == current_origin_) {
    current_url_ =
        frame_tree_node_->opener()->current_frame_host()->GetLastCommittedURL();
  }
}

CrossOriginOpenerPolicyStatus::~CrossOriginOpenerPolicyStatus() = default;

absl::optional<network::mojom::BlockedByResponseReason>
CrossOriginOpenerPolicyStatus::SanitizeResponse(
    network::mojom::URLResponseHead* response) const {
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
    return network::mojom::BlockedByResponseReason::
        kCoopSandboxedIFrameCannotNavigateToCoopPage;
  }

  return absl::nullopt;
}

void CrossOriginOpenerPolicyStatus::EnforceCOOP(
    const network::CrossOriginOpenerPolicy& response_coop,
    const url::Origin& response_origin,
    const net::NetworkIsolationKey& network_isolation_key) {
  // COOP only applies to top level browsing contexts.
  if (!frame_tree_node_->IsMainFrame())
    return;

  const GURL& response_url = navigation_request_->common_params().url;
  const GURL& response_referrer_url =
      navigation_request_->common_params().referrer->url;

  StoragePartition* storage_partition = frame_tree_node_->current_frame_host()
                                            ->GetProcess()
                                            ->GetStoragePartition();
  // TODO(crbug.com/1209057): This should not create a new reporting source
  // token. The navigation request should have a token which is used here, and
  // is either migrated to the document when it loads, or updated in the COOP
  // reporter to the document's source token instead.
  auto response_reporter = std::make_unique<CrossOriginOpenerPolicyReporter>(
      storage_partition, response_url, response_referrer_url, response_coop,
      base::UnguessableToken::Create(), network_isolation_key);
  CrossOriginOpenerPolicyReporter* previous_reporter =
      use_current_document_coop_reporter_
          ? frame_tree_node_->current_frame_host()->coop_reporter()
          : coop_reporter_.get();

  bool cross_origin_policy_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.value, current_origin_, is_initial_navigation_,
          response_coop.value, response_origin);

  // Both report only cases (navigation from and to document) use the following
  // result, computing the need of a browsing context group swap based on both
  // documents' report-only values.
  bool report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.report_only_value, current_origin_,
          is_initial_navigation_, response_coop.report_only_value,
          response_origin);

  bool navigating_to_report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.value, current_origin_, is_initial_navigation_,
          response_coop.report_only_value, response_origin);

  bool navigating_from_report_only_coop_swap =
      ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
          current_coop_.report_only_value, current_origin_,
          is_initial_navigation_, response_coop.value, response_origin);

  bool has_other_window_in_browsing_context_group =
      frame_tree_node_->current_frame_host()
          ->delegate()
          ->GetActiveTopLevelDocumentsInBrowsingContextGroup(
              frame_tree_node_->current_frame_host())
          .size() > 1;

  if (cross_origin_policy_swap) {
    require_browsing_instance_swap_ = true;

    // If this response's COOP causes a BrowsingInstance swap that severs
    // communication with another page, report this to the previous COOP
    // reporter and/or the COOP reporter of the response if they exist.
    if (has_other_window_in_browsing_context_group) {
      response_reporter->QueueNavigationToCOOPReport(
          current_url_, current_origin_.IsSameOriginWith(response_origin),
          false /* is_report_only */);

      if (previous_reporter) {
        previous_reporter->QueueNavigationAwayFromCOOPReport(
            response_url, is_navigation_source_,
            current_origin_.IsSameOriginWith(response_origin),
            false /* is_report_only */);
      }
    }
  }

  bool virtual_browsing_instance_swap =
      report_only_coop_swap && (navigating_to_report_only_coop_swap ||
                                navigating_from_report_only_coop_swap);
  if (virtual_browsing_instance_swap) {
    // If this response's report-only COOP would cause a BrowsingInstance swap
    // that would sever communication with another page, report this to the
    // previous COOP reporter and/or the COOP reporter of the response if they
    // exist.
    if (has_other_window_in_browsing_context_group) {
      response_reporter->QueueNavigationToCOOPReport(
          current_url_, current_origin_.IsSameOriginWith(response_origin),
          true /* is_report_only */);

      if (previous_reporter) {
        previous_reporter->QueueNavigationAwayFromCOOPReport(
            response_url, is_navigation_source_,
            current_origin_.IsSameOriginWith(response_origin),
            true /* is_report_only */);
      }
    }
  }

  if (require_browsing_instance_swap_ || virtual_browsing_instance_swap) {
    virtual_browsing_context_group_ =
        CrossOriginOpenerPolicyReporter::NextVirtualBrowsingContextGroup();
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
      &coop, CoepFromMainResponse(response_url, response_head));

  if (coop == network::CrossOriginOpenerPolicy())
    return;

  if (!base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicy) ||
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
      !network::IsUrlPotentiallyTrustworthy(response_url) ||
      // The COOP header must be ignored outside of the top-level context. It is
      // removed as a defensive measure.
      !frame_tree_node_->IsMainFrame()) {
    coop = network::CrossOriginOpenerPolicy();

    if (!network::IsUrlPotentiallyTrustworthy(response_url)) {
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
    return;
  }

  // The reporting part can be enabled via either a command-line flag or an
  // origin trial.
  bool reporting_enabled = base::FeatureList::IsEnabled(
      network::features::kCrossOriginOpenerPolicyReporting);

  reporting_enabled |=
      base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicyReportingOriginTrial) &&
      blink::TrialTokenValidator().RequestEnablesFeature(
          response_url, response_head->headers.get(),
          "CrossOriginOpenerPolicyReporting", base::Time::Now());

  if (!reporting_enabled) {
    coop.reporting_endpoint = absl::nullopt;
    coop.report_only_reporting_endpoint = absl::nullopt;
    coop.report_only_value =
        network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone;
  }
}

}  // namespace content

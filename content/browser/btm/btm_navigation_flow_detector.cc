// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_navigation_flow_detector.h"

#include "base/check.h"
#include "base/rand_util.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {
enum class QuantityBucket {
  kZero = 0,
  kOne,
  kMultiple,
};

// Types that qualify a navigation for the
// DIPS.TrustIndicator.DirectNavigationV2 UKM event. Should only contain core
// page transition types (no qualifiers).
constexpr const std::array<ui::PageTransition, 2>&
    kDirectNavigationPageTransitions{
        ui::PAGE_TRANSITION_TYPED,
        ui::PAGE_TRANSITION_AUTO_BOOKMARK,
    };

bool IsPageTransitionDirectNavigation(ui::PageTransition page_transition) {
  for (auto& direct_navigation_type : kDirectNavigationPageTransitions) {
    if (ui::PageTransitionCoreTypeIs(page_transition, direct_navigation_type)) {
      return true;
    }
  }
  return false;
}

btm::DirectNavigationSource ToDirectNavigationSource(
    ui::PageTransition page_transition) {
  if (ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_TYPED)) {
    return btm::DirectNavigationSource::kOmnibar;
  }
  if (ui::PageTransitionCoreTypeIs(page_transition,
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return btm::DirectNavigationSource::kBookmark;
  }
  return btm::DirectNavigationSource::kUnknown;
}

// Looks for a redirect to the current page that qualifies as a server-redirect
// exit from a suspected tracker flow (i.e., a single-hop server-side redirect)
// and returns it, if one exists. Returns nullptr otherwise.
const BtmServerRedirectInfo* GetEntrypointExitServerRedirect(
    const BtmNavigationInfo& navigation) {
  return navigation.server_redirects.size() == 1
             ? &navigation.server_redirects.front()
             : nullptr;
}

// Matches true client redirects (HTML `<meta>` tag redirects, JavaScript
// `window.location.replace` redirects), as well as navigations initiated by
// page content without a user gesture.
bool WasClientRedirectLike(const BtmNavigationInfo& navigation) {
  return navigation.was_renderer_initiated && !navigation.was_user_initiated;
}

QuantityBucket GetCrossSiteRedirectQuantity(
    const std::string& initial_site,
    const BtmNavigationInfo& navigation) {
  std::string referring_site = initial_site;
  size_t num_cross_site_redirects = 0;

  for (const auto& server_redirect : navigation.server_redirects) {
    std::string redirector_site = GetSiteForBtm(server_redirect.url);
    if (redirector_site != referring_site) {
      num_cross_site_redirects += 1;
      if (num_cross_site_redirects >= 2) {
        return QuantityBucket::kMultiple;
      }
      referring_site.swap(redirector_site);
    }
  }

  const std::string destination_site =
      GetSiteForBtm(navigation.destination.url);
  if (destination_site != referring_site) {
    num_cross_site_redirects += 1;
  }

  switch (num_cross_site_redirects) {
    case 0:
      return QuantityBucket::kZero;
    case 1:
      return QuantityBucket::kOne;
    default:
      return QuantityBucket::kMultiple;
  }
}

void EmitSuspectedTrackerFlowUkm(ukm::SourceId referrer_source_id,
                                 ukm::SourceId entrypoint_source_id,
                                 bool did_entrypoint_access_storage,
                                 int32_t flow_id,
                                 BtmRedirectType exit_redirect_type) {
  ukm::builders::DIPS_SuspectedTrackerFlowReferrerV2(referrer_source_id)
      .SetFlowId(flow_id)
      .Record(ukm::UkmRecorder::Get());

  ukm::builders::DIPS_SuspectedTrackerFlowEntrypointV2(entrypoint_source_id)
      .SetExitRedirectType(static_cast<int64_t>(exit_redirect_type))
      .SetHadActiveStorageAccess(did_entrypoint_access_storage)
      .SetFlowId(flow_id)
      .Record(ukm::UkmRecorder::Get());
}

void MaybeEmitDirectNavigationUkm(const BtmNavigationInfo& navigation) {
  if (!IsPageTransitionDirectNavigation(navigation.page_transition)) {
    return;
  }

  ukm::SourceId source_id = navigation.server_redirects.empty()
                                ? navigation.destination.source_id
                                : navigation.server_redirects.front().source_id;

  ukm::builders::DIPS_TrustIndicator_DirectNavigationV2(source_id)
      .SetNavigationSource(static_cast<int64_t>(
          ToDirectNavigationSource(navigation.page_transition)))
      .Record(ukm::UkmRecorder::Get());
}
}  // namespace

namespace btm {

EntrypointInfo::EntrypointInfo(
    const BtmServerRedirectInfo& server_redirect_info,
    bool was_referral_client_redirect_like)
    : site(GetSiteForBtm(server_redirect_info.url)),
      source_id(server_redirect_info.source_id),
      had_active_storage_access(server_redirect_info.did_write_cookies),
      was_referral_client_redirect(was_referral_client_redirect_like) {}

EntrypointInfo::EntrypointInfo(const BtmNavigationInfo& referral)
    : site(GetSiteForBtm(referral.destination.url)),
      source_id(referral.destination.source_id),
      had_active_storage_access(false),
      was_referral_client_redirect(WasClientRedirectLike(referral)) {}

EntrypointInfo::EntrypointInfo(const BtmNavigationInfo& referral,
                               const BtmPageVisitInfo& entrypoint_visit)
    : site(GetSiteForBtm(entrypoint_visit.url)),
      source_id(entrypoint_visit.source_id),
      had_active_storage_access(entrypoint_visit.had_active_storage_access),
      was_referral_client_redirect(WasClientRedirectLike(referral)) {}

InFlowSuccessorInteractionState::InFlowSuccessorInteractionState(
    btm::EntrypointInfo flow_entrypoint)
    : flow_entrypoint_(flow_entrypoint) {}

InFlowSuccessorInteractionState::~InFlowSuccessorInteractionState() = default;

void InFlowSuccessorInteractionState::RecordActiveStorageAccessByEntrypoint() {
  flow_entrypoint_.had_active_storage_access = true;
}

void InFlowSuccessorInteractionState::IncrementFlowIndex(size_t increment) {
  flow_index_ += increment;
}

void InFlowSuccessorInteractionState::
    RecordSuccessorInteractionAtCurrentFlowIndex() {
  bool has_existing_record_for_current_index =
      !successor_interaction_indices_.empty() &&
      successor_interaction_indices_.back() == flow_index_;
  if (!has_existing_record_for_current_index) {
    successor_interaction_indices_.push_back(flow_index_);
  }
}

bool InFlowSuccessorInteractionState::IsAtSuccessor() const {
  return flow_index_ > 0;
}

}  // namespace btm

BtmNavigationFlowDetector::BtmNavigationFlowDetector(WebContents* web_contents)
    : WebContentsUserData<BtmNavigationFlowDetector>(*web_contents),
      page_visit_observer_(
          web_contents,
          base::BindRepeating(
              &BtmNavigationFlowDetector::OnPageVisitReported,
              // `base::Unretained()` is safe here because this class outlives
              // `page_visit_observer_`, so `page_visit_observer_` won't run the
              // callback after this class is destroyed.
              base::Unretained(this))) {}

BtmNavigationFlowDetector::~BtmNavigationFlowDetector() = default;

void BtmNavigationFlowDetector::OnPageVisitReported(
    BtmPageVisitInfo page_visit,
    BtmNavigationInfo navigation) {
  CHECK(!previous_page_to_current_page_.has_value() ||
        previous_page_to_current_page_->destination.url == page_visit.url);

  // Slide our sliding window by one report (page visit + navigation).
  two_pages_ago_ = std::move(previous_page_);
  two_pages_ago_to_previous_page_ = std::move(previous_page_to_current_page_);
  previous_page_ = std::move(page_visit);
  previous_page_to_current_page_ = std::move(navigation);

  // Update IFSI tracking state based on the visit. To have all the information
  // we need, we have to do this after the visit is reported but before we
  // modify the IFSI flow state.
  //
  // Make sure in-visit storage accesses are propagated to IFSI tracking state
  // entrypoints.
  bool was_visit_for_successor_flow_entrypoint =
      flow_status_ == btm::FlowStatus::kOngoing &&
      successor_interaction_tracking_state_.has_value() &&
      !successor_interaction_tracking_state_->IsAtSuccessor();
  if (was_visit_for_successor_flow_entrypoint &&
      previous_page_->had_active_storage_access) {
    successor_interaction_tracking_state_
        ->RecordActiveStorageAccessByEntrypoint();
  }
  // Record any in-flow successor interactions.
  if (previous_page_->received_user_activation &&
      successor_interaction_tracking_state_.has_value() &&
      successor_interaction_tracking_state_->IsAtSuccessor()) {
    successor_interaction_tracking_state_
        ->RecordSuccessorInteractionAtCurrentFlowIndex();
  }

  // Update in-flow successor interaction tracking state based on the flow
  // status after this report, and maybe emit InFlowSuccessorInteraction UKM.
  bool did_start_new_flow = MaybeInitializeSuccessorInteractionTrackingState();
  flow_status_ = FlowStatusAfterNavigation(did_start_new_flow);
  if (flow_status_ == btm::FlowStatus::kOngoing && !did_start_new_flow) {
    successor_interaction_tracking_state_->IncrementFlowIndex(
        previous_page_to_current_page_->server_redirects.size() + 1);
  }
  if (flow_status_ == btm::FlowStatus::kEnded) {
    MaybeEmitInFlowSuccessorInteraction();
  }
  if (flow_status_ != btm::FlowStatus::kOngoing) {
    successor_interaction_tracking_state_.reset();
  }

  MaybeEmitDirectNavigationUkm(previous_page_to_current_page_.value());
  MaybeEmitNavFlowNodeUkmForPreviousPage();

  int32_t flow_id = static_cast<int32_t>(base::RandUint64());
  const BtmServerRedirectInfo* server_redirect_entrypoint_exit =
      GetEntrypointExitServerRedirect(previous_page_to_current_page_.value());
  if (server_redirect_entrypoint_exit != nullptr) {
    MaybeEmitSuspectedTrackerFlowUkmForServerRedirectExit(
        *server_redirect_entrypoint_exit, flow_id);
  } else {
    MaybeEmitSuspectedTrackerFlowUkmForClientRedirectExit(flow_id);
    MaybeEmitInFlowInteraction(flow_id);
  }
}

void BtmNavigationFlowDetector::MaybeEmitNavFlowNodeUkmForPreviousPage() {
  if (!CanEmitNavFlowNodeUkmForPreviousPage()) {
    return;
  }

  ukm::builders::DIPS_NavigationFlowNode(previous_page_->source_id)
      .SetWerePreviousAndNextSiteSame(GetSiteForBtm(two_pages_ago_->url) ==
                                      GetSiteForCurrentPage())
      .SetDidHaveUserActivation(previous_page_->received_user_activation)
      .SetDidHaveSuccessfulWAA(
          previous_page_->had_successful_web_authn_assertion)
      .SetWereEntryAndExitRendererInitiated(
          two_pages_ago_to_previous_page_->was_renderer_initiated &&
          previous_page_to_current_page_->was_renderer_initiated)
      .SetWasEntryUserInitiated(
          two_pages_ago_to_previous_page_->was_user_initiated)
      .SetWasExitUserInitiated(
          previous_page_to_current_page_->was_user_initiated)
      .SetVisitDurationMilliseconds(ukm::GetExponentialBucketMinForUserTiming(
          previous_page_->visit_duration.InMilliseconds()))
      .Record(ukm::UkmRecorder::Get());
}

bool BtmNavigationFlowDetector::CanEmitNavFlowNodeUkmForPreviousPage() const {
  bool page_is_in_series_of_three =
      two_pages_ago_.has_value() && !two_pages_ago_->url.is_empty();
  if (!page_is_in_series_of_three) {
    return false;
  }

  CHECK(previous_page_.has_value() &&
        previous_page_to_current_page_.has_value());

  bool page_has_valid_source_id =
      previous_page_->source_id != ukm::kInvalidSourceId;
  bool is_site_different_from_prior_page =
      GetSiteForBtm(previous_page_->url) != GetSiteForBtm(two_pages_ago_->url);
  bool is_site_different_from_next_page =
      GetSiteForBtm(previous_page_->url) != GetSiteForCurrentPage();

  return page_has_valid_source_id &&
         previous_page_->had_active_storage_access &&
         is_site_different_from_prior_page && is_site_different_from_next_page;
}

void BtmNavigationFlowDetector::
    MaybeEmitSuspectedTrackerFlowUkmForServerRedirectExit(
        const BtmServerRedirectInfo& exit_info,
        int32_t flow_id) {
  if (!CanEmitSuspectedTrackerFlowUkmForServerRedirectExit(exit_info)) {
    return;
  }

  EmitSuspectedTrackerFlowUkm(previous_page_->source_id, exit_info.source_id,
                              exit_info.did_write_cookies, flow_id,
                              BtmRedirectType::kServer);
}

bool BtmNavigationFlowDetector::
    CanEmitSuspectedTrackerFlowUkmForServerRedirectExit(
        const BtmServerRedirectInfo& exit_info) const {
  if (!previous_page_.has_value() ||
      !previous_page_to_current_page_.has_value()) {
    return false;
  }

  btm::EntrypointInfo entrypoint_info_for_server_redirect_exit(
      exit_info, WasClientRedirectLike(*previous_page_to_current_page_));
  return CanEmitSuspectedTrackerFlowUkm(
      *previous_page_, entrypoint_info_for_server_redirect_exit,
      GetSiteForCurrentPage());
}

void BtmNavigationFlowDetector::
    MaybeEmitSuspectedTrackerFlowUkmForClientRedirectExit(int32_t flow_id) {
  if (!CanEmitSuspectedTrackerFlowUkmForClientRedirectExit()) {
    return;
  }

  EmitSuspectedTrackerFlowUkm(two_pages_ago_->source_id,
                              previous_page_->source_id,
                              previous_page_->had_active_storage_access,
                              flow_id, BtmRedirectType::kClient);
}

bool BtmNavigationFlowDetector::
    CanEmitSuspectedTrackerFlowUkmForClientRedirectExit() const {
  bool page_is_in_series_of_three =
      two_pages_ago_.has_value() && !two_pages_ago_->url.is_empty();
  if (!page_is_in_series_of_three) {
    return false;
  }

  CHECK(previous_page_.has_value() &&
        previous_page_to_current_page_.has_value());

  if (!WasClientRedirectLike(*previous_page_to_current_page_)) {
    return false;
  }

  btm::EntrypointInfo entrypoint_info(*two_pages_ago_to_previous_page_,
                                      *previous_page_);
  return CanEmitSuspectedTrackerFlowUkm(two_pages_ago_.value(), entrypoint_info,
                                        GetSiteForCurrentPage());
}

bool BtmNavigationFlowDetector::CanEmitSuspectedTrackerFlowUkm(
    const BtmPageVisitInfo& referrer_page_info,
    const btm::EntrypointInfo& entrypoint_info,
    const std::string& exit_site) const {
  bool referrer_has_valid_source_id =
      referrer_page_info.source_id != ukm::kInvalidSourceId;
  bool entrypoint_has_valid_source_id =
      entrypoint_info.source_id != ukm::kInvalidSourceId;
  bool is_entrypoint_site_different_from_referrer =
      entrypoint_info.site != GetSiteForBtm(referrer_page_info.url);
  bool is_entrypoint_site_different_from_exit_page =
      entrypoint_info.site != exit_site;

  return referrer_has_valid_source_id && entrypoint_has_valid_source_id &&
         is_entrypoint_site_different_from_referrer &&
         is_entrypoint_site_different_from_exit_page &&
         entrypoint_info.was_referral_client_redirect;
}

void BtmNavigationFlowDetector::MaybeEmitInFlowInteraction(int32_t flow_id) {
  if (!CanEmitSuspectedTrackerFlowUkmForClientRedirectExit() ||
      !two_pages_ago_to_previous_page_->server_redirects.empty() ||
      !previous_page_->received_user_activation) {
    return;
  }

  ukm::builders::DIPS_TrustIndicator_InFlowInteractionV2(
      previous_page_->source_id)
      .SetFlowId(flow_id)
      .Record(ukm::UkmRecorder::Get());
}

void BtmNavigationFlowDetector::MaybeEmitInFlowSuccessorInteraction() {
  if (!successor_interaction_tracking_state_.has_value() ||
      successor_interaction_tracking_state_->successor_interaction_indices()
          .empty()) {
    return;
  }

  for (size_t index :
       successor_interaction_tracking_state_->successor_interaction_indices()) {
    ukm::builders::DIPS_TrustIndicator_InFlowSuccessorInteraction(
        successor_interaction_tracking_state_->flow_entrypoint().source_id)
        .SetSuccessorRedirectIndex(index)
        .SetDidEntrypointAccessStorage(
            successor_interaction_tracking_state_->flow_entrypoint()
                .had_active_storage_access)
        .Record(ukm::UkmRecorder::Get());
  }
}

btm::FlowStatus BtmNavigationFlowDetector::FlowStatusAfterNavigation(
    bool did_most_recent_navigation_start_new_flow) const {
  if (!WasClientRedirectLike(*previous_page_to_current_page_)) {
    return btm::FlowStatus::kInvalidated;
  }
  if (!successor_interaction_tracking_state_.has_value()) {
    return btm::FlowStatus::kInvalidated;
  }

  if (did_most_recent_navigation_start_new_flow) {
    bool is_still_on_entrypoint =
        previous_page_to_current_page_->server_redirects.empty();
    if (is_still_on_entrypoint) {
      return btm::FlowStatus::kOngoing;
    }

    bool are_entrypoint_and_current_page_same_site =
        successor_interaction_tracking_state_->flow_entrypoint().site ==
        GetSiteForCurrentPage();
    return are_entrypoint_and_current_page_same_site ? btm::FlowStatus::kOngoing
                                                     : btm::FlowStatus::kEnded;
  }

  QuantityBucket cross_site_redirect_quantity_bucket =
      GetCrossSiteRedirectQuantity(GetSiteForBtm(previous_page_->url),
                                   *previous_page_to_current_page_);
  switch (cross_site_redirect_quantity_bucket) {
    case QuantityBucket::kZero:
      return btm::FlowStatus::kOngoing;
    case QuantityBucket::kOne:
      return btm::FlowStatus::kEnded;
    case QuantityBucket::kMultiple:
      return btm::FlowStatus::kInvalidated;
  }
}

bool BtmNavigationFlowDetector::
    MaybeInitializeSuccessorInteractionTrackingState() {
  if (flow_status_ == btm::FlowStatus::kOngoing) {
    return false;
  }
  if (!previous_page_ || !previous_page_to_current_page_) {
    return false;
  }
  if (!WasClientRedirectLike(*previous_page_to_current_page_)) {
    return false;
  }

  // Look for an entrypoint, which must either be the current page or the first
  // server redirect since the prior page.

  const std::vector<BtmServerRedirectInfo>& server_redirects =
      previous_page_to_current_page_->server_redirects;
  bool can_entrypoint_be_current_page = server_redirects.empty();
  const std::string site_for_previous_page = GetSiteForBtm(previous_page_->url);

  if (can_entrypoint_be_current_page) {
    if (site_for_previous_page != GetSiteForCurrentPage()) {
      successor_interaction_tracking_state_.emplace(
          btm::EntrypointInfo(*previous_page_to_current_page_));
      return true;
    }
    return false;
  }

  const BtmServerRedirectInfo& possible_entrypoint = server_redirects.front();
  if (GetSiteForBtm(possible_entrypoint.url) == site_for_previous_page) {
    return false;
  }
  bool had_cross_site_redirect_after_entrypoint =
      GetCrossSiteRedirectQuantity(site_for_previous_page,
                                   *previous_page_to_current_page_) ==
      QuantityBucket::kMultiple;
  if (had_cross_site_redirect_after_entrypoint) {
    return false;
  }

  successor_interaction_tracking_state_.emplace(btm::EntrypointInfo(
      possible_entrypoint,
      WasClientRedirectLike(*previous_page_to_current_page_)));
  successor_interaction_tracking_state_->IncrementFlowIndex(
      server_redirects.size());
  return true;
}

const std::string BtmNavigationFlowDetector::GetSiteForCurrentPage() const {
  return GetSiteForBtm(previous_page_to_current_page_->destination.url);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BtmNavigationFlowDetector);

}  // namespace content

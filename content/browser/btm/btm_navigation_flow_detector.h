// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_NAVIGATION_FLOW_DETECTOR_H_
#define CONTENT_BROWSER_BTM_BTM_NAVIGATION_FLOW_DETECTOR_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "content/browser/btm/btm_page_visit_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {

namespace btm {

// Should match DIPSDirectNavigationSource in tools/metrics/histograms/enums.xml
enum class DirectNavigationSource {
  kUnknown = 0,
  kOmnibar = 1,
  kBookmark = 2,
};

struct EntrypointInfo {
  // Used when the entrypoint has a server redirect exit.
  explicit EntrypointInfo(const BtmServerRedirectInfo& server_redirect_info,
                          bool was_referral_client_redirect_like);
  // Used when the entrypoint has a client redirect(-like) exit, when the page
  // visit has already been reported.
  explicit EntrypointInfo(const BtmNavigationInfo& referral,
                          const BtmPageVisitInfo& entrypoint_visit);
  // Used when the entrypoint has a client redirect(-like) exit, when the
  // EntrypointInfo needs to be created before the page visit is reported.
  explicit EntrypointInfo(const BtmNavigationInfo& referral);

  const std::string site;
  ukm::SourceId source_id;
  bool had_active_storage_access;
  bool was_referral_client_redirect;
};

enum class FlowStatus {
  kInvalidated = 0,
  kOngoing,
  kEnded,
};

class InFlowSuccessorInteractionState {
 public:
  explicit InFlowSuccessorInteractionState(btm::EntrypointInfo flow_entrypoint);
  ~InFlowSuccessorInteractionState();

  void RecordActiveStorageAccessByEntrypoint();
  void IncrementFlowIndex(size_t increment);
  void RecordSuccessorInteractionAtCurrentFlowIndex();
  bool IsAtSuccessor() const;

  const btm::EntrypointInfo& flow_entrypoint() const {
    return flow_entrypoint_;
  }
  size_t flow_index() const { return flow_index_; }
  const std::vector<size_t>& successor_interaction_indices() const {
    return successor_interaction_indices_;
  }

 private:
  btm::EntrypointInfo flow_entrypoint_;
  size_t flow_index_ = 0;
  std::vector<size_t> successor_interaction_indices_;
};

}  // namespace btm

// Detects possible navigation flows with the aim of discovering how to
// distinguish user-interest navigation flows from navigational tracking.
//
// For most events a navigation flow consists of three consecutive navigations
// in a tab (A->B->C). Some events might be recorded for flows with more than
// three navigations e.g. InFlowSuccessorInteraction where there is 4 or more
// navigations.
//
// Currently only reports UKM to inform how we might identify possible
// navigational tracking by sites that also perform user-interest activity.
class CONTENT_EXPORT BtmNavigationFlowDetector
    : public WebContentsUserData<BtmNavigationFlowDetector> {
 public:
  ~BtmNavigationFlowDetector() override;

  void SetClockForTesting(base::Clock* clock) {
    page_visit_observer_.SetClockForTesting(clock);
  }

 protected:
  explicit BtmNavigationFlowDetector(WebContents* web_contents);

  // Records an event describing the characteristics of a navigation flow.
  void MaybeEmitNavFlowNodeUkmForPreviousPage();
  bool CanEmitNavFlowNodeUkmForPreviousPage() const;

  // Records events for flows we suspect include a tracker and have a server
  // redirect.
  void MaybeEmitSuspectedTrackerFlowUkmForServerRedirectExit(
      const BtmServerRedirectInfo& exit_info,
      int32_t flow_id);
  bool CanEmitSuspectedTrackerFlowUkmForServerRedirectExit(
      const BtmServerRedirectInfo& exit_info) const;

  // Records events for flows we suspect include a tracker and have a client
  // redirect.
  void MaybeEmitSuspectedTrackerFlowUkmForClientRedirectExit(int32_t flow_id);
  bool CanEmitSuspectedTrackerFlowUkmForClientRedirectExit() const;

  bool CanEmitSuspectedTrackerFlowUkm(
      const BtmPageVisitInfo& referrer_page_info,
      const btm::EntrypointInfo& entrypoint_info,
      const std::string& exit_site) const;

  // Records an event for flows where there was a user interaction in between,
  // i.e. for flow A->B->C, there was a user interaction on B. This could be
  // used as a signal that B is not a tracker.
  void MaybeEmitInFlowInteraction(int32_t flow_id);

  // Records events for flows where there's a series of same-site redirects,
  // followed by a page with a user interaction (what we consider the
  // "successor"), followed by another series of same-site redirects that end
  // in a cross-site redirect. For example, we would record this event for
  // A->B1->B2->B3->C, where B2 had a user interaction. This pattern is commonly
  // used in auth flows and could be used as a signal that B1 is not a tracker.
  void MaybeEmitInFlowSuccessorInteraction();

 private:
  // So WebContentsUserData::CreateForWebContents can call the constructor.
  friend class WebContentsUserData<BtmNavigationFlowDetector>;

  // Callback to be called by `BtmPageVisitObserver`.
  void OnPageVisitReported(BtmPageVisitInfo page_visit,
                           BtmNavigationInfo navigation);

  btm::FlowStatus FlowStatusAfterNavigation(
      bool did_most_recent_navigation_start_new_flow) const;
  // Returns whether the entrypoint was set or not.
  bool MaybeInitializeSuccessorInteractionTrackingState();

  // Must be called only when `previous_page_to_current_page_` is populated.
  const std::string GetSiteForCurrentPage() const;

  // Navigation Flow:
  // A navigation flow consists of three navigations in a tab (A->B->C).
  // The infos below are updated when the primary page changes.
  //
  // Note that server redirects don't commit, so if there's a server redirect
  // from B->C, B is not committed and not reported as a page visit, but instead
  // in the `server_redirects` field of the corresponding `BtmNavigationInfo`.
  // In this case, `previous_page_` corresponds to A,
  // `previous_page_to_current_page_->server_redirects` will contain B, and
  // `previous_page_to_current_page_->destination` will have some limited
  // information about C.

  // In a series of three committed pages A->B->C, contains information about
  // the visit on A.
  std::optional<BtmPageVisitInfo> two_pages_ago_;
  // In a series of three committed pages A->B->C, contains information about
  // the navigation A->B.
  std::optional<BtmNavigationInfo> two_pages_ago_to_previous_page_;
  // In a series of three committed pages A->B->C, contains information about
  // the visit on B.
  std::optional<BtmPageVisitInfo> previous_page_;
  // In a series of three committed pages A->B->C, contains information about
  // the navigation B->C.
  std::optional<BtmNavigationInfo> previous_page_to_current_page_;

  // The status of a flow for the purposes of InFlowSuccessorInteraction, after
  // the most recent primary page change.
  btm::FlowStatus flow_status_ = btm::FlowStatus::kInvalidated;
  // Data needed for emitting DIPS.TrustIndicator.InFlowSuccessorInteraction.
  // Set only when there's an ongoing flow that's possibly valid (we can't know
  // for sure until it ends or is invalidated).
  std::optional<btm::InFlowSuccessorInteractionState>
      successor_interaction_tracking_state_;

  BtmPageVisitObserver page_visit_observer_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_NAVIGATION_FLOW_DETECTOR_H_

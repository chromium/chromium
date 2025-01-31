// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_short_visit_observer.h"

#include <cstdint>

#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

// State associated with navigations that BtmShortVisitObserver needs to be able
// to discard if a navigation doesn't commit.
class NavigationState : public NavigationHandleUserData<NavigationState> {
 public:
  NavigationState(NavigationHandle& navigation_handle, base::Time started_at)
      : started_at_(started_at) {}

  base::Time started_at() const { return started_at_; }

 private:
  const base::Time started_at_;

  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationState);

// Returns the eTLD+1 of `url` (or the whole hostname if no eTLD is detected).
std::string GetSite(const GURL& url) {
  const std::string site =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return site.empty() ? url.host() : site;
}

}  // namespace

BtmShortVisitObserver::BtmShortVisitObserver(WebContents* web_contents,
                                             const base::Clock* clock)
    : WebContentsObserver(web_contents),
      clock_(clock ? *clock : *base::DefaultClock::GetInstance()),
      last_committed_at_(clock_->Now()),
      page_source_id_(
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()) {}

BtmShortVisitObserver::~BtmShortVisitObserver() = default;

void BtmShortVisitObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    // Ignore irrelevant navigations.
    return;
  }

  // Record the start time of the navigation.
  NavigationState::CreateForNavigationHandle(*navigation_handle, clock_->Now());
}

void BtmShortVisitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    // Ignore irrelevant navigations.
    return;
  }

  const GURL& visit_url = navigation_handle->GetPreviousPrimaryMainFrameURL();
  const std::string visit_site = GetSite(visit_url);

  if (NavigationState* nav_state =
          NavigationState::GetForNavigationHandle(*navigation_handle)) {
    const base::TimeDelta visit_duration =
        nav_state->started_at() - last_committed_at_;
    // Only emit metrics for visits up to 10 seconds long.
    if (visit_duration <= base::Seconds(10)) {
      // Round the duration to the nearest second.
      const int64_t visit_seconds =
          (visit_duration.InMilliseconds() + 500) / 1000;
      const std::string next_site =
          GetSite(web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL());

      ukm::builders::BTM_ShortVisit(page_source_id_)
          .SetVisitDuration(visit_seconds)
          .SetExitWasRendererInitiated(navigation_handle->IsRendererInitiated())
          .SetExitHadUserGesture(navigation_handle->HasUserGesture())
          .SetExitPageTransition(navigation_handle->GetPageTransition())
          // TODO: .SetSiteEngagement()
          // TODO: .SetTimeSinceLastInteraction()
          .SetPreviousSiteSame(prev_site_.has_value() ? prev_site_ == visit_site
                                                      : -1)
          .SetNextSiteSame(visit_site == next_site)
          .SetPreviousAndNextSiteSame(
              prev_site_.has_value() ? prev_site_ == next_site : -1)
          .Record(ukm::UkmRecorder::Get());
    }
  }

  page_source_id_ = web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  last_committed_at_ = clock_->Now();
  prev_site_ = visit_site;
}

}  // namespace content

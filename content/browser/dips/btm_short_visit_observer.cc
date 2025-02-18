// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_short_visit_observer.h"

#include <cstdint>
#include <optional>

#include "base/functional/bind.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/dips/dips_service_impl.h"
#include "content/browser/dips/dips_state.h"
#include "content/browser/dips/dips_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
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

const base::Clock* g_default_clock = nullptr;

}  // namespace

BtmShortVisitObserver::BtmShortVisitObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      clock_(g_default_clock ? *g_default_clock
                             : *base::DefaultClock::GetInstance()),
      last_committed_at_(clock_->Now()),
      page_source_id_(
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()) {}

BtmShortVisitObserver::~BtmShortVisitObserver() = default;

/* static */
const base::Clock* BtmShortVisitObserver::SetDefaultClockForTesting(
    const base::Clock* clock) {
  return std::exchange(g_default_clock, clock);
}

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

namespace {

// Returns the latest time of the two ranges (or nullopt if both are null).
std::optional<base::Time> LastEvent(TimestampRange rng1, TimestampRange rng2) {
  if (rng1.has_value()) {
    if (rng2.has_value()) {
      return std::max(rng1->second, rng2->second);
    } else {
      return rng1->second;
    }
  } else {
    if (rng2.has_value()) {
      return rng2->second;
    } else {
      return std::nullopt;
    }
  }
}

void EmitShortVisit(ukm::builders::BTM_ShortVisit event,
                    base::Time now,
                    std::optional<BtmState> btm_state) {
  if (!btm_state.has_value()) {
    // No BtmService.
    event.SetTimeSinceLastInteraction(-2);
  } else if (std::optional<base::Time> last_interaction =
                 LastEvent(btm_state->user_activation_times(),
                           btm_state->web_authn_assertion_times())) {
    const int64_t ms = (now - *last_interaction).InMillisecondsRoundedUp();
    event.SetTimeSinceLastInteraction(
        ms >= 0 ? ukm::GetSemanticBucketMinForDurationTiming(ms) : -3);
  } else {
    // No previous interaction.
    event.SetTimeSinceLastInteraction(-1);
  }
  event.Record(ukm::UkmRecorder::Get());
}

}  // namespace

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

  if (prev_site_.has_value()) {
    if (NavigationState* nav_state =
            NavigationState::GetForNavigationHandle(*navigation_handle)) {
      const base::TimeDelta visit_duration =
          nav_state->started_at() - last_committed_at_;
      // Only emit metrics for visits up to 10 seconds long.
      if (visit_duration <= base::Seconds(10)) {
        // Round the duration to the nearest second.
        const int64_t visit_seconds =
            (visit_duration.InMilliseconds() + 500) / 1000;
        const std::string next_site = GetSite(navigation_handle->GetURL());
        const bool prev_site_same = prev_site_ == visit_site;
        const bool next_site_same = visit_site == next_site;

        if (!prev_site_same || !next_site_same) {
          ukm::builders::BTM_ShortVisit event(page_source_id_);
          event.SetVisitDuration(visit_seconds)
              .SetExitWasRendererInitiated(
                  navigation_handle->IsRendererInitiated())
              .SetExitHadUserGesture(navigation_handle->HasUserGesture())
              .SetExitPageTransition(navigation_handle->GetPageTransition())
              // TODO: .SetSiteEngagement()
              .SetPreviousSiteSame(prev_site_same)
              .SetNextSiteSame(next_site_same)
              .SetPreviousAndNextSiteSame(prev_site_ == next_site);
          if (auto* btm =
                  BtmServiceImpl::Get(web_contents()->GetBrowserContext())) {
            btm->storage()
                ->AsyncCall(&BtmStorage::Read)
                .WithArgs(visit_url)
                .Then(base::BindOnce(&EmitShortVisit, std::move(event),
                                     clock_->Now()));
          } else {
            EmitShortVisit(std::move(event), clock_->Now(), std::nullopt);
          }
        }
      }
    }
  }

  page_source_id_ = navigation_handle->GetNextPageUkmSourceId();
  last_committed_at_ = clock_->Now();
  prev_site_ = visit_site;
}

}  // namespace content

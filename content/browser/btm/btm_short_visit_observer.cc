// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_short_visit_observer.h"

#include <cstdint>
#include <optional>
#include <set>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_state.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr base::TimeDelta kShortVisitMaxDuration = base::Seconds(10);

// State associated with navigations that BtmShortVisitObserver needs to be able
// to discard if a navigation doesn't commit.
class NavigationState : public NavigationHandleUserData<NavigationState> {
 public:
  NavigationState(NavigationHandle& navigation_handle, base::Time started_at)
      : started_at_(started_at) {}

  base::Time started_at() const { return started_at_; }

  const std::set<GURL>& cookie_urls() const { return cookie_urls_; }
  void RecordCookieAccess(const GURL& url) { cookie_urls_.insert(url); }

 private:
  const base::Time started_at_;
  // URLs in the navigation that accessed cookies.
  std::set<GURL> cookie_urls_;

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

struct NoBtmService {};
struct NoInteraction {};
// Represents the BTM.ShortVisit::TimeSinceLastInteraction UKM metric.
using TimeSinceInteraction =
    std::variant<NoBtmService, NoInteraction, base::TimeDelta>;

// Get the actual integer that should be reported in
// BTM.ShortVisit::TimeSinceLastInteraction.
int64_t ToMetricValue(TimeSinceInteraction interaction_time) {
  return std::visit(  //
      base::Overloaded{[&](NoBtmService) -> int64_t { return -2; },
                       [&](NoInteraction) -> int64_t { return -1; },
                       [&](base::TimeDelta td) -> int64_t {
                         if (td.is_negative()) {
                           return -3;
                         }
                         return ukm::GetSemanticBucketMinForDurationTiming(
                             td.InMillisecondsRoundedUp());
                       }},
      std::move(interaction_time));
}

}  // namespace

// BtmShortVisitObserver emits a UKM event when a user navigates away from a
// page, which depends on the time of the last user interaction, which is
// fetched asynchronously from the BTM database when the user first landed on
// the page. The fetch or the next navigation could complete first, and the
// event can't be emitted until both have. This class stores the necessary state
// to accomplish that.
class BtmShortVisitObserver::AsyncMetricsState {
 public:
  // Initialize the state with the time the current page was committed.
  explicit AsyncMetricsState(base::Time committed_at);
  ~AsyncMetricsState();

  base::WeakPtr<AsyncMetricsState> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Set the time since the last user interaction. Must only be called once.
  void SetTimeSinceInteraction(TimeSinceInteraction delta);
  // Set the UKM event builder. `builder` must already have all metrics
  // populated except for TimeSinceLastInteraction. Must only be called once.
  void SetUkmBuilder(ukm::builders::BTM_ShortVisit builder);

  // The time the page was committed.
  base::Time committed_at() const { return committed_at_; }
  // Returns true iff we have the data needed to emit the UKM event.
  bool is_ready() const {
    return time_since_interaction_.has_value() && builder_.has_value();
  }
  // Emit the UKM metric.
  void Emit();

 private:
  const base::Time committed_at_;
  std::optional<TimeSinceInteraction> time_since_interaction_;
  std::optional<ukm::builders::BTM_ShortVisit> builder_;
  base::WeakPtrFactory<AsyncMetricsState> weak_factory_{this};
};

BtmShortVisitObserver::AsyncMetricsState::AsyncMetricsState(
    base::Time committed_at)
    : committed_at_(committed_at) {}
BtmShortVisitObserver::AsyncMetricsState::~AsyncMetricsState() = default;

void BtmShortVisitObserver::AsyncMetricsState::SetTimeSinceInteraction(
    TimeSinceInteraction delta) {
  CHECK(!time_since_interaction_.has_value());
  time_since_interaction_ = delta;
}

void BtmShortVisitObserver::AsyncMetricsState::SetUkmBuilder(
    ukm::builders::BTM_ShortVisit builder) {
  CHECK(!builder_.has_value());
  builder_ = std::move(builder);
}

void BtmShortVisitObserver::AsyncMetricsState::Emit() {
  CHECK(is_ready());
  builder_->SetTimeSinceLastInteraction(
      ToMetricValue(*time_since_interaction_));
  builder_->Record(ukm::UkmRecorder::Get());
}

void BtmShortVisitObserver::StoreLastInteraction(
    base::WeakPtr<AsyncMetricsState> metrics_state,
    const BtmState& btm_state) {
  if (!metrics_state) {
    // The user is no longer on the page and it was not a relevant short visit,
    // so the metrics state was already deleted.
    return;
  }

  std::optional<base::Time> last_interaction = LastEvent(
      btm_state.user_activation_times(), btm_state.web_authn_assertion_times());

  if (last_interaction.has_value()) {
    metrics_state->SetTimeSinceInteraction(metrics_state->committed_at() -
                                           *last_interaction);
  } else {
    metrics_state->SetTimeSinceInteraction(NoInteraction());
  }

  if (!metrics_state->is_ready()) {
    // We don't have the UKM builder, so the user must still be on the page.
    // (DidFinishNavigation() will delete the state.)
    return;
  }

  metrics_state->Emit();
  // If we're ready to emit, then the next navigation already completed and the
  // state must be in `pending_metrics_`. Delete it since it's done.
  auto iter = pending_metrics_.find(metrics_state.get());
  CHECK(iter != pending_metrics_.end());
  pending_metrics_.erase(iter);
}

void BtmShortVisitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    // Ignore irrelevant navigations.
    return;
  }

  GURL visit_url = navigation_handle->GetPreviousPrimaryMainFrameURL();
  if (visit_url.is_empty()) {
    if (const std::optional<url::Origin>& origin =
            navigation_handle->GetInitiatorOrigin()) {
      visit_url = origin->GetURL();
    }
  }
  const std::string visit_site = GetSite(visit_url);
  NavigationState* const nav_state =
      NavigationState::GetForNavigationHandle(*navigation_handle);

  if (nav_state && prev_site_.has_value()) {
    const base::TimeDelta visit_duration =
        nav_state->started_at() - last_committed_at_;
    // Only emit metrics for short visits.
    if (visit_duration <= kShortVisitMaxDuration) {
      // Round the duration to the nearest second.
      const int64_t visit_seconds =
          (visit_duration.InMilliseconds() + 500) / 1000;
      const std::string next_site = GetSite(navigation_handle->GetURL());
      const bool prev_site_same = prev_site_ == visit_site;
      const bool next_site_same = visit_site == next_site;

      if (!prev_site_same || !next_site_same) {
        const int32_t visit_id = static_cast<int32_t>(base::RandUint64());
        ukm::builders::BTM_ShortVisit event(page_source_id_);
        event.SetVisitDuration(visit_seconds)
            .SetEntranceWasRendererInitiated(
                last_navigation.was_renderer_initiated)
            .SetEntranceHadUserGesture(last_navigation.had_user_gesture)
            .SetEntrancePageTransition(last_navigation.page_transition)
            .SetExitWasRendererInitiated(
                navigation_handle->IsRendererInitiated())
            .SetExitHadUserGesture(navigation_handle->HasUserGesture())
            .SetExitPageTransition(navigation_handle->GetPageTransition())
            // TODO: .SetSiteEngagement()
            .SetPreviousSiteSame(prev_site_same)
            .SetNextSiteSame(next_site_same)
            .SetPreviousAndNextSiteSame(prev_site_ == next_site)
            .SetHadKeyDownEvent(had_keydown_event_)
            .SetAccessedStorage(page_accessed_storage_)
            .SetShortVisitId(visit_id);
        current_page_metrics_->SetUkmBuilder(std::move(event));
        if (current_page_metrics_->is_ready()) {
          current_page_metrics_->Emit();
          current_page_metrics_.reset();
        } else {
          // Still waiting for the call to BtmStorage::Read() to get the last
          // interaction time. (StoreLastInteraction() will delete the state.)
          pending_metrics_.insert(std::move(current_page_metrics_));
        }

        if (prev_source_id_ != ukm::kInvalidSourceId) {
          ukm::builders::BTM_ShortVisitNeighbor(prev_source_id_)
              .SetShortVisitId(visit_id)
              .SetIsPreceding(true)
              .Record(ukm::UkmRecorder::Get());
        }

        ukm::builders::BTM_ShortVisitNeighbor(
            navigation_handle->GetNextPageUkmSourceId())
            .SetShortVisitId(visit_id)
            .SetIsPreceding(false)
            .Record(ukm::UkmRecorder::Get());
      }
    }
  }

  had_keydown_event_ = false;
  page_accessed_storage_ = false;
  if (nav_state) {
    page_accessed_storage_ =
        base::Contains(nav_state->cookie_urls(), navigation_handle->GetURL());
  }
  last_navigation.was_renderer_initiated =
      navigation_handle->IsRendererInitiated();
  last_navigation.had_user_gesture = navigation_handle->HasUserGesture();
  last_navigation.page_transition = navigation_handle->GetPageTransition();
  prev_source_id_ = page_source_id_;
  page_source_id_ = navigation_handle->GetNextPageUkmSourceId();
  last_committed_at_ = clock_->Now();
  prev_site_ = visit_site;
  current_page_metrics_ =
      std::make_unique<AsyncMetricsState>(last_committed_at_);
  if (auto* btm = BtmServiceImpl::Get(web_contents()->GetBrowserContext())) {
    btm->storage()
        ->AsyncCall(&BtmStorage::Read)
        .WithArgs(navigation_handle->GetURL())
        .Then(base::BindOnce(&BtmShortVisitObserver::StoreLastInteraction,
                             weak_factory_.GetWeakPtr(),
                             current_page_metrics_->GetWeakPtr()));
  } else {
    current_page_metrics_->SetTimeSinceInteraction(NoBtmService());
  }
}

void BtmShortVisitObserver::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    had_keydown_event_ = true;
  }
}

void BtmShortVisitObserver::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  if (!render_frame_host->GetMainFrame()->IsInPrimaryMainFrame()) {
    return;
  }

  if (details.source != CookieAccessDetails::Source::kNavigation) {
    page_accessed_storage_ = true;
    return;
  }

  if (details.type == CookieAccessDetails::Type::kChange &&
      details.url == render_frame_host->GetLastCommittedURL()) {
    // A late notification of a navigational cookie write performed by the
    // current page.
    page_accessed_storage_ = true;
  }
}

void BtmShortVisitObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const CookieAccessDetails& details) {
  if (!IsInPrimaryPage(*navigation_handle)) {
    return;
  }

  if (details.type == CookieAccessDetails::Type::kRead) {
    // Ignore cookie reads due to navigation.
    return;
  }

  if (NavigationState* nav_state =
          NavigationState::GetForNavigationHandle(*navigation_handle)) {
    nav_state->RecordCookieAccess(details.url);
  }
}

void BtmShortVisitObserver::NotifyStorageAccessed(
    RenderFrameHost* render_frame_host,
    blink::mojom::StorageTypeAccessed storage_type,
    bool blocked) {
  if (!render_frame_host->GetMainFrame()->IsInPrimaryMainFrame()) {
    return;
  }

  page_accessed_storage_ = true;
}

}  // namespace content

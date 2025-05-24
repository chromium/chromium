// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_SHORT_VISIT_OBSERVER_H_
#define CONTENT_BROWSER_BTM_BTM_SHORT_VISIT_OBSERVER_H_

#include <optional>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/page_transition_types.h"

namespace base {
class Clock;
}

namespace content {

class BtmState;

// Emits UKM metrics for pages visited only for a short time.
class CONTENT_EXPORT BtmShortVisitObserver
    : public content::WebContentsObserver {
 public:
  // See the definition in the .cc file for a description.
  class AsyncMetricsState;

  explicit BtmShortVisitObserver(content::WebContents* web_contents);
  ~BtmShortVisitObserver() override;

  static const base::Clock* SetDefaultClockForTesting(const base::Clock* clock);

  // WebContentsObserver overrides:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;
  void NotifyStorageAccessed(RenderFrameHost* render_frame_host,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;

 private:
  // Called with the result of reading the BTM state for a page.
  void StoreLastInteraction(base::WeakPtr<AsyncMetricsState> metrics_state,
                            const BtmState& btm_state);

  // For determining the length of page visits; can be overridden in tests.
  const raw_ref<const base::Clock> clock_;
  // The eTLD+1 of the page visited before the currently-committed page.
  std::optional<std::string> prev_site_;
  // The UKM source id of the page visited before the current page.
  ukm::SourceId prev_source_id_ = ukm::kInvalidSourceId;
  // Several properties of the navigation that led to the current page.
  struct {
    bool was_renderer_initiated = false;
    bool had_user_gesture = false;
    ui::PageTransition page_transition = ui::PAGE_TRANSITION_LINK;
  } last_navigation;
  // Whether the current page has received a keydown event.
  bool had_keydown_event_ = false;
  // Whether the current page has accessed storage (cookies, localStorage, etc).
  bool page_accessed_storage_ = false;
  // The time the current page committed.
  base::Time last_committed_at_;
  // The source ID of the current page -- used in DidFinishNavigation() to emit
  // an event for the previous page.
  ukm::SourceId page_source_id_;
  // Metrics state for the current page.
  std::unique_ptr<AsyncMetricsState> current_page_metrics_;
  // Metrics state for previously-visited pages that we're still waiting for BTM
  // state for.
  std::set<std::unique_ptr<AsyncMetricsState>, base::UniquePtrComparator>
      pending_metrics_;
  base::WeakPtrFactory<BtmShortVisitObserver> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_SHORT_VISIT_OBSERVER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_PAGE_VISIT_OBSERVER_H_
#define CONTENT_BROWSER_BTM_BTM_PAGE_VISIT_OBSERVER_H_

#include <deque>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT BtmPageVisitInfo {
  GURL url;
  ukm::SourceId source_id;
  bool had_active_storage_access = false;
  bool received_user_activation = false;
  bool had_successful_web_authn_assertion = false;
  // Computed based on wall-clock times, so the usual caveat for working with
  // wall-clock times applies: the system clock may have been changed during the
  // page visit and so the duration may not be positive.
  base::TimeDelta visit_duration;
};

struct CONTENT_EXPORT BtmServerRedirectInfo {
  GURL url;
  ukm::SourceId source_id;
  bool did_write_cookies = false;
};

struct CONTENT_EXPORT BtmNavigationInfo {
  // Precondition: `navigation_handle.HasCommitted()` must be `true`.
  explicit BtmNavigationInfo(NavigationHandle& navigation_handle);
  BtmNavigationInfo(BtmNavigationInfo&&);
  BtmNavigationInfo& operator=(BtmNavigationInfo&&);
  ~BtmNavigationInfo();

  std::vector<BtmServerRedirectInfo> server_redirects;
  bool was_user_initiated;
  bool was_renderer_initiated;
  ui::PageTransition page_transition;
  // The page where the navigation ultimately committed.
  GURL destination_url;
  ukm::SourceId destination_source_id;
};

// Observes a `WebContents` and reports page visit information to a callback. A
// page visit is defined as the time from when a primary page is committed until
// the next primary page change. Also attempts to attribute late cookie access
// notifications (reports of navigational cookie accesses that are received
// after the navigation has finished) to the appropriate page or redirect.
class CONTENT_EXPORT BtmPageVisitObserver : public WebContentsObserver {
 public:
  using VisitCallback =
      base::RepeatingCallback<void(BtmPageVisitInfo, BtmNavigationInfo)>;

  // The arguments to `VisitCallback`.
  struct VisitTuple {
    BtmPageVisitInfo prev_page;
    BtmNavigationInfo navigation;
  };

  BtmPageVisitObserver(WebContents* web_contents,
                       VisitCallback callback,
                       base::Clock* clock = base::DefaultClock::GetInstance());
  ~BtmPageVisitObserver() override;

  // WebContentsObserver overrides:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void NotifyStorageAccessed(RenderFrameHost* render_frame_host,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;
  void FrameReceivedUserActivation(RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      RenderFrameHost* render_frame_host) override;

  void SetClockForTesting(base::Clock* clock) { clock_ = CHECK_DEREF(clock); }

 private:
  // Execute the visit callback with a tuple from the pending queue.
  void ReportVisit();

  // The callback to execute for each page visit.
  VisitCallback callback_;
  // Metadata on the currently committed page.
  BtmPageVisitInfo current_page_;
  raw_ref<base::Clock> clock_;
  base::Time last_page_change_time_;
  // Past page visits that we are holding on to, to see if any late cookie
  // access notifications come through for them. A "late" cookie access
  // notification is when a navigational cookie access is reported after the
  // navigation has finished.
  std::deque<VisitTuple> pending_visits_;
  base::WeakPtrFactory<BtmPageVisitObserver> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_PAGE_VISIT_OBSERVER_H_

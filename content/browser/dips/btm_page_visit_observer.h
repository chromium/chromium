// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_H_
#define CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_H_

#include <deque>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT BtmPageVisitInfo {
  GURL url;
  bool had_qualifying_storage_access = false;
};

struct CONTENT_EXPORT BtmServerRedirectInfo {
  GURL url;
  bool did_write_cookies = false;
};

struct CONTENT_EXPORT BtmNavigationInfo {
  BtmNavigationInfo();
  BtmNavigationInfo(const BtmNavigationInfo&);
  BtmNavigationInfo(BtmNavigationInfo&&);
  ~BtmNavigationInfo();

  std::vector<BtmServerRedirectInfo> server_redirects;
};

class CONTENT_EXPORT BtmPageVisitObserver : public WebContentsObserver {
 public:
  using VisitCallback = base::RepeatingCallback<
      void(const BtmPageVisitInfo&, const BtmNavigationInfo&, const GURL&)>;

  // The three arguments to `VisitCallback`.
  struct VisitTuple {
    BtmPageVisitInfo prev_page;
    BtmNavigationInfo navigation;
    GURL url;
  };

  BtmPageVisitObserver(WebContents* web_contents, VisitCallback callback);
  ~BtmPageVisitObserver() override;

  // WebContentsObserver overrides:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;

 private:
  // Execute the visit callback with a tuple from the pending queue.
  void ReportVisit();

  // The callback to execute for each page visit.
  VisitCallback callback_;
  // Metadata on the currently committed page.
  BtmPageVisitInfo current_page_;
  // Past page visits that we are still waiting to see if late cookie accesses
  // are reported for them.
  std::deque<VisitTuple> pending_visits_;
  base::WeakPtrFactory<BtmPageVisitObserver> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_H_

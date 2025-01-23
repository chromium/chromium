// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_H_
#define CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {

struct CONTENT_EXPORT BtmPageVisitInfo {
  GURL url;
};

struct CONTENT_EXPORT BtmServerRedirectInfo {
  GURL url;
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
  BtmPageVisitObserver(WebContents* web_contents, VisitCallback callback);
  ~BtmPageVisitObserver() override;

  // WebContentsObserver overrides:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  VisitCallback callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIPS_BTM_PAGE_VISIT_OBSERVER_H_

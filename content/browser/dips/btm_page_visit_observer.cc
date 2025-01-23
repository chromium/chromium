// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_page_visit_observer.h"

#include "content/public/browser/navigation_handle.h"

namespace content {

BtmNavigationInfo::BtmNavigationInfo() = default;
BtmNavigationInfo::BtmNavigationInfo(const BtmNavigationInfo&) = default;
BtmNavigationInfo::BtmNavigationInfo(BtmNavigationInfo&&) = default;
BtmNavigationInfo::~BtmNavigationInfo() = default;

BtmPageVisitObserver::BtmPageVisitObserver(WebContents* web_contents,
                                           VisitCallback callback)
    : WebContentsObserver(web_contents), callback_(callback) {}

BtmPageVisitObserver::~BtmPageVisitObserver() = default;

void BtmPageVisitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  BtmNavigationInfo navigation;
  for (size_t i = 1; i < navigation_handle->GetRedirectChain().size(); ++i) {
    navigation.server_redirects.emplace_back(
        navigation_handle->GetRedirectChain()[i - 1]);
  }
  callback_.Run(
      BtmPageVisitInfo{navigation_handle->GetPreviousPrimaryMainFrameURL()},
      navigation, navigation_handle->GetURL());
}

}  // namespace content

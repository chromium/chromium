// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/breadcrumbs/core/breadcrumb_manager_tab_helper.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewScrollViewProxyObserver;

namespace web {
class WebState;
}  // namespace web

// Handles logging of Breadcrumb events associated with `web_state_`.
class BreadcrumbManagerTabHelper
    : public breadcrumbs::BreadcrumbManagerTabHelper,
      public web::WebStateObserver,
      public web::WebStateUserData<BreadcrumbManagerTabHelper> {
 public:
  ~BreadcrumbManagerTabHelper() override;
  BreadcrumbManagerTabHelper(const BreadcrumbManagerTabHelper&) = delete;
  BreadcrumbManagerTabHelper& operator=(const BreadcrumbManagerTabHelper&) =
      delete;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  friend class web::WebStateUserData<BreadcrumbManagerTabHelper>;

  explicit BreadcrumbManagerTabHelper(web::WebState* web_state);

  // breadcrumbs::BreadcrumbManagerTabHelper:
  void PlatformLogEvent(const std::string& event) override;

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Helpers used to create and respond to the webState scrollViewProxy.
  void CreateBreadcrumbScrollingObserver();
  void OnScrollEvent(const std::string& event);

  // The webstate associated with this tab helper.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // A counter which is incremented for each scroll event. This value is reset
  // when any other event is logged.
  int sequentially_scrolled_ = 0;

  // Allows observing Objective-C object for Scroll and Zoom events.
  __strong id<CRWWebViewScrollViewProxyObserver> scroll_observer_;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<BreadcrumbManagerTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

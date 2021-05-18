// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

#include <string>

#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewScrollViewProxyObserver;

namespace web {
class WebState;
}  // namespace web

// Name of DidStartNavigation event (see WebStateObserver::DidStartNavigation).
extern const char kBreadcrumbDidStartNavigation[];

// Name of DidStartNavigation event (see WebStateObserver::DidFinishNavigation).
extern const char kBreadcrumbDidFinishNavigation[];

// Name of PageLoaded event (see WebStateObserver::PageLoaded).
extern const char kBreadcrumbPageLoaded[];

// Name of DidChangeVisibleSecurityState event
// (see WebStateObserver::DidChangeVisibleSecurityState).
extern const char kBreadcrumbDidChangeVisibleSecurityState[];

// Name of OnInfoBarAdded event
// (see infobars::InfoBarManager::Observer::OnInfoBarAdded).
extern const char kBreadcrumbInfobarAdded[];

// Name of OnInfoBarRemoved event
// (see infobars::InfoBarManager::Observer::OnInfoBarRemoved).
extern const char kBreadcrumbInfobarRemoved[];

// Name of OnInfoBarReplaced event
// (see infobars::InfoBarManager::Observer::OnInfoBarReplaced).
extern const char kBreadcrumbInfobarReplaced[];

// Name of Scroll event, logged when web contents scroll view finishes
// scrolling.
extern const char kBreadcrumbScroll[];

// Name of Zoom event, logged when web contents scroll view finishes zooming.
extern const char kBreadcrumbZoom[];

// Constants below represent metadata for breadcrumb events.

// Appended to |kBreadcrumbDidChangeVisibleSecurityState| event if page has bad
// SSL cert.
extern const char kBreadcrumbAuthenticationBroken[];

// Appended to |kBreadcrumbDidFinishNavigation| event if
// navigation is a download.
extern const char kBreadcrumbDownload[];

// Appended to |kBreadcrumbInfobarRemoved| if infobar removal is not animated.
extern const char kBreadcrumbInfobarNotAnimated[];

// Appended to |kBreadcrumbDidChangeVisibleSecurityState| event if page has
// passive mixed content (f.e. an http served image on https served page).
extern const char kBreadcrumbMixedContent[];

// Appended to |kBreadcrumbPageLoaded| event if page load has
// failed.
extern const char kBreadcrumbPageLoadFailure[];

// Appended to |kBreadcrumbDidStartNavigation| and
// |kBreadcrumbPageLoaded| event if the navigation url was Chrome's New Tab
// Page.
extern const char kBreadcrumbNtpNavigation[];

// Appended to |kBreadcrumbDidStartNavigation| and
// |kBreadcrumbPageLoaded| events if the navigation url had google.com host.
// Users tend to search and then navigate back and forth between search results
// page and landing page. And these back-forward navigations can cause crashes.
extern const char kBreadcrumbGoogleNavigation[];

// Appended to |kBreadcrumbPageLoaded| event if content is PDF.
extern const char kBreadcrumbPdfLoad[];

// Appended to |kBreadcrumbDidStartNavigation| event if navigation
// was a client side redirect (f.e. window.open without user gesture).
extern const char kBreadcrumbRendererInitiatedByScript[];

// Appended to |kBreadcrumbDidStartNavigation| event if navigation
// was renderer-initiated navigation with user gesture (f.e. link tap or
// widow.open with user gesture).
extern const char kBreadcrumbRendererInitiatedByUser[];

// Handles logging of Breadcrumb events associated with |web_state_|.
class BreadcrumbManagerTabHelper
    : public infobars::InfoBarManager::Observer,
      public web::WebStateObserver,
      public web::WebStateUserData<BreadcrumbManagerTabHelper> {
 public:
  ~BreadcrumbManagerTabHelper() override;

  // Returns a unique identifier to be used in breadcrumb event logs to identify
  // events associated with the underlying WebState. This value is unique across
  // this application run only and is NOT persisted and will change across
  // launches.
  int GetUniqueId() const { return unique_id_; }

 private:
  friend class web::WebStateUserData<BreadcrumbManagerTabHelper>;

  explicit BreadcrumbManagerTabHelper(web::WebState* web_state);

  BreadcrumbManagerTabHelper(const BreadcrumbManagerTabHelper&) = delete;
  BreadcrumbManagerTabHelper& operator=(const BreadcrumbManagerTabHelper&) =
      delete;

  // Logs an event for the associated WebState.
  void LogEvent(const std::string& event);

  // web::WebStateObserver implementation.
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

  // infobars::InfoBarManager::Observer
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  // The webstate associated with this tab helper.
  web::WebState* web_state_ = nullptr;
  int unique_id_ = -1;

  infobars::InfoBarManager* infobar_manager_ = nullptr;
  // A counter which is incremented for each |OnInfoBarReplaced| call. This
  // value is reset when any other infobars::InfoBarManager::Observer callback
  // is received.
  int sequentially_replaced_infobars_ = 0;

  // A counter which is incremented for each scroll event. This value is reset
  // when any other event is logged.
  int sequentially_scrolled_ = 0;

  // Manages this object as an observer of infobars.
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_observation_{this};

  // Allows observing Objective-C object for Scroll and Zoom events.
  __strong id<CRWWebViewScrollViewProxyObserver> scroll_observer_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_TAB_HELPER_H_

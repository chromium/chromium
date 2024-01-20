// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IPH_FOR_NEW_CHROME_USER_MODEL_TAB_BASED_IPH_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_IPH_FOR_NEW_CHROME_USER_MODEL_TAB_BASED_IPH_BROWSER_AGENT_H_

#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"

class Browser;
@class CommandDispatcher;
@protocol HelpCommands;
class UrlLoadingNotifierBrowserAgent;
class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

// A browser agent that serves a central manager for all IPHs that should be
// triggered by tab and/or tab list changes.
class TabBasedIPHBrowserAgent : public BrowserUserData<TabBasedIPHBrowserAgent>,
                                public BrowserObserver,
                                public UrlLoadingObserver,
                                public web::WebStateObserver {
 public:
  TabBasedIPHBrowserAgent(const TabBasedIPHBrowserAgent&) = delete;
  TabBasedIPHBrowserAgent& operator=(const TabBasedIPHBrowserAgent&) = delete;

  ~TabBasedIPHBrowserAgent() override;

  // Notifies the browser agent that the user has performed a multi-gesture tab
  // refresh, so that the related in-product help would be attempted.
  void NotifyMultiGestureRefreshEvent();

 private:
  friend class BrowserUserData<TabBasedIPHBrowserAgent>;

  explicit TabBasedIPHBrowserAgent(Browser* browser);

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // UrlLoadingObserver
  void TabDidLoadUrl(const GURL& url,
                     ui::PageTransition transition_type) override;
  void NewTabDidLoadUrl(const GURL& url, bool user_initiated) override;

  // WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidStopLoading(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // If the multi gesture refresh has occurred, resets it. Also it removes
  // existing IPH related to multi-gesture refresh.
  void ResetMultiGestureRefreshStateAndRemoveIPH();

  // Command handler for help commands.
  id<HelpCommands> HelpHandler();

  // Observer for the browser's web state list and the active web state.
  raw_ptr<WebStateList> web_state_list_;
  std::unique_ptr<ActiveWebStateObservationForwarder>
      active_web_state_observer_;

  // Observer for URL loading.
  raw_ptr<UrlLoadingNotifierBrowserAgent> url_loading_notifier_;
  // Command dispatcher for the browser; used to retrieve help handler.
  CommandDispatcher* command_dispatcher_;
  // Records events for the use of in-product help.
  raw_ptr<feature_engagement::Tracker> engagement_tracker_;

  // Whether a multi-gesture refresh is currently happening.
  bool multi_gesture_refresh_ = false;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_IPH_FOR_NEW_CHROME_USER_MODEL_TAB_BASED_IPH_BROWSER_AGENT_H_

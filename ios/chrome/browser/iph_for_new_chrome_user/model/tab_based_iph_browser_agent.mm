// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"

TabBasedIPHBrowserAgent::TabBasedIPHBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()),
      active_web_state_observer_(
          std::make_unique<ActiveWebStateObservationForwarder>(web_state_list_,
                                                               this)),
      url_loading_notifier_(
          UrlLoadingNotifierBrowserAgent::FromBrowser(browser)),
      command_dispatcher_(browser->GetCommandDispatcher()),
      engagement_tracker_(
          feature_engagement::TrackerFactory::GetForBrowserState(
              browser->GetBrowserState())) {
  browser->AddObserver(this);
  url_loading_notifier_->AddObserver(this);
}

TabBasedIPHBrowserAgent::~TabBasedIPHBrowserAgent() = default;

void TabBasedIPHBrowserAgent::NotifyMultiGestureRefreshEvent() {
  engagement_tracker_->NotifyEvent(
      feature_engagement::events::kIOSMultiGestureRefreshUsed);
  multi_gesture_refresh_ = true;
}

#pragma mark - BrowserObserver

void TabBasedIPHBrowserAgent::BrowserDestroyed(Browser* browser) {
  active_web_state_observer_.reset();
  url_loading_notifier_->RemoveObserver(this);
  browser->RemoveObserver(this);
  web_state_list_ = nil;
  url_loading_notifier_ = nil;
  command_dispatcher_ = nil;
  engagement_tracker_ = nil;
}

#pragma mark - UrlLoadingObserver

void TabBasedIPHBrowserAgent::TabDidLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  ResetMultiGestureRefreshStateAndRemoveIPH();
  web::WebState* currentWebState = web_state_list_->GetActiveWebState();
  if (currentWebState) {
    if ((transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) ||
        (transition_type & ui::PAGE_TRANSITION_FORWARD_BACK)) {
      [HelpHandler() presentNewTabToolbarItemBubble];
    }
    GURL visible = currentWebState->GetLastCommittedURL();
    if (url == visible &&
        transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR &&
        url != kChromeUINewTabURL) {
      NotifyMultiGestureRefreshEvent();
    }
  }
}

void TabBasedIPHBrowserAgent::NewTabDidLoadUrl(const GURL& url,
                                               bool user_initiated) {
  if (user_initiated) {
    [HelpHandler() presentTabGridToolbarItemBubble];
  }
}

#pragma mark - WebStateObserver

void TabBasedIPHBrowserAgent::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // `multi_gesture_refresh_` would be set to `false` immediately after the
  // presentation of the pull-to-refresh IPH, so it is possible that the IPH is
  // still visible when the user attempted a new navigation. Remove it from
  // view.
  //
  // However, if `multi_gesture_refresh_` is `true`, this invocation is most
  // likely caused by the multi-gesture refresh, so we would NOT do anything
  // here. In case the user navigates away when `multi_gesture_refresh_` is
  // called, it would be handled by `DidStopLoading`.
  if (!multi_gesture_refresh_) {
    [HelpHandler() removePullToRefreshSideSwipeBubble];
  }
}

void TabBasedIPHBrowserAgent::DidStopLoading(web::WebState* web_state) {
  if (multi_gesture_refresh_) {
    if (web_state->GetLoadingProgress() == 1) {
      [HelpHandler() presentPullToRefreshSideSwipeBubble];
      multi_gesture_refresh_ = false;
      return;
    }
    // User navigates away before loading completes.
    ResetMultiGestureRefreshStateAndRemoveIPH();
  }
}

void TabBasedIPHBrowserAgent::WasHidden(web::WebState* web_state) {
  // User either goes to the tab grid or switches tab with a swipe on the bottom
  // tab grid; remove the IPH from view.
  ResetMultiGestureRefreshStateAndRemoveIPH();
}

void TabBasedIPHBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  ResetMultiGestureRefreshStateAndRemoveIPH();
}

#pragma mark - Private

void TabBasedIPHBrowserAgent::ResetMultiGestureRefreshStateAndRemoveIPH() {
  multi_gesture_refresh_ = false;
  [HelpHandler() removePullToRefreshSideSwipeBubble];
}

id<HelpCommands> TabBasedIPHBrowserAgent::HelpHandler() {
  return HandlerForProtocol(command_dispatcher_, HelpCommands);
}

BROWSER_USER_DATA_KEY_IMPL(TabBasedIPHBrowserAgent)

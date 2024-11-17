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
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

TabBasedIPHBrowserAgent::TabBasedIPHBrowserAgent(Browser* browser)
    : web_state_list_(browser->GetWebStateList()),
      active_web_state_observer_(
          std::make_unique<ActiveWebStateObservationForwarder>(web_state_list_,
                                                               this)),
      url_loading_notifier_(
          UrlLoadingNotifierBrowserAgent::FromBrowser(browser)),
      command_dispatcher_(browser->GetCommandDispatcher()),
      engagement_tracker_(
          feature_engagement::TrackerFactory::GetForProfile(browser->GetProfile())) {
  browser->AddObserver(this);
  url_loading_notifier_->AddObserver(this);
}

TabBasedIPHBrowserAgent::~TabBasedIPHBrowserAgent() = default;

void TabBasedIPHBrowserAgent::RootViewForInProductHelpDidAppear() {
  web::WebState* current_web_state = web_state_list_->GetActiveWebState();
  if (tapped_adjacent_tab_ && current_web_state &&
      !current_web_state->IsLoading()) {
    [HelpHandler()
        presentInProductHelpWithType:InProductHelpType::kToolbarSwipe];
    tapped_adjacent_tab_ = false;
  }
}

void TabBasedIPHBrowserAgent::RootViewForInProductHelpWillDisappear() {
  ResetFeatureStatesAndRemoveIPHViews();
}

void TabBasedIPHBrowserAgent::NotifyMultiGestureRefreshEvent() {
  engagement_tracker_->NotifyEvent(
      feature_engagement::events::kIOSMultiGestureRefreshUsed);
  web::WebState* current_web_state = web_state_list_->GetActiveWebState();
  if (current_web_state) {
    // Check whether the page is scrolled to the top. Normally this should be
    // checked after the page has been fully refreshed, but at that time the web
    // view might not have resumed its original scroll offset. Adding a check
    // here as a precaution.
    CRWWebViewScrollViewProxy* proxy =
        current_web_state->GetWebViewProxy().scrollViewProxy;
    CGPoint scroll_offset = proxy.contentOffset;
    UIEdgeInsets content_inset = proxy.contentInset;
    if (AreCGFloatsEqual(scroll_offset.y, -content_inset.top)) {
      multi_gesture_refresh_ = true;
    }
  }
}

void TabBasedIPHBrowserAgent::NotifyBackForwardButtonTap() {
  ResetFeatureStatesAndRemoveIPHViews();
  engagement_tracker_->NotifyEvent(
      feature_engagement::events::kIOSBackForwardButtonTapped);
  back_forward_button_tapped_ = true;
}

void TabBasedIPHBrowserAgent::NotifySwitchToAdjacentTabFromTabGrid() {
  engagement_tracker_->NotifyEvent(
      feature_engagement::events::kIOSTabGridAdjacentTabTapped);
  tapped_adjacent_tab_ = true;
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
  ResetFeatureStatesAndRemoveIPHViews();
  web::WebState* current_web_state = web_state_list_->GetActiveWebState();
  if (current_web_state) {
    GURL visible = current_web_state->GetLastCommittedURL();
    if (url == visible &&
        transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR &&
        url != kChromeUINewTabURL) {
      NotifyMultiGestureRefreshEvent();
    }
  }
}

#pragma mark - WebStateObserver

void TabBasedIPHBrowserAgent::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument() &&
      !navigation_context->HasUserGesture()) {
    return;
  }
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
    [HelpHandler() handleTapOutsideOfVisibleGestureInProductHelp];
  }
}

void TabBasedIPHBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Catch back/forward swipe actions that is implemented by WKWebView instead
  // of the side swipe gesture recognizer.
  if (navigation_context->GetPageTransition() &
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK &&
      !navigation_context->HasUserGesture() &&
      !navigation_context->IsSameDocument()) {
    engagement_tracker_->NotifyEvent(
        feature_engagement::events::kIOSSwipeBackForwardUsed);
  }
}

void TabBasedIPHBrowserAgent::DidStopLoading(web::WebState* web_state) {
  // User navigates away before loading completes.
  // In case of multi-gesture refresh, `DidStopLoading` would be called BEFORE
  // the refresh attempt, instead of AFTER, so any invocations of this observer
  // that doesn't satisfy GetLoadingProgress() == 1 means user navigates away
  // from the current page before loading completes.
  if (web_state->GetLoadingProgress() < 1) {
    multi_gesture_refresh_ = false;
    tapped_adjacent_tab_ = false;
  }
  // If the user taps the back/forward button when the current page is still
  // loading, it is expected that `DidStopLoading` would be called with
  // `GetLoadingProgress() < 1` AFTER, as a result of the user performing the
  // tap. Therefore, the state should NOT be reset.
}

void TabBasedIPHBrowserAgent::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE) {
    ResetFeatureStatesAndRemoveIPHViews();
    return;
  }
  if (multi_gesture_refresh_) {
    [HelpHandler()
        presentInProductHelpWithType:InProductHelpType::kPullToRefresh];
    multi_gesture_refresh_ = false;
  } else if (back_forward_button_tapped_) {
    [HelpHandler()
        presentInProductHelpWithType:InProductHelpType::kBackForwardSwipe];
    back_forward_button_tapped_ = false;
  } else if (tapped_adjacent_tab_) {
    [HelpHandler()
        presentInProductHelpWithType:InProductHelpType::kToolbarSwipe];
    tapped_adjacent_tab_ = false;
  }
}

void TabBasedIPHBrowserAgent::WasHidden(web::WebState* web_state) {
  // User either goes to the tab grid or switches tab with a swipe on the bottom
  // tab grid; remove the IPH from view.
  ResetFeatureStatesAndRemoveIPHViews();
}

void TabBasedIPHBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  ResetFeatureStatesAndRemoveIPHViews();
}

#pragma mark - Private

void TabBasedIPHBrowserAgent::ResetFeatureStatesAndRemoveIPHViews() {
  multi_gesture_refresh_ = false;
  back_forward_button_tapped_ = false;
  tapped_adjacent_tab_ = false;
  // Invocation of this method is usually caused by manually triggered changes
  // to the web state, which is a result of the user tapping the location bar or
  // toolbar, both outside of the gestural IPH.
  [HelpHandler() handleTapOutsideOfVisibleGestureInProductHelp];
}

id<HelpCommands> TabBasedIPHBrowserAgent::HelpHandler() {
  return HandlerForProtocol(command_dispatcher_, HelpCommands);
}

BROWSER_USER_DATA_KEY_IMPL(TabBasedIPHBrowserAgent)

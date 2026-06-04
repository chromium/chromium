// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/types/pass_key.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent_observing.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

// C++ proxy class that generates the PassKey required to mutate
// the FullscreenBrowserAgent.
class FullscreenMediatorPassKeyProvider {
 public:
  static base::PassKey<FullscreenMediatorPassKeyProvider> passkey() {
    return base::PassKey<FullscreenMediatorPassKeyProvider>();
  }
};

namespace {
// Helper function to return a passkey used to mutate the browser agent state.
inline base::PassKey<FullscreenMediatorPassKeyProvider> PassKey() {
  return FullscreenMediatorPassKeyProvider::passkey();
}

// The threshold for direction-based snapping.
const CGFloat kFullscreenSnapThreshold = 10.0;
}  // namespace

@interface FullscreenMediator () <CRWWebStateObserver,
                                  CRWWebViewScrollViewProxyObserver,
                                  FullscreenBrowserAgentObserving,
                                  OmniboxPositionBrowserAgentObserving,
                                  WebStateListObserving,
                                  WebViewProxyTabHelperObserving>

// The active WebState.
@property(nonatomic, assign) web::WebState* webState;

// The scroll view proxy of the active WebState.
@property(nonatomic, weak) CRWWebViewScrollViewProxy* scrollViewProxy;

@end

@implementation FullscreenMediator {
  raw_ptr<FullscreenBrowserAgent> _browserAgent;
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebViewProxyTabHelperObserverBridge> _webViewProxyObserver;
  std::unique_ptr<FullscreenBrowserAgentObserverBridge> _browserAgentObserver;
  std::unique_ptr<OmniboxPositionBrowserAgentObserverBridge>
      _omniboxPositionObserver;
  std::unique_ptr<ScopedFullscreenDisabler> _voiceOverDisabler;
  CGFloat _lastContentOffset;
  BOOL _isBottomOmnibox;
  BOOL _updatingInsets;
  BOOL _handlingScroll;
  // Indicates whether the inset ranges have been initialized on startup.
  BOOL _hasInitializedInsets;
  // Scroll distance since the start of the drag, or since the scroll direction
  // changed.
  CGFloat _scrollTotal;
}

#pragma mark - Public

- (instancetype)initWithBrowserAgent:(FullscreenBrowserAgent*)browserAgent
                        webStateList:(WebStateList*)webStateList
         omniboxPositionBrowserAgent:
             (OmniboxPositionBrowserAgent*)omniboxPositionBrowserAgent {
  if ((self = [super init])) {
    CHECK(browserAgent);
    CHECK(webStateList);
    CHECK(omniboxPositionBrowserAgent);
    _browserAgent = browserAgent;
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webViewProxyObserver =
        std::make_unique<WebViewProxyTabHelperObserverBridge>(self);
    _browserAgentObserver =
        std::make_unique<FullscreenBrowserAgentObserverBridge>(self,
                                                               browserAgent);
    _omniboxPositionObserver =
        std::make_unique<OmniboxPositionBrowserAgentObserverBridge>(
            self, omniboxPositionBrowserAgent);
    _isBottomOmnibox =
        omniboxPositionBrowserAgent->IsCurrentLayoutBottomOmnibox();
    self.webState = _webStateList->GetActiveWebState();

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter
        addObserver:self
           selector:@selector(voiceOverStatusDidChange)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(applicationDidEnterBackground)
                          name:UIApplicationDidEnterBackgroundNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(applicationWillEnterForeground)
                          name:UIApplicationWillEnterForegroundNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(keyboardWillShow:)
                          name:UIKeyboardWillShowNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(keyboardWillChangeFrame:)
                          name:UIKeyboardWillChangeFrameNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(keyboardWillHide:)
                          name:UIKeyboardWillHideNotification
                        object:nil];
  }
  return self;
}

- (void)disconnect {
  _browserAgent = nullptr;
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
  self.webState = nullptr;
  _webStateObserver = nullptr;
  _browserAgentObserver = nullptr;
  _webViewProxyObserver = nullptr;
  _omniboxPositionObserver = nullptr;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    WebViewProxyTabHelper::FromWebState(_webState)->RemoveObserver(
        _webViewProxyObserver.get());
  }
  _webState = webState;
  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());
    WebViewProxyTabHelper* tabHelper =
        WebViewProxyTabHelper::FromWebState(_webState);
    tabHelper->AddObserver(_webViewProxyObserver.get());
    self.scrollViewProxy = tabHelper->GetWebViewProxy().scrollViewProxy;
    _browserAgent->ExitFullscreen(
        PassKey(), FullscreenModeTransitionTrigger::kForcedByCode,
        /*animated=*/false);
  } else {
    self.scrollViewProxy = nil;
  }
}

- (void)setScrollViewProxy:(CRWWebViewScrollViewProxy*)scrollViewProxy {
  if (_scrollViewProxy == scrollViewProxy) {
    return;
  }
  [_scrollViewProxy removeObserver:self];
  _scrollViewProxy = scrollViewProxy;
  [_scrollViewProxy addObserver:self];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (status.active_web_state_change()) {
    self.webState = status.new_active_web_state;
  }
}

#pragma mark - OmniboxPositionBrowserAgentObserving

- (void)omniboxPositionBrowserAgent:(OmniboxPositionBrowserAgent*)browser_agent
                  didUpdatePosition:(BOOL)isCurrentLayoutBottomOmnibox {
  if (_isBottomOmnibox == isCurrentLayoutBottomOmnibox) {
    return;
  }
  _isBottomOmnibox = isCurrentLayoutBottomOmnibox;
  _browserAgent->InvalidateInsetRange();
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  id<CRWWebViewProxy> webView =
      WebViewProxyTabHelper::FromWebState(webState)->GetWebViewProxy();
  if (@available(iOS 26, *)) {
    webView.shouldUseViewContentInset = YES;
  } else {
    webView.shouldUseViewContentInset = NO;
  }
  // TODO(crbug.com/496229929): Call InvalidateInsetRange() from the correct
  // event(s).
  if (!_hasInitializedInsets) {
    _browserAgent->InvalidateInsetRange();
    _hasInitializedInsets = YES;
  } else {
    [self setViewportInsetRange];
  }
  [self updateViewportInsets:_browserAgent->insets()];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (!navigationContext->IsSameDocument()) {
    _browserAgent->ExitFullscreen(
        PassKey(), FullscreenModeTransitionTrigger::kForcedByCode,
        /*animated=*/true);
    [self updateViewportInsets:_browserAgent->insets()];
  }
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(self.webState, webState);
  self.webState = nullptr;
}

#pragma mark - WebViewProxyTabHelperObserving

- (void)webViewProxyDidChange:(WebViewProxyTabHelper*)tabHelper {
  self.scrollViewProxy = tabHelper->GetWebViewProxy().scrollViewProxy;
}

- (void)webViewProxyTabHelperWasDestroyed:(WebViewProxyTabHelper*)tabHelper {
  self.scrollViewProxy = nil;
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidScroll:(CRWWebViewScrollViewProxy*)scrollView {
  CGFloat contentOffset = scrollView.contentOffset.y;
  CGFloat delta = contentOffset - _lastContentOffset;
  _lastContentOffset = contentOffset;

  // Ignore programmatic scrolls (e.g. from inset updates). Only process scroll
  // events that are actively driven by the user's touch or residual momentum.
  if (!scrollView.isDragging && !scrollView.isDecelerating) {
    return;
  }

  // Check if content is scrolled past the top.
  CGFloat topInsetRemaining =
      _browserAgent->max_insets().top - _browserAgent->insets().top;
  if (contentOffset + topInsetRemaining <= -scrollView.contentInset.top) {
    return;
  }
  // Check if content is scrolled past the bottom.
  CGFloat scrollViewHeight = CGRectGetHeight(scrollView.frame);
  CGFloat contentHeight = scrollView.contentSize.height;
  if (contentOffset + scrollViewHeight - scrollView.contentInset.bottom >
      contentHeight) {
    return;
  }

  if (_handlingScroll || _updatingInsets) {
    return;
  }
  _handlingScroll = YES;

  if (delta != 0) {
    // If the direction changed, reset the _scrollTotal.
    if ((delta > 0 && _scrollTotal < 0) || (delta < 0 && _scrollTotal > 0)) {
      _scrollTotal = delta;
    } else {
      _scrollTotal += delta;
    }
  }

  _browserAgent->IncrementalScroll(
      delta, FullscreenMediatorPassKeyProvider::passkey());

  _handlingScroll = NO;
}

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  _lastContentOffset = webViewScrollViewProxy.contentOffset.y;
  _scrollTotal = 0;
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  if (!decelerate) {
    [self snap];
  }
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self snap];
}

- (void)webViewScrollViewWillBeginZooming:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // TODO(crbug.com/491845727): Implement zoom lock logic.
}

- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale {
  // TODO(crbug.com/491845727): Implement zoom lock logic.
}

#pragma mark - FullscreenCommands

- (void)enterFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                          animated:(BOOL)animated {
  _browserAgent->EnterFullscreen(PassKey(), trigger, animated);
}

- (void)exitFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                         animated:(BOOL)animated {
  _browserAgent->ExitFullscreen(PassKey(), trigger, animated);
}

- (void)disableFullscreenAnimated:(BOOL)animated {
  _browserAgent->IncrementDisabledCounter(PassKey(), animated);
}

- (void)reenableFullscreen {
  _browserAgent->DecrementDisabledCounter(PassKey());
}

#pragma mark - System Notifications

- (void)voiceOverStatusDidChange {
  _voiceOverDisabler = UIAccessibilityIsVoiceOverRunning()
                           ? std::make_unique<ScopedFullscreenDisabler>(self)
                           : nullptr;
}

- (void)applicationDidEnterBackground {
  [self exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                         animated:NO];
}

- (void)applicationWillEnterForeground {
  [self exitFullscreenWithTrigger:FullscreenModeTransitionTrigger::kForcedByCode
                         animated:NO];
}

- (void)keyboardWillShow:(NSNotification*)notification {
  [self updateKeyboardHeightFromNotification:notification];
}

- (void)keyboardWillChangeFrame:(NSNotification*)notification {
  [self updateKeyboardHeightFromNotification:notification];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  if (_browserAgent) {
    _browserAgent->SetKeyboardObscuredInset(0);
  }
}

- (void)updateKeyboardHeightFromNotification:(NSNotification*)notification {
  if (!_browserAgent || !self.webState) {
    return;
  }

  UIView* view = self.webState->GetView();
  if (!view.window) {
    return;
  }

  _browserAgent->SetKeyboardObscuredInset(
      VisibleKeyboardHeightFromNotification(notification, view.window));
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenDidUpdateState:(FullscreenBrowserAgent*)agent {
  [self updateViewportInsets:agent->insets()];
}

- (void)fullscreenDidUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent {
  [self setViewportInsetRange];
}

#pragma mark - Private

// Sets the min/max viewport insets for the current WebView.
- (void)setViewportInsetRange {
  if (!self.webState) {
    return;
  }

  id<CRWWebViewProxy> webView =
      WebViewProxyTabHelper::FromWebState(self.webState)->GetWebViewProxy();
  [webView setMinimumViewportInset:_browserAgent->min_insets()
              maximumViewportInset:_browserAgent->max_insets()];
}

// Updates the WebView's obscuredContentInset and the scroll view's
// contentInset to adjust for the current position and size of
// the toolbars.
- (void)updateViewportInsets:(UIEdgeInsets)insets {
  if (!self.webState) {
    return;
  }

  id<CRWWebViewProxy> webView =
      WebViewProxyTabHelper::FromWebState(self.webState)->GetWebViewProxy();
  CRWWebViewScrollViewProxy* scrollView = webView.scrollViewProxy;

  if (UIEdgeInsetsEqualToEdgeInsets(insets, scrollView.contentInset) &&
      UIEdgeInsetsEqualToEdgeInsets(insets, webView.obscuredInsets)) {
    return;
  }

  _updatingInsets = YES;
  if (_browserAgent->invalidating_inset_range()) {
    // Do not allow the perceived scroll position to change when the obscured
    // inset is updated due to a device rotation or omnibox position change.
    CGPoint offset = _scrollViewProxy.contentOffset;
    offset.y += _scrollViewProxy.contentInset.top;
    webView.obscuredInsets = insets;
    offset.y -= _scrollViewProxy.contentInset.top;
    _scrollViewProxy.contentOffset = offset;
  } else {
    webView.obscuredInsets = insets;
  }
  _updatingInsets = NO;
}

// Snaps the fullscreen progress to 0.0 or 1.0.
- (void)snap {
  CGFloat topProgress = _browserAgent->top_progress();
  CGFloat bottomProgress = _browserAgent->bottom_progress();
  if ((topProgress == 0.0 && bottomProgress == 0.0) ||
      (topProgress == 1.0 && bottomProgress == 1.0)) {
    return;
  }

  // The type of snap to be executed.
  enum class SnapType { kExit, kEnter };
  SnapType snapType = SnapType::kExit;

  if (_scrollTotal > kFullscreenSnapThreshold) {
    snapType = SnapType::kEnter;
  } else if (_scrollTotal < -kFullscreenSnapThreshold) {
    snapType = SnapType::kExit;
  } else {
    CGFloat progress = topProgress;
    if (_browserAgent->min_insets().top == _browserAgent->max_insets().top) {
      progress = bottomProgress;
    }
    snapType = progress >= 0.5 ? SnapType::kExit : SnapType::kEnter;
  }

  if (snapType == SnapType::kExit) {
    _browserAgent->ExitFullscreen(
        PassKey(),
        FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode,
        /*animated=*/true);
  } else {
    _browserAgent->EnterFullscreen(
        PassKey(),
        FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode,
        /*animated=*/true);
  }
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/types/pass_key.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent_observing.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper.h"
#import "ios/chrome/browser/web/model/web_view_proxy/web_view_proxy_tab_helper_observer_bridge.h"
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
}  // namespace

@interface FullscreenMediator () <CRWWebStateObserver,
                                  CRWWebViewScrollViewProxyObserver,
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
  std::unique_ptr<OmniboxPositionBrowserAgentObserverBridge>
      _omniboxPositionObserver;
  CGFloat _lastContentOffset;
  BOOL _isBottomOmnibox;
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
  _browserAgent->InvalidateInsetRange(PassKey());
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasShown:(web::WebState*)webState {
  // TODO(crbug.com/496229929): Call InvalidateInsetRange() from the correct
  // event(s).
  _browserAgent->InvalidateInsetRange(PassKey());
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

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Ignore programmatic scrolls (e.g. from inset updates). Only process scroll
  // events that are actively driven by the user's touch or residual momentum.
  if (!webViewScrollViewProxy.isDragging &&
      !webViewScrollViewProxy.isDecelerating) {
    return;
  }

  CGFloat currentContentOffset = webViewScrollViewProxy.contentOffset.y;
  CGFloat delta = currentContentOffset - _lastContentOffset;
  _lastContentOffset = currentContentOffset;

  _browserAgent->IncrementalScroll(
      delta, FullscreenMediatorPassKeyProvider::passkey());
}

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  _lastContentOffset = webViewScrollViewProxy.contentOffset.y;
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  // TODO(crbug.com/491845727): Implement snapping animations.
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  // TODO(crbug.com/491845727): Implement snapping animations.
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // TODO(crbug.com/491845727): Implement snapping animations.
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

- (void)enterFullscreenWithAnimation:(BOOL)animated {
  _browserAgent->EnterFullscreen(PassKey(), animated);
}

- (void)exitFullscreenWithAnimation:(BOOL)animated {
  _browserAgent->ExitFullscreen(PassKey(), animated);
}

- (void)disableFullscreen {
  _browserAgent->IncrementDisabledCounter(PassKey());
}

- (void)reenableFullscreen {
  _browserAgent->DecrementDisabledCounter(PassKey());
}

#pragma mark - System Notifications

- (void)voiceOverStatusDidChange {
  // TODO(crbug.com/493903024): Toggle fullscreen disabled with
  // ScopedFullscreenDisabler.
}

- (void)applicationDidEnterBackground {
  [self exitFullscreenWithAnimation:NO];
}

- (void)applicationWillEnterForeground {
  [self exitFullscreenWithAnimation:NO];
}

@end

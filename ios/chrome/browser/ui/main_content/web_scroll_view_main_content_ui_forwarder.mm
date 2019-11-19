// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/web_scroll_view_main_content_ui_forwarder.h"

#include <memory>

#include "base/logging.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Uses the current values of |proxy|'s properties to update the
// MainContentUIState via |updater|.
void UpdateStateWithProxy(MainContentUIStateUpdater* updater,
                          CRWWebViewScrollViewProxy* proxy) {
  [updater scrollViewSizeDidChange:proxy.frame.size];
  [updater scrollViewDidResetContentSize:proxy.contentSize];
  [updater scrollViewDidResetContentInset:proxy.contentInset];
}
}

@interface WebScrollViewMainContentUIForwarder ()<
    CRWWebStateObserver,
    CRWWebViewScrollViewProxyObserver,
    WebStateListObserving> {
  // The observer bridges.
  std::unique_ptr<WebStateListObserver> _webStateListBridge;
  std::unique_ptr<web::WebStateObserver> _webStateBridge;
}

// The updater being driven by this object.
@property(nonatomic, readonly, strong) MainContentUIStateUpdater* updater;
// The WebStateList whose active WebState's scroll state is being forwaded.
@property(nonatomic, readonly) WebStateList* webStateList;
// The WebStateList's active WebState.
@property(nonatomic, assign) web::WebState* webState;
// The scroll view proxy whose scroll events are forwarded to |updater|.
@property(nonatomic, readonly, strong) CRWWebViewScrollViewProxy* proxy;

@end

@implementation WebScrollViewMainContentUIForwarder
@synthesize updater = _updater;
@synthesize webStateList = _webStateList;
@synthesize webState = _webState;
@synthesize proxy = _proxy;

- (instancetype)initWithUpdater:(MainContentUIStateUpdater*)updater
                   webStateList:(WebStateList*)webStateList {
  if (self = [super init]) {
    _updater = updater;
    DCHECK(_updater);
    _webStateList = webStateList;
    DCHECK(_webStateList);
    _webStateBridge = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListBridge = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListBridge.get());
    web::WebState* activeWebState = webStateList->GetActiveWebState();
    if (activeWebState) {
      _webState = activeWebState;
      _webState->AddObserver(_webStateBridge.get());
      _proxy = activeWebState->GetWebViewProxy().scrollViewProxy;
      [_proxy addObserver:self];
      UpdateStateWithProxy(_updater, _proxy);
    }
  }
  return self;
}

- (void)dealloc {
  // |-disconnect| must be called before deallocation.
  DCHECK(!_webStateListBridge);
  DCHECK(!_webStateBridge);
  DCHECK(!_webState);
  DCHECK(!_proxy);
}

#pragma mark Accessors

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;
  if (_webState)
    _webState->RemoveObserver(_webStateBridge.get());
  _webState = webState;
  if (_webState)
    _webState->AddObserver(_webStateBridge.get());
  self.proxy =
      _webState ? _webState->GetWebViewProxy().scrollViewProxy : nullptr;
}

- (void)setProxy:(CRWWebViewScrollViewProxy*)proxy {
  if (_proxy == proxy)
    return;
  [_proxy removeObserver:self];
  _proxy = proxy;
  [_proxy addObserver:self];
  UpdateStateWithProxy(_updater, _proxy);
}

#pragma mark Public

- (void)disconnect {
  self.webStateList->RemoveObserver(_webStateListBridge.get());
  _webStateListBridge = nullptr;
  self.webState = nullptr;
  _webStateBridge = nullptr;
}

#pragma mark CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  if (!navigation->IsSameDocument())
    [self.updater scrollWasInterrupted];
}

#pragma mark CRWWebViewScrollViewObserver

- (void)webViewScrollViewFrameDidChange:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Frame changes may move the scroll view relative to its safe area, so check
  // for content inset adjustments.
  [self checkForContentInsetAdjustment];

  [self.updater scrollViewSizeDidChange:webViewScrollViewProxy.frame.size];
}

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Check whether this scroll is due to a content inset adjustment.
  [self checkForContentInsetAdjustment];

  [self.updater scrollViewDidScrollToOffset:self.proxy.contentOffset];
}

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self.updater
      scrollViewWillBeginDraggingWithGesture:self.proxy.panGestureRecognizer];
}

- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  [self.updater
      scrollViewDidEndDraggingWithGesture:self.proxy.panGestureRecognizer
                      targetContentOffset:*targetContentOffset];
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self.updater scrollViewDidEndDecelerating];
}

- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self.updater
      scrollViewDidResetContentSize:webViewScrollViewProxy.contentSize];
}

- (void)webViewScrollViewDidResetContentInset:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self checkForContentInsetAdjustment];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  if (newWebState == webStateList->GetActiveWebState())
    self.webState = newWebState;
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  self.webState = newWebState;
}

#pragma mark - Private

// Checks whether the content inset has been updated, notifying the updater of
// any changes.
- (void)checkForContentInsetAdjustment {
  UIEdgeInsets inset = self.proxy.contentInset;
  inset = self.proxy.adjustedContentInset;
  if (!UIEdgeInsetsEqualToEdgeInsets(inset, self.updater.state.contentInset))
    [self.updater scrollViewDidResetContentInset:inset];
}

@end

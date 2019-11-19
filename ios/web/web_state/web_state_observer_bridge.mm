// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebStateObserverBridge::WebStateObserverBridge(id<CRWWebStateObserver> observer)
    : observer_(observer) {}

WebStateObserverBridge::~WebStateObserverBridge() = default;

void WebStateObserverBridge::WasShown(web::WebState* web_state) {
  if ([observer_ respondsToSelector:@selector(webStateWasShown:)]) {
    [observer_ webStateWasShown:web_state];
  }
}

void WebStateObserverBridge::WasHidden(web::WebState* web_state) {
  if ([observer_ respondsToSelector:@selector(webStateWasHidden:)]) {
    [observer_ webStateWasHidden:web_state];
  }
}

void WebStateObserverBridge::NavigationItemsPruned(web::WebState* web_state,
                                                   size_t pruned_item_count) {
  SEL selector = @selector(webState:didPruneNavigationItemsWithCount:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state
        didPruneNavigationItemsWithCount:pruned_item_count];
  }
}

void WebStateObserverBridge::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if ([observer_ respondsToSelector:@selector(webState:didStartNavigation:)]) {
    [observer_ webState:web_state didStartNavigation:navigation_context];
  }
}

void WebStateObserverBridge::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if ([observer_ respondsToSelector:@selector(webState:didFinishNavigation:)]) {
    [observer_ webState:web_state didFinishNavigation:navigation_context];
  }
}

void WebStateObserverBridge::DidStartLoading(web::WebState* web_state) {
  SEL selector = @selector(webStateDidStartLoading:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidStartLoading:web_state];
  }
}

void WebStateObserverBridge::DidStopLoading(web::WebState* web_state) {
  SEL selector = @selector(webStateDidStopLoading:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidStopLoading:web_state];
  }
}

void WebStateObserverBridge::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  SEL selector = @selector(webState:didLoadPageWithSuccess:);
  if ([observer_ respondsToSelector:selector]) {
    BOOL success = NO;
    switch (load_completion_status) {
      case PageLoadCompletionStatus::SUCCESS:
        success = YES;
        break;
      case PageLoadCompletionStatus::FAILURE:
        success = NO;
        break;
    }
    [observer_ webState:web_state didLoadPageWithSuccess:success];
  }
}

void WebStateObserverBridge::LoadProgressChanged(web::WebState* web_state,
                                                 double progress) {
  SEL selector = @selector(webState:didChangeLoadingProgress:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state didChangeLoadingProgress:progress];
  }
}

void WebStateObserverBridge::DidChangeBackForwardState(
    web::WebState* web_state) {
  SEL selector = @selector(webStateDidChangeBackForwardState:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidChangeBackForwardState:web_state];
  }
}

void WebStateObserverBridge::TitleWasSet(web::WebState* web_state) {
  if ([observer_ respondsToSelector:@selector(webStateDidChangeTitle:)]) {
    [observer_ webStateDidChangeTitle:web_state];
  }
}

void WebStateObserverBridge::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  SEL selector = @selector(webStateDidChangeVisibleSecurityState:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webStateDidChangeVisibleSecurityState:web_state];
  }
}

void WebStateObserverBridge::FaviconUrlUpdated(
    web::WebState* web_state,
    const std::vector<FaviconURL>& candidates) {
  SEL selector = @selector(webState:didUpdateFaviconURLCandidates:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state didUpdateFaviconURLCandidates:candidates];
  }
}

void WebStateObserverBridge::WebFrameDidBecomeAvailable(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  SEL selector = @selector(webState:frameDidBecomeAvailable:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state frameDidBecomeAvailable:web_frame];
  }
}

void WebStateObserverBridge::WebFrameWillBecomeUnavailable(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  SEL selector = @selector(webState:frameWillBecomeUnavailable:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ webState:web_state frameWillBecomeUnavailable:web_frame];
  }
}

void WebStateObserverBridge::RenderProcessGone(web::WebState* web_state) {
  if ([observer_ respondsToSelector:@selector(renderProcessGoneForWebState:)]) {
    [observer_ renderProcessGoneForWebState:web_state];
  }
}

void WebStateObserverBridge::WebStateDestroyed(web::WebState* web_state) {
  SEL selector = @selector(webStateDestroyed:);
  if ([observer_ respondsToSelector:selector]) {
    // |webStateDestroyed:| may delete |this|, so don't expect |this| to be
    // valid afterwards.
    [observer_ webStateDestroyed:web_state];
  }
}

}  // namespace web

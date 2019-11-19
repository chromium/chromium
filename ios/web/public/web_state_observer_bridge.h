// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"

// Observes page lifecyle events from Objective-C. To use as a
// web::WebStateObserver, wrap in a web::WebStateObserverBridge.
@protocol CRWWebStateObserver <NSObject>
@optional

// Invoked by WebStateObserverBridge::WasShown.
- (void)webStateWasShown:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::WasHidden.
- (void)webStateWasHidden:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::NavigationItemsPruned.
- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count;

// Invoked by WebStateObserverBridge::DidStartNavigation.
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation;

// Invoked by WebStateObserverBridge::DidFinishNavigation.
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation;

// Invoked by WebStateObserverBridge::PageLoaded.
- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success;

// Invoked by WebStateObserverBridge::LoadProgressChanged.
- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress;

// Invoked by WebStateObserverBridge::DidChangeBackForwardState.
- (void)webStateDidChangeBackForwardState:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::TitleWasSet.
- (void)webStateDidChangeTitle:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidChangeVisibleSecurityState.
- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::FaviconUrlUpdated.
- (void)webState:(web::WebState*)webState
    didUpdateFaviconURLCandidates:
        (const std::vector<web::FaviconURL>&)candidates;

// Invoked by WebStateObserverBridge::WebFrameDidBecomeAvailable.
- (void)webState:(web::WebState*)webState
    frameDidBecomeAvailable:(web::WebFrame*)web_frame;

// Invoked by WebStateObserverBridge::WebFrameWillBecomeUnavailable.
- (void)webState:(web::WebState*)webState
    frameWillBecomeUnavailable:(web::WebFrame*)web_frame;

// Invoked by WebStateObserverBridge::RenderProcessGone.
- (void)renderProcessGoneForWebState:(web::WebState*)webState;

// Note: after |webStateDestroyed:| is invoked, the WebState being observed
// is no longer valid.
- (void)webStateDestroyed:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidStopLoading.
- (void)webStateDidStopLoading:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidStartLoading.
- (void)webStateDidStartLoading:(web::WebState*)webState;

@end

namespace web {

// Bridge to use an id<CRWWebStateObserver> as a web::WebStateObserver.
class WebStateObserverBridge : public web::WebStateObserver {
 public:
  // It it the responsibility of calling code to add/remove the instance
  // from the WebStates observer lists.
  WebStateObserverBridge(id<CRWWebStateObserver> observer);
  ~WebStateObserverBridge() override;

  // web::WebStateObserver methods.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void NavigationItemsPruned(web::WebState* web_state,
                             size_t pruned_item_count) override;
  void DidStartNavigation(web::WebState* web_state,
                          NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void LoadProgressChanged(web::WebState* web_state, double progress) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void TitleWasSet(web::WebState* web_state) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  void FaviconUrlUpdated(web::WebState* web_state,
                         const std::vector<FaviconURL>& candidates) override;
  void WebFrameDidBecomeAvailable(WebState* web_state,
                                  WebFrame* web_frame) override;
  void WebFrameWillBecomeUnavailable(WebState* web_state,
                                     WebFrame* web_frame) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;

 private:
  __weak id<CRWWebStateObserver> observer_ = nil;
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverBridge);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_BRIDGE_H_

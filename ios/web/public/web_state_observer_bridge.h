// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "ios/web/public/web_state_observer.h"

namespace web {
class NavigationContext;
enum Permission : NSUInteger;
}

// Observes page lifecycle events from Objective-C. To use as a
// web::WebStateObserver, wrap in a web::WebStateObserverBridge.
@protocol CRWWebStateObserver <NSObject>
@optional

// Invoked by WebStateObserverBridge::WasShown.
- (void)webStateWasShown:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::WasHidden.
- (void)webStateWasHidden:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidStartNavigation.
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext;

// Invoked by WebStateObserverBridge::DidRedirectNavigation.
- (void)webState:(web::WebState*)webState
    didRedirectNavigation:(web::NavigationContext*)navigationContext;

// Invoked by WebStateObserverBridge::DidFinishNavigation.
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext;

// Invoked by WebStateObserverBridge::DidStartLoading.
- (void)webStateDidStartLoading:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::DidStopLoading.
- (void)webStateDidStopLoading:(web::WebState*)webState;

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

// Invoked by WebStateObserverBridge::PermissionStateChanged.
- (void)webState:(web::WebState*)webState
    didChangeStateForPermission:(web::Permission)permission;

// Invoked by WebStateObserverBridge::UnderPageBackgroundColorChanged.
- (void)webStateDidChangeUnderPageBackgroundColor:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::RenderProcessGone.
- (void)renderProcessGoneForWebState:(web::WebState*)webState;

// Invoked by WebStateObserverBridge::WebStateRealized.
- (void)webStateRealized:(web::WebState*)webState;

// Note: after `webStateDestroyed:` is invoked, the WebState being observed
// is no longer valid.
- (void)webStateDestroyed:(web::WebState*)webState;

@end

namespace web {

// Bridge to use an id<CRWWebStateObserver> as a web::WebStateObserver.
class WebStateObserverBridge : public web::WebStateObserver {
 public:
  // It it the responsibility of calling code to add/remove the instance
  // from the WebStates observer lists.
  WebStateObserverBridge(id<CRWWebStateObserver> observer);

  WebStateObserverBridge(const WebStateObserverBridge&) = delete;
  WebStateObserverBridge& operator=(const WebStateObserverBridge&) = delete;

  ~WebStateObserverBridge() override;

  // web::WebStateObserver methods.
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          NavigationContext* navigation_context) override;
  void DidRedirectNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           NavigationContext* navigation_context) override;
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void LoadProgressChanged(web::WebState* web_state, double progress) override;
  void DidChangeBackForwardState(web::WebState* web_state) override;
  void TitleWasSet(web::WebState* web_state) override;
  void DidChangeVisibleSecurityState(web::WebState* web_state) override;
  void FaviconUrlUpdated(web::WebState* web_state,
                         const std::vector<FaviconURL>& candidates) override;
  void PermissionStateChanged(web::WebState* web_state,
                              web::Permission permission) override;
  void UnderPageBackgroundColorChanged(WebState* web_state) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  __weak id<CRWWebStateObserver> observer_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_OBSERVER_BRIDGE_H_

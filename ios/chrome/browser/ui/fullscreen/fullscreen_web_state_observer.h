// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#include "ios/web/public/web_state_observer.h"
#include "url/gurl.h"

class FullscreenController;
class FullscreenMediator;
class FullscreenModel;
@class FullscreenWebViewProxyObserver;

// A WebStateObserver that updates a FullscreenModel for navigation events.
class FullscreenWebStateObserver : public web::WebStateObserver {
 public:
  // Constructor for an observer that updates `controller` and `model`.
  FullscreenWebStateObserver(FullscreenController* controller,
                             FullscreenModel* model,
                             FullscreenMediator* mediator);
  ~FullscreenWebStateObserver() override;

  // Tells the observer to start observing `web_state`.
  void SetWebState(web::WebState* web_state);

 private:
  // WebStateObserver:
  void WasShown(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void DidStartLoading(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState being observed.
  raw_ptr<web::WebState> web_state_ = nullptr;
  // The FullscreenController passed on construction.
  raw_ptr<FullscreenController> controller_;
  // The model passed on construction.
  raw_ptr<FullscreenModel> model_;
  // The mediator passed on construction.
  raw_ptr<FullscreenMediator> mediator_ = nullptr;
  // Observer for `web_state_`'s scroll view proxy.
  __strong FullscreenWebViewProxyObserver* web_view_proxy_observer_;
  // The URL received in the NavigationContext of the last finished navigation.
  GURL last_navigation_url_;
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_STATE_OBSERVER_H_

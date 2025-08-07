// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_TAB_HELPER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol MiniMapCommands;
class MiniMapService;

// Observes navigations to Google maps to show native UI.
class MiniMapTabHelper : public web::WebStateUserData<MiniMapTabHelper>,
                         public web::WebStateObserver,
                         public web::WebStatePolicyDecider {
 public:
  explicit MiniMapTabHelper(web::WebState* web_state);
  ~MiniMapTabHelper() override;

  // Sets the MiniMapCommands that can display mini maps.
  void SetMiniMapCommands(id<MiniMapCommands> mini_map_handler);

  // WebStateObserver overrides:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // WebStatePolicyDecider overrides:
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) override;
  void WebStateDestroyed() override;

 private:
  // Whether the navigation to `url` should be intercepted.
  bool ShouldInterceptRequest(NSURL* url, ui::PageTransition page_transition);

  // Whether the web_state is currently on Google SRP.
  bool is_on_google_srp_ = false;

  // Command handlers for mini maps commands.
  id<MiniMapCommands> mini_map_handler_ = nil;

  // Service to observe Profile scoped prefs.
  raw_ptr<MiniMapService> mini_map_service_;
};

#endif  // IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_TAB_HELPER_H_

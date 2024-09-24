// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewScrollViewProxyObserver;


namespace web {
class WebState;
}

// WebSessionStateTabHelper manages WKWebView session state reading, writing and
// deleting.
class WebSessionStateTabHelper
    : public web::WebFramesManager::Observer,
      public web::WebStateObserver,
      public web::WebStateUserData<WebSessionStateTabHelper> {
 public:
  WebSessionStateTabHelper(const WebSessionStateTabHelper&) = delete;
  WebSessionStateTabHelper& operator=(const WebSessionStateTabHelper&) = delete;

  ~WebSessionStateTabHelper() override;

  // Returns the WKWebView session data blob from cache or nil if the feature
  // is disabled or the data not found in the cache.
  NSData* FetchSessionFromCache();

  // Calls SaveSessionState if the tab helper is stale.
  void SaveSessionStateIfStale();

  // Persists the WKWebView's sessionState data via the WebSessionStateCache.
  void SaveSessionState();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  friend class web::WebStateUserData<WebSessionStateTabHelper>;

  explicit WebSessionStateTabHelper(web::WebState* web_state);

  // web::WebFramesManager::Observer
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Helpers used to create and respond to the webState scrollViewProxy.
  void CreateScrollingObserver();
  void OnScrollEvent();

  ProfileIOS* GetProfile();

  // Mark the tab helper as stale.  Future calls to SaveSessionStateIfStale()
  // will result in calls to SaveSessionState().
  void MarkStale();

  // Whether this tab helper is stale and needs a future call to
  // SaveSessionState().
  bool stale_ = false;

  // Cache the values of this `web_state`'s navigation manager GetItemCount()
  // and GetLastCommittedItemIndex().
  int item_count_ = 0;
  int last_committed_item_index_ = 0;

  // The WebState with which this object is associated.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Allows observing Objective-C object for Scroll and Zoom events.
  __strong id<CRWWebViewScrollViewProxyObserver> scroll_observer_;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<WebSessionStateTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_WEB_SESSION_STATE_TAB_HELPER_H_

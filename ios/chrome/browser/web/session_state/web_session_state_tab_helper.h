// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include "base/sequence_checker.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewScrollViewProxyObserver;

class ChromeBrowserState;

namespace web {
class WebState;
}

// WebSessionStateTabHelper manages WKWebView session state reading, writing and
// deleting.
class WebSessionStateTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<WebSessionStateTabHelper> {
 public:
  WebSessionStateTabHelper(const WebSessionStateTabHelper&) = delete;
  WebSessionStateTabHelper& operator=(const WebSessionStateTabHelper&) = delete;

  ~WebSessionStateTabHelper() override;

  // Returns true if the feature is enabled and running iOS 15 or newer.
  static bool IsEnabled();

  // If kRestoreSessionFromCache is enabled restore `web_state`'s WKWebView
  // using the previously saved sessionState data via the WebSessionStateCache.
  // Returns true if the session could be restored.
  bool RestoreSessionFromCache();

  // Calls SaveSessionState if the tab helper is stale.
  void SaveSessionStateIfStale();

  // Persists the WKWebView's sessionState data via the WebSessionStateCache.
  void SaveSessionState();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  friend class web::WebStateUserData<WebSessionStateTabHelper>;

  explicit WebSessionStateTabHelper(web::WebState* web_state);

  // web::WebStateObserver overrides:
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebFrameDidBecomeAvailable(web::WebState* web_state,
                                  web::WebFrame* web_frame) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Helpers used to create and respond to the webState scrollViewProxy.
  void CreateScrollingObserver();
  void OnScrollEvent();

  ChromeBrowserState* GetBrowserState();

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
  web::WebState* web_state_ = nullptr;

  // Allows observing Objective-C object for Scroll and Zoom events.
  __strong id<CRWWebViewScrollViewProxyObserver> scroll_observer_;

  WEB_STATE_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<WebSessionStateTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_SESSION_STATE_WEB_SESSION_STATE_TAB_HELPER_H_

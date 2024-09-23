// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_WEB_STATE_OBSERVER_H_

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

@class SessionRestorationScrollObserver;

// A tab helper that records whether a given WebState state should be
// saved to disk. It observes multiple events that indicate the content
// displayed has changed in a way that the user would want to see saved
// (such as navigated to a new page or scroll through the page).
class SessionRestorationWebStateObserver final
    : public web::WebStateObserver,
      public web::WebFramesManager::Observer,
      public web::WebStateUserData<SessionRestorationWebStateObserver> {
 public:
  // Callback passed upon construction. Will be invoked when the state
  // transition from clean to dirty (i.e. needs saving).
  using WebStateDirtyCallback = base::RepeatingCallback<void(web::WebState*)>;

  ~SessionRestorationWebStateObserver() final;

  // Returns whether the state of the WebState needs to be saved to disk.
  bool is_dirty() const { return is_dirty_; }

  // Should be called after saving the state of the WebState to disk. The
  // callback passed to the constructor will be called the next time this
  // is set to true.
  void clear_dirty() { is_dirty_ = false; }

  // web::WebStateObserver implementation.
  void WasShown(web::WebState* web_state) final;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) final;
  void WebStateRealized(web::WebState* web_state) final;
  void WebStateDestroyed(web::WebState* web_state) final;

  // web::WebFramesManager::Observer implementation.
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) final;

 private:
  friend class web::WebStateUserData<SessionRestorationWebStateObserver>;

  // Constructor.
  SessionRestorationWebStateObserver(web::WebState* web_state,
                                     WebStateDirtyCallback callback);

  // Called when scroll events are detected for the WebState.
  void OnScrollEvent();

  // Called when the state of the WebState needs to be saved. If the WebState
  // was not yet marked as dirty, will invoke `callback_`.
  void MarkDirty();

  const raw_ptr<web::WebState> web_state_;
  WebStateDirtyCallback callback_;

  bool is_dirty_ = false;

  int item_count_ = -1;
  int last_committed_item_index_ = -1;

  __strong SessionRestorationScrollObserver* scroll_observer_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_WEB_STATE_OBSERVER_H_

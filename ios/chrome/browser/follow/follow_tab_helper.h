// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_FOLLOW_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FOLLOW_FOLLOW_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

@protocol FollowIPHPresenter;

// FollowTabHelper encapsulates tab behavior related to following channels.
class FollowTabHelper : public web::WebStateObserver,
                        public web::WebStateUserData<FollowTabHelper> {
 public:
  FollowTabHelper(const FollowTabHelper&) = delete;
  FollowTabHelper& operator=(const FollowTabHelper&) = delete;

  ~FollowTabHelper() override;

  // Creates the TabHelper and attaches to |web_state|. |web_state| must not be
  // null.
  static void CreateForWebState(web::WebState* web_state);

  // Sets the presenter for follow in-product help (IPH). |presenter| is not
  // retained by this tab helper.
  void set_follow_iph_presenter(id<FollowIPHPresenter> presenter) {
    follow_iph_presenter_ = presenter;
  }

 private:
  friend class web::WebStateUserData<FollowTabHelper>;

  explicit FollowTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  web::WebState* web_state_ = nullptr;

  // Presenter for follow in-product help (IPH).
  __weak id<FollowIPHPresenter> follow_iph_presenter_ = nil;

  // Manages this object as an observer of |web_state_|.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<FollowTabHelper> weak_ptr_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_FOLLOW_TAB_HELPER_H_

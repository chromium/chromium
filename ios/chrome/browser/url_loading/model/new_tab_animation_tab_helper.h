// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_NEW_TAB_ANIMATION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_NEW_TAB_ANIMATION_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state_user_data.h"

// Records whether or not the WebState should be animated in when added.
class NewTabAnimationTabHelper
    : public web::WebStateUserData<NewTabAnimationTabHelper> {
 public:
  NewTabAnimationTabHelper(const NewTabAnimationTabHelper&) = delete;
  NewTabAnimationTabHelper& operator=(const NewTabAnimationTabHelper&) = delete;

  ~NewTabAnimationTabHelper() override;

  // Disables animation when adding the WebState as a new tab.
  void DisableNewTabAnimation();

  // Indicates whether or not to animate the insertion of the new tab for this
  // WebState.
  bool ShouldAnimateNewTab() const;

 private:
  explicit NewTabAnimationTabHelper(web::WebState* web_state);

  friend class web::WebStateUserData<NewTabAnimationTabHelper>;

  bool animation_disabled_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_NEW_TAB_ANIMATION_TAB_HELPER_H_

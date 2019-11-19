// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_OVERSCROLL_ACTIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_OVERSCROLL_ACTIONS_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol OverscrollActionsControllerDelegate;

namespace web {
class WebState;
}

// OverscrollActionsTabHelper is a Web state observer that owns the
// OverscrollActionsController.
class OverscrollActionsTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<OverscrollActionsTabHelper> {
 public:
  ~OverscrollActionsTabHelper() override;

  // Sets the delegate. The delegate is not owned by the tab helper.
  void SetDelegate(id<OverscrollActionsControllerDelegate> delegate);

  // Returns a pointer for the currently used OverscrollActionsController.
  OverscrollActionsController* GetOverscrollActionsController();

  // Forces the the controller to switch to NO_PULL_STARTED state.
  void Clear();

 private:
  friend class web::WebStateUserData<OverscrollActionsTabHelper>;

  OverscrollActionsTabHelper(web::WebState* web_state);

  // web::WebStateObserver override.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The Overscroll controller responsible for displaying the
  // overscrollActionsView above the toolbar.
  OverscrollActionsController* overscroll_actions_controller_ = nil;

  // A weak pointer to the OverscrollActionsControllerDelegate object.
  __weak id<OverscrollActionsControllerDelegate>
      overscroll_actions_controller_delegate_ = nil;

  // The WebState that is observer by the tab helper.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(OverscrollActionsTabHelper);
};

#endif  // IOS_CHROME_BROWSER_OVERSCROLL_ACTIONS_OVERSCROLL_ACTIONS_TAB_HELPER_H_

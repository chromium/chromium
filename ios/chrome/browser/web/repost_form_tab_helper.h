// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_REPOST_FORM_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_REPOST_FORM_TAB_HELPER_H_

#include <CoreGraphics/CoreGraphics.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol RepostFormTabHelperDelegate;

// Allows presenting a repost form dialog. Listens to web::WebState activity
// and dismisses the dialog when necessary.
class RepostFormTabHelper : public web::WebStateUserData<RepostFormTabHelper>,
                            public web::WebStateObserver {
 public:
  ~RepostFormTabHelper() override;

  // Creates TabHelper. |delegate| is not retained by TabHelper and must not be
  // null.
  static void CreateForWebState(web::WebState* web_state,
                                id<RepostFormTabHelperDelegate> delegate);

  // Presents a repost form dialog at the given |location|. |callback| is called
  // with true if the repost was confirmed and with false if it was cancelled.
  void PresentDialog(CGPoint location, base::OnceCallback<void(bool)> callback);

 private:
  friend class web::WebStateUserData<RepostFormTabHelper>;

  RepostFormTabHelper(web::WebState* web_state,
                      id<RepostFormTabHelperDelegate> delegate);

  // Called to dismiss the repost form dialog.
  void DismissReportFormDialog();

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  __weak id<RepostFormTabHelperDelegate> delegate_ = nil;

  // true if form repost dialog is currently being presented.
  bool is_presenting_dialog_ = false;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(RepostFormTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_REPOST_FORM_TAB_HELPER_H_

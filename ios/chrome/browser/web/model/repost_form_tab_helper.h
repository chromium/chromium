// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_REPOST_FORM_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_REPOST_FORM_TAB_HELPER_H_

#include <CoreGraphics/CoreGraphics.h>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol RepostFormTabHelperDelegate;

// Allows presenting a repost form dialog. Listens to web::WebState activity
// and dismisses the dialog when necessary.
class RepostFormTabHelper : public web::WebStateUserData<RepostFormTabHelper>,
                            public web::WebStateObserver {
 public:
  RepostFormTabHelper(const RepostFormTabHelper&) = delete;
  RepostFormTabHelper& operator=(const RepostFormTabHelper&) = delete;

  ~RepostFormTabHelper() override;

  // Presents a repost form dialog at the given `location`. `callback` is called
  // with true if the repost was confirmed and with false if it was cancelled.
  void PresentDialog(CGPoint location, base::OnceCallback<void(bool)> callback);

  // Set the delegate.
  void SetDelegate(id<RepostFormTabHelperDelegate> delegate);

 private:
  friend class web::WebStateUserData<RepostFormTabHelper>;

  RepostFormTabHelper(web::WebState* web_state);

  // Called to dismiss the repost form dialog.
  void DismissReportFormDialog();

  // Called by the callback passed to the delegate when the dialog has
  // been presented.
  void OnDialogPresented();

  // web::WebStateObserver overrides:
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  __weak id<RepostFormTabHelperDelegate> delegate_ = nil;

  // true if form repost dialog is currently being presented.
  bool is_presenting_dialog_ = false;

  base::WeakPtrFactory<RepostFormTabHelper> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_REPOST_FORM_TAB_HELPER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/repost_form_tab_helper.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

RepostFormTabHelper::RepostFormTabHelper(
    web::WebState* web_state,
    id<RepostFormTabHelperDelegate> delegate)
    : web_state_(web_state), delegate_(delegate) {
  web_state_->AddObserver(this);
}

RepostFormTabHelper::~RepostFormTabHelper() {
  DCHECK(!web_state_);
}

void RepostFormTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<RepostFormTabHelperDelegate> delegate) {
  DCHECK(web_state);
  DCHECK(delegate);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new RepostFormTabHelper(web_state, delegate)));
  }
}

void RepostFormTabHelper::DismissReportFormDialog() {
  if (is_presenting_dialog_)
    [delegate_ repostFormTabHelperDismissRepostFormDialog:this];
  is_presenting_dialog_ = false;
}

void RepostFormTabHelper::PresentDialog(
    CGPoint location,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(!is_presenting_dialog_);
  is_presenting_dialog_ = true;
  __block base::OnceCallback<void(bool)> block_callback = std::move(callback);
  [delegate_ repostFormTabHelper:this
      presentRepostFormDialogForWebState:web_state_
                           dialogAtPoint:location
                       completionHandler:^(BOOL should_continue) {
                         is_presenting_dialog_ = false;
                         std::move(block_callback).Run(should_continue);
                       }];
}

void RepostFormTabHelper::DidStartNavigation(web::WebState* web_state,
                                             web::NavigationContext*) {
  DCHECK_EQ(web_state_, web_state);
  DismissReportFormDialog();
}

void RepostFormTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DismissReportFormDialog();

  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(RepostFormTabHelper)

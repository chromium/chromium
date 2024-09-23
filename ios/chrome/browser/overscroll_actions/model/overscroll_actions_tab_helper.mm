// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

OverscrollActionsTabHelper::~OverscrollActionsTabHelper() {}

void OverscrollActionsTabHelper::SetDelegate(
    id<OverscrollActionsControllerDelegate> delegate) {
  DCHECK(web_state_);
  // The check for overscrollActionsControllerDelegate is necessary to avoid
  // recreating a OverscrollActionsController during teardown.
  if (overscroll_actions_controller_.delegate == delegate) {
    return;
  }

  // If the WebState is unrealized, save the delegate and do not create the
  // OverscrollActionsController. It will be created when the WebState becomes
  // "realized".
  if (!web_state_->IsRealized()) {
    delegate_ = delegate;
    return;
  }

  // Lazily create a OverscrollActionsController when setting the delegate.
  // OverscrollTabHelper can't be used without a delegate so if it's not set,
  // there is no point of having a controller.
  if (!overscroll_actions_controller_) {
    overscroll_actions_controller_ = [[OverscrollActionsController alloc]
        initWithWebViewProxy:web_state_->GetWebViewProxy()];

    overscroll_actions_controller_.style =
        web_state_->GetBrowserState()->IsOffTheRecord()
            ? OverscrollStyle::REGULAR_PAGE_INCOGNITO
            : OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  }

  DCHECK(overscroll_actions_controller_);
  overscroll_actions_controller_.delegate = delegate;
}

OverscrollActionsTabHelper::OverscrollActionsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_observation_.Observe(web_state_.get());
}

OverscrollActionsController*
OverscrollActionsTabHelper::GetOverscrollActionsController() {
  return overscroll_actions_controller_;
}

// web::WebStateObserver implementation.
void OverscrollActionsTabHelper::WebStateRealized(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  if (delegate_) {
    // This will create the OverscrollActionsController whose creation was
    // delayed because the WebState was unrealized.
    SetDelegate(delegate_);
    delegate_ = nil;
  }
}

void OverscrollActionsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_observation_.Reset();
  web_state_ = nullptr;

  [overscroll_actions_controller_ invalidate];
  overscroll_actions_controller_ = nil;
  delegate_ = nil;
}

void OverscrollActionsTabHelper::Clear() {
  [overscroll_actions_controller_ clear];
}

WEB_STATE_USER_DATA_KEY_IMPL(OverscrollActionsTabHelper)

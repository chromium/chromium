// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

OverscrollActionsTabHelper::~OverscrollActionsTabHelper() {
  [overscroll_actions_controller_ invalidate];
  overscroll_actions_controller_ = nil;
  delegate_ = nil;
}

void OverscrollActionsTabHelper::SetDelegate(
    id<OverscrollActionsControllerDelegate> delegate) {
  // The check for overscrollActionsControllerDelegate is necessary to avoid
  // recreating a OverscrollActionsController during teardown.
  if (overscroll_actions_controller_.delegate == delegate) {
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
    : web_state_(CHECK_DEREF(web_state)) {
  CHECK(web_state_->IsRealized());
}

OverscrollActionsController*
OverscrollActionsTabHelper::GetOverscrollActionsController() {
  return overscroll_actions_controller_;
}

void OverscrollActionsTabHelper::Clear() {
  [overscroll_actions_controller_ clear];
}

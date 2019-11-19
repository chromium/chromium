// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OverscrollActionsTabHelper::~OverscrollActionsTabHelper() {}

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
  }
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  overscroll_actions_controller_.style =
      browser_state->IsOffTheRecord()
          ? OverscrollStyle::REGULAR_PAGE_INCOGNITO
          : OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  overscroll_actions_controller_.delegate = delegate;
  overscroll_actions_controller_.browserState = browser_state;
}

OverscrollActionsTabHelper::OverscrollActionsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

OverscrollActionsController*
OverscrollActionsTabHelper::GetOverscrollActionsController() {
  return overscroll_actions_controller_;
}

// web::WebStateObserver implementation.
void OverscrollActionsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
  overscroll_actions_controller_delegate_ = nil;
  [overscroll_actions_controller_ invalidate];
  overscroll_actions_controller_ = nil;
}

void OverscrollActionsTabHelper::Clear() {
  [overscroll_actions_controller_ clear];
}

WEB_STATE_USER_DATA_KEY_IMPL(OverscrollActionsTabHelper)

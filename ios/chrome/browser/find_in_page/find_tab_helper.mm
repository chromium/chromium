// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/find_in_page/find_in_page_controller.h"
#import "ios/chrome/browser/find_in_page/find_in_page_model.h"
#import "ios/web/public/navigation/navigation_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FindTabHelper::FindTabHelper(web::WebState* web_state) {
  DCHECK(web_state);
  observation_.Observe(web_state);

  if (web_state->IsRealized()) {
    CreateFindInPageController(web_state);
  }
}

FindTabHelper::~FindTabHelper() {}

void FindTabHelper::SetResponseDelegate(
    id<FindInPageResponseDelegate> response_delegate) {
  if (!controller_) {
    response_delegate_ = response_delegate;
  } else {
    controller_.responseDelegate = response_delegate;
  }
}

void FindTabHelper::StartFinding(NSString* search_term) {
  [controller_ findStringInPage:search_term];
}

void FindTabHelper::ContinueFinding(FindDirection direction) {
  if (direction == FORWARD) {
    [controller_ findNextStringInPage];

  } else if (direction == REVERSE) {
    [controller_ findPreviousStringInPage];

  } else {
    NOTREACHED();
  }
}

void FindTabHelper::StopFinding() {
  SetFindUIActive(false);
  [controller_ disableFindInPage];
}

FindInPageModel* FindTabHelper::GetFindResult() const {
  return controller_.findInPageModel;
}

bool FindTabHelper::CurrentPageSupportsFindInPage() const {
  return [controller_ canFindInPage];
}

bool FindTabHelper::IsFindUIActive() const {
  return controller_.findInPageModel.enabled;
}

void FindTabHelper::SetFindUIActive(bool active) {
  controller_.findInPageModel.enabled = active;
}

void FindTabHelper::PersistSearchTerm() {
  [controller_ saveSearchTerm];
}

void FindTabHelper::RestoreSearchTerm() {
  [controller_ restoreSearchTerm];
}

void FindTabHelper::CreateFindInPageController(web::WebState* web_state) {
  DCHECK(!controller_);
  controller_ = [[FindInPageController alloc] initWithWebState:web_state];
  if (response_delegate_) {
    controller_.responseDelegate = response_delegate_;
    response_delegate_ = nil;
  }
}

void FindTabHelper::WebStateRealized(web::WebState* web_state) {
  CreateFindInPageController(web_state);
}

void FindTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();

  [controller_ detachFromWebState];
  controller_ = nil;
}

void FindTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  if (IsFindUIActive()) {
    StopFinding();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FindTabHelper)

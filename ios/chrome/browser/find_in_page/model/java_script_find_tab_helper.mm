// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/java_script_find_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_model.h"
#import "ios/chrome/browser/find_in_page/model/java_script_find_in_page_controller.h"
#import "ios/web/public/navigation/navigation_context.h"

JavaScriptFindTabHelper::JavaScriptFindTabHelper(web::WebState* web_state) {
  DCHECK(web_state);
  observation_.Observe(web_state);

  if (web_state->IsRealized()) {
    CreateFindInPageController(web_state);
  }
}

JavaScriptFindTabHelper::~JavaScriptFindTabHelper() {}

void JavaScriptFindTabHelper::SetResponseDelegate(
    id<FindInPageResponseDelegate> response_delegate) {
  if (!controller_) {
    response_delegate_ = response_delegate;
  } else {
    controller_.responseDelegate = response_delegate;
  }
}

void JavaScriptFindTabHelper::StartFinding(NSString* search_term) {
  [controller_ findStringInPage:search_term];
}

void JavaScriptFindTabHelper::ContinueFinding(FindDirection direction) {
  if (direction == FORWARD) {
    [controller_ findNextStringInPage];

  } else if (direction == REVERSE) {
    [controller_ findPreviousStringInPage];

  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void JavaScriptFindTabHelper::StopFinding() {
  SetFindUIActive(false);
  [controller_ disableFindInPage];
}

FindInPageModel* JavaScriptFindTabHelper::GetFindResult() const {
  return controller_.findInPageModel;
}

bool JavaScriptFindTabHelper::CurrentPageSupportsFindInPage() const {
  return [controller_ canFindInPage];
}

bool JavaScriptFindTabHelper::IsFindUIActive() const {
  return controller_.findInPageModel.enabled;
}

void JavaScriptFindTabHelper::SetFindUIActive(bool active) {
  controller_.findInPageModel.enabled = active;
}

void JavaScriptFindTabHelper::PersistSearchTerm() {
  [controller_ saveSearchTerm];
}

void JavaScriptFindTabHelper::RestoreSearchTerm() {
  [controller_ restoreSearchTerm];
}

void JavaScriptFindTabHelper::CreateFindInPageController(
    web::WebState* web_state) {
  DCHECK(!controller_);
  controller_ =
      [[JavaScriptFindInPageController alloc] initWithWebState:web_state];
  if (response_delegate_) {
    controller_.responseDelegate = response_delegate_;
    response_delegate_ = nil;
  }
}

void JavaScriptFindTabHelper::WebStateRealized(web::WebState* web_state) {
  CreateFindInPageController(web_state);
}

void JavaScriptFindTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();

  [controller_ detachFromWebState];
  controller_ = nil;
}

void JavaScriptFindTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  if (IsFindUIActive()) {
    StopFinding();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(JavaScriptFindTabHelper)

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/web_state.h"

BrowserWebStateListDelegate::BrowserWebStateListDelegate(
    ProfileIOS* profile,
    InsertionPolicy insertion_policy,
    ActivationPolicy activation_policy)
    : profile_(profile),
      insertion_policy_(insertion_policy),
      activation_policy_(activation_policy) {
  CHECK(profile);
}

BrowserWebStateListDelegate::~BrowserWebStateListDelegate() = default;

void BrowserWebStateListDelegate::WillAddWebState(web::WebState* web_state) {
  CHECK_EQ(profile_, web_state->GetBrowserState());
  if (insertion_policy_ != InsertionPolicy::kAttachTabHelpers) {
    return;
  }

  // Attach all TabHelpers. It is okay to call this function multiple
  // times (e.g. when a WebState is moved between Browsers) as it is
  // idempotent.
  AttachTabHelpers(web_state);
}

void BrowserWebStateListDelegate::WillActivateWebState(
    web::WebState* web_state) {
  CHECK_EQ(profile_, web_state->GetBrowserState());
  if (activation_policy_ != ActivationPolicy::kForceRealization) {
    return;
  }

  // Do not trigger a CheckForOverRealization here as some user actions
  // (such as side swipe over multiple tab in the tab strip) can cause
  // rapid change of the active WebState.
  web::IgnoreOverRealizationCheck();
  web_state->ForceRealized();
}

void BrowserWebStateListDelegate::WillRemoveWebState(web::WebState* web_state) {
  CHECK_EQ(profile_, web_state->GetBrowserState());
  // Nothing to do.
}

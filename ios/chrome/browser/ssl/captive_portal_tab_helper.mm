// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/captive_portal_tab_helper.h"

#include <memory>

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ssl/captive_portal_tab_helper_delegate.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
void CaptivePortalTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<CaptivePortalTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new CaptivePortalTabHelper(delegate)));
  }
}

CaptivePortalTabHelper::CaptivePortalTabHelper(
    id<CaptivePortalTabHelperDelegate> delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

void CaptivePortalTabHelper::DisplayCaptivePortalLoginPage(GURL landing_url) {
  [delegate_ captivePortalTabHelper:this connectWithLandingURL:landing_url];
}

CaptivePortalTabHelper::~CaptivePortalTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(CaptivePortalTabHelper)

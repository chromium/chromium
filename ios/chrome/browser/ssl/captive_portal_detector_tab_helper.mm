// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper_delegate.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
void CaptivePortalDetectorTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<CaptivePortalDetectorTabHelperDelegate> delegate,
    network::mojom::URLLoaderFactory* loader_factory_for_testing) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new CaptivePortalDetectorTabHelper(
                           web_state, delegate, loader_factory_for_testing)));
  }
}

CaptivePortalDetectorTabHelper::CaptivePortalDetectorTabHelper(
    web::WebState* web_state,
    id<CaptivePortalDetectorTabHelperDelegate> delegate,
    network::mojom::URLLoaderFactory* loader_factory_for_testing)
    : delegate_(delegate),
      detector_(std::make_unique<captive_portal::CaptivePortalDetector>(
          loader_factory_for_testing
              ? loader_factory_for_testing
              : web_state->GetBrowserState()->GetURLLoaderFactory())) {
  DCHECK(delegate);
}

captive_portal::CaptivePortalDetector*
CaptivePortalDetectorTabHelper::detector() {
  return detector_.get();
}

void CaptivePortalDetectorTabHelper::DisplayCaptivePortalLoginPage(
    GURL landing_url) {
  [delegate_ captivePortalDetectorTabHelper:this
                      connectWithLandingURL:landing_url];
}

CaptivePortalDetectorTabHelper::~CaptivePortalDetectorTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(CaptivePortalDetectorTabHelper)

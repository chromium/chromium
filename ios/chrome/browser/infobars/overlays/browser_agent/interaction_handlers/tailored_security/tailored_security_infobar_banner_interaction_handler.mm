// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/tailored_security/tailored_security_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_service_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using safe_browsing::TailoredSecurityServiceInfobarDelegate;

#pragma mark - InfobarBannerInteractionHandler

TailoredSecurityInfobarBannerInteractionHandler::
    TailoredSecurityInfobarBannerInteractionHandler(
        const OverlayRequestSupport* request_support)
    : InfobarBannerInteractionHandler(request_support) {}

TailoredSecurityInfobarBannerInteractionHandler::
    ~TailoredSecurityInfobarBannerInteractionHandler() = default;

void TailoredSecurityInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  infobar->set_accepted(GetInfobarDelegate(infobar)->Accept());
}
#pragma mark - Private

TailoredSecurityServiceInfobarDelegate*
TailoredSecurityInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  TailoredSecurityServiceInfobarDelegate* delegate =
      TailoredSecurityServiceInfobarDelegate::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}

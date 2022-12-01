// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"

#import "base/check.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - InfobarBannerInteractionHandler

PasswordInfobarBannerInteractionHandler::
    PasswordInfobarBannerInteractionHandler(
        const OverlayRequestSupport* request_support)
    : InfobarBannerInteractionHandler(request_support) {}

PasswordInfobarBannerInteractionHandler::
    ~PasswordInfobarBannerInteractionHandler() = default;

void PasswordInfobarBannerInteractionHandler::BannerVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  if (visible) {
    GetInfobarDelegate(infobar)->InfobarPresenting(/*automatic=*/YES);
  } else {
    GetInfobarDelegate(infobar)->InfobarDismissed();
  }
}

void PasswordInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  infobar->set_accepted(GetInfobarDelegate(infobar)->Accept());
}
#pragma mark - Private

IOSChromeSavePasswordInfoBarDelegate*
PasswordInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  IOSChromeSavePasswordInfoBarDelegate* delegate =
      IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
          infobar->delegate());
  DCHECK(delegate);
  return delegate;
}

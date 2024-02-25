// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/confirm/confirm_infobar_banner_interaction_handler.h"

#import "base/notreached.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"

using confirm_infobar_overlays::ConfirmBannerRequestConfig;

#pragma mark - InfobarBannerInteractionHandler

ConfirmInfobarBannerInteractionHandler::ConfirmInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          ConfirmBannerRequestConfig::RequestSupport()) {}

ConfirmInfobarBannerInteractionHandler::
    ~ConfirmInfobarBannerInteractionHandler() = default;

void ConfirmInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  // Confirm Infobars don't need to update badge status.
  GetInfobarDelegate(infobar)->Accept();
}

void ConfirmInfobarBannerInteractionHandler::BannerVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  if (!visible)
    GetInfobarDelegate(infobar)->InfoBarDismissed();
}

#pragma mark - Private

ConfirmInfoBarDelegate*
ConfirmInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);
  return delegate;
}

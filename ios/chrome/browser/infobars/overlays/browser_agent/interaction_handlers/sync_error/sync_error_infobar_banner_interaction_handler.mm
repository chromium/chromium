// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/sync_error/sync_error_infobar_banner_interaction_handler.h"

#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/sync_error_infobar_banner_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - InfobarBannerInteractionHandler

SyncErrorInfobarBannerInteractionHandler::
    SyncErrorInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          sync_error_infobar_overlays::SyncErrorBannerRequestConfig::
              RequestSupport()) {}

SyncErrorInfobarBannerInteractionHandler::
    ~SyncErrorInfobarBannerInteractionHandler() = default;

// Handle the main button tapped action.
void SyncErrorInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  GetInfobarDelegate(infobar)->Accept();
}

// Dismiss the banner if visibility changed to false.
void SyncErrorInfobarBannerInteractionHandler::BannerVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  if (!visible) {
    GetInfobarDelegate(infobar)->InfoBarDismissed();
  }
}

#pragma mark - Private

ConfirmInfoBarDelegate*
SyncErrorInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);
  return delegate;
}

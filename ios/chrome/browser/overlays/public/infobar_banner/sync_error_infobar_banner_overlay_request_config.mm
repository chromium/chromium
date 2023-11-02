// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/sync_error_infobar_banner_overlay_request_config.h"

#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_error_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sync_error_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SyncErrorBannerRequestConfig);

SyncErrorBannerRequestConfig::SyncErrorBannerRequestConfig(
    infobars::InfoBar* infobar)
    : ConfirmBannerRequestConfigStorage(infobar) {
  SyncErrorInfoBarDelegate* delegate =
      static_cast<SyncErrorInfoBarDelegate*>(this->infobar()->delegate());
  icon_image_tint_color_ = delegate->GetIconImageTintColor();
  icon_background_color_ = delegate->GetIconBackgroundColor();
}

SyncErrorBannerRequestConfig::~SyncErrorBannerRequestConfig() = default;

void SyncErrorBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar()),
      InfobarOverlayType::kBanner, is_high_priority());
}

}  // namespace sync_error_infobar_overlays

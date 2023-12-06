// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"

#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"

namespace confirm_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(ConfirmBannerRequestConfig);

ConfirmBannerRequestConfig::ConfirmBannerRequestConfig(
    infobars::InfoBar* infobar)
    : ConfirmBannerRequestConfigStorage(infobar) {}

ConfirmBannerRequestConfig::~ConfirmBannerRequestConfig() = default;

void ConfirmBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar()),
      InfobarOverlayType::kBanner, is_high_priority());
}

}  // namespace confirm_infobar_overlays

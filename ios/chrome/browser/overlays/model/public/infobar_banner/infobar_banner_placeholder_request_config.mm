// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_placeholder_request_config.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"

OVERLAY_USER_DATA_SETUP_IMPL(InfobarBannerPlaceholderRequestConfig);

InfobarBannerPlaceholderRequestConfig::InfobarBannerPlaceholderRequestConfig(
    infobars::InfoBar* infobar)
    : infobar_(infobar) {}

InfobarBannerPlaceholderRequestConfig::
    ~InfobarBannerPlaceholderRequestConfig() = default;

void InfobarBannerPlaceholderRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  PlaceholderRequestConfig::CreateForUserData(user_data);
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

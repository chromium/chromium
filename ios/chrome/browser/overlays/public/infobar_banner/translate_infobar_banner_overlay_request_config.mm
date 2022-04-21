// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"

#include "components/infobars/core/infobar.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The name of the icon image for the save passwords banner.
NSString* const kIconImageName = @"infobar_translate_icon";
}

namespace translate_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(TranslateBannerRequestConfig);

TranslateBannerRequestConfig::TranslateBannerRequestConfig(
    infobars::InfoBar* infobar)
    : infobar_(infobar), icon_image_name_(kIconImageName) {
  DCHECK(infobar_);
  translate::TranslateInfoBarDelegate* delegate =
      static_cast<translate::TranslateInfoBarDelegate*>(infobar_->delegate());
  source_language_ = delegate->source_language_name();
  target_language_ = delegate->target_language_name();
  translate_step_ = delegate->translate_step();
}

TranslateBannerRequestConfig::~TranslateBannerRequestConfig() = default;

void TranslateBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false, true);
}

}  // namespace translate_infobar_overlays

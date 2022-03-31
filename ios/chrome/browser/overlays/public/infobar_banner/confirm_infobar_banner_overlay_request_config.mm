// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#include "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace confirm_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(ConfirmBannerRequestConfig);

ConfirmBannerRequestConfig::ConfirmBannerRequestConfig(
    infobars::InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  ConfirmInfoBarDelegate* delegate =
      static_cast<ConfirmInfoBarDelegate*>(infobar_->delegate());
  title_text_ = delegate->GetTitleText();
  message_text_ = delegate->GetMessageText();
  button_label_text_ =
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK);
  icon_image_ = gfx::Image(delegate->GetIcon().Rasterize(nullptr));
  is_high_priority_ = static_cast<InfoBarIOS*>(infobar)->high_priority();
  use_icon_background_tint_ = delegate->UseIconBackgroundTint();
}

ConfirmBannerRequestConfig::~ConfirmBannerRequestConfig() = default;

void ConfirmBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, is_high_priority_);
}

}  // namespace confirm_infobar_overlays

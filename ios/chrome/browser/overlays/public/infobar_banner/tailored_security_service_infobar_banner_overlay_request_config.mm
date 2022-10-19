// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tailored_security_service_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(TailoredSecurityServiceBannerRequestConfig);

TailoredSecurityServiceBannerRequestConfig::
    TailoredSecurityServiceBannerRequestConfig(infobars::InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  safe_browsing::TailoredSecurityServiceInfobarDelegate* delegate =
      safe_browsing::TailoredSecurityServiceInfobarDelegate::
          FromInfobarDelegate(infobar_->delegate());
  message_text_ = delegate->GetMessageText();
  button_label_text_ = delegate->GetMessageActionText();
  description_ = delegate->GetDescription();
  message_state_ = delegate->message_state();
}

TailoredSecurityServiceBannerRequestConfig::
    ~TailoredSecurityServiceBannerRequestConfig() = default;

void TailoredSecurityServiceBannerRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

}  // namespace tailored_security_service_infobar_overlays

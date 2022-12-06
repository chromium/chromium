// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;

OVERLAY_USER_DATA_SETUP_IMPL(PasswordInfobarBannerOverlayRequestConfig);

PasswordInfobarBannerOverlayRequestConfig::
    PasswordInfobarBannerOverlayRequestConfig(InfoBar* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  IOSChromeSavePasswordInfoBarDelegate* delegate =
      IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
          infobar_->delegate());
  message_ = base::SysUTF16ToNSString(delegate->GetMessageText());
  username_ = delegate->GetUserNameText();
  button_text_ = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  password_length_ = delegate->GetPasswordText().length;
}

PasswordInfobarBannerOverlayRequestConfig::
    ~PasswordInfobarBannerOverlayRequestConfig() = default;

void PasswordInfobarBannerOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

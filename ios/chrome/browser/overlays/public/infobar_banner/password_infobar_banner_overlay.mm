// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
  title_ = base::SysUTF16ToNSString(delegate->GetMessageText());
  if (base::FeatureList::IsEnabled(
          password_manager::features::kIOSShowPasswordStorageInSaveInfobar)) {
    absl::optional<std::string> account_string =
        delegate->GetAccountToStorePassword();
    subtitle_ = account_string
                    ? l10n_util::GetNSStringF(
                          IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                          base::UTF8ToUTF16(*account_string))
                    : l10n_util::GetNSString(
                          IDS_IOS_PASSWORD_MANAGER_LOCAL_SAVE_SUBTITLE);
  } else {
    NSString* username = delegate->GetUserNameText();
    NSString* password =
        [@"" stringByPaddingToLength:delegate->GetPasswordText().length
                          withString:@"•"
                     startingAtIndex:0];
    subtitle_ = [NSString stringWithFormat:@"%@ %@", username, password];
    custom_accessibility_label_ =
        [NSString stringWithFormat:@"%@,%@, %@", title_, username,
                                   l10n_util::GetNSString(
                                       IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL)];
  }
  button_text_ = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
}

PasswordInfobarBannerOverlayRequestConfig::
    ~PasswordInfobarBannerOverlayRequestConfig() = default;

void PasswordInfobarBannerOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, static_cast<InfoBarIOS*>(infobar_),
      InfobarOverlayType::kBanner, false);
}

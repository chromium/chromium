// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_modal::PasswordAction;

OVERLAY_USER_DATA_SETUP_IMPL(PasswordInfobarModalOverlayRequestConfig);

PasswordInfobarModalOverlayRequestConfig::
    PasswordInfobarModalOverlayRequestConfig(InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  IOSChromeSavePasswordInfoBarDelegate* delegate =
      IOSChromeSavePasswordInfoBarDelegate::FromInfobarDelegate(
          infobar_->delegate());
  action_ = delegate->IsPasswordUpdate() ? PasswordAction::kUpdate
                                         : PasswordAction::kSave;
  username_ = delegate->GetUserNameText();
  password_ = delegate->GetPasswordText();
  absl::optional<std::string> account_string =
      delegate->GetAccountToStorePassword();
  details_text_ =
      account_string
          ? l10n_util::GetNSStringF(
                IDS_SAVE_PASSWORD_FOOTER_DISPLAYING_USER_EMAIL,
                base::UTF8ToUTF16(*account_string))
          : l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING);

  save_button_text_ = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  cancel_button_text_ = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  url_ = delegate->GetURLHostText();
  is_current_password_saved_ = delegate->IsCurrentPasswordSaved();
}

PasswordInfobarModalOverlayRequestConfig::
    ~PasswordInfobarModalOverlayRequestConfig() = default;

void PasswordInfobarModalOverlayRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

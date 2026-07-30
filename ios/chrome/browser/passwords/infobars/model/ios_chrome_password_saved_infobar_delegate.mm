// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/infobars/model/ios_chrome_password_saved_infobar_delegate.h"

#import <utility>

#import "base/check.h"
#import "base/not_fatal_until.h"
#import "base/strings/utf_string_conversions.h"
#import "build/build_config.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

IOSChromePasswordSavedInfoBarDelegate::IOSChromePasswordSavedInfoBarDelegate(
    std::u16string account_to_store_password,
    id<SettingsCommands> settings_commands_handler,
    password_manager::CredentialUIEntry password)
    : account_to_store_password_(std::move(account_to_store_password)),
      settings_commands_handler_(settings_commands_handler),
      password_(std::move(password)) {
  CHECK(!account_to_store_password_.empty(), base::NotFatalUntil::M160);
}

IOSChromePasswordSavedInfoBarDelegate::
    ~IOSChromePasswordSavedInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
IOSChromePasswordSavedInfoBarDelegate::GetIdentifier() const {
  return PASSWORD_SAVED_INFOBAR_DELEGATE_IOS;
}

std::u16string IOSChromePasswordSavedInfoBarDelegate::GetTitleText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_SAVED_INFOBAR_TITLE);
}

std::u16string IOSChromePasswordSavedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_IOS_PASSWORD_SAVED_INFOBAR_MESSAGE,
                                    account_to_store_password_);
}

int IOSChromePasswordSavedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string IOSChromePasswordSavedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_SAVED_INFOBAR_VIEW_BUTTON);
}

ui::ImageModel IOSChromePasswordSavedInfoBarDelegate::GetIcon() const {
  UIImage* image =
#if BUILDFLAG(IS_IOS_MACCATALYST)
      SymbolWithPointSize(SymbolPassword, kInfobarSymbolPointSize);
#else
      MakeSymbolMulticolor(SymbolWithPointSize(SymbolMulticolorPassword,
                                               kInfobarSymbolPointSize));
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  return ui::ImageModel::FromImage(gfx::Image(image));
}

bool IOSChromePasswordSavedInfoBarDelegate::Accept() {
  if (!password_.has_value()) {
    return false;
  }

  [settings_commands_handler_
      showPasswordDetailsForCredential:*std::move(password_)
                            inEditMode:NO];
  password_ = std::nullopt;
  return true;
}

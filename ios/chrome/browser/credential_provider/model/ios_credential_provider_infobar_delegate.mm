// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"

#import <utility>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

// static
std::unique_ptr<IOSCredentialProviderInfoBarDelegate>
IOSCredentialProviderInfoBarDelegate::Create(
    std::string account_string,
    sync_pb::WebauthnCredentialSpecifics passkey,
    id<SettingsCommands> settings_handler) {
  return std::make_unique<IOSCredentialProviderInfoBarDelegate>(
      std::move(account_string), std::move(passkey), settings_handler);
}

IOSCredentialProviderInfoBarDelegate::IOSCredentialProviderInfoBarDelegate(
    std::string account_string,
    sync_pb::WebauthnCredentialSpecifics passkey,
    id<SettingsCommands> settings_handler)
    : account_string_(std::move(account_string)),
      passkey_(std::move(passkey)),
      settings_handler_(settings_handler) {}

IOSCredentialProviderInfoBarDelegate::~IOSCredentialProviderInfoBarDelegate() =
    default;

infobars::InfoBarDelegate::InfoBarIdentifier
IOSCredentialProviderInfoBarDelegate::GetIdentifier() const {
  return CREDENTIAL_PROVIDER_INFOBAR_DELEGATE_IOS;
}

std::u16string IOSCredentialProviderInfoBarDelegate::GetTitleText() const {
  return l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_SAVED);
}

std::u16string IOSCredentialProviderInfoBarDelegate::GetMessageText() const {
  return base::SysNSStringToUTF16(
      l10n_util::GetNSStringF(IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                              base::UTF8ToUTF16(account_string_)));
}

ui::ImageModel IOSCredentialProviderInfoBarDelegate::GetIcon() const {
  UIImage* image =
#if BUILDFLAG(IS_IOS_MACCATALYST)
      CustomSymbolWithPointSize(kPasswordSymbol, kInfobarSymbolPointSize);
#else
      MakeSymbolMulticolor(CustomSymbolWithPointSize(kMulticolorPasswordSymbol,
                                                     kInfobarSymbolPointSize));
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  return ui::ImageModel::FromImage(gfx::Image(image));
}

bool IOSCredentialProviderInfoBarDelegate::UseIconBackgroundTint() const {
  return false;
}

std::u16string IOSCredentialProviderInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_VIEW_PASSKEY);
}

bool IOSCredentialProviderInfoBarDelegate::Accept() {
  ShowPasskeyDetails();
  return true;
}

void IOSCredentialProviderInfoBarDelegate::ShowPasskeyDetails() const {
  std::vector<sync_pb::WebauthnCredentialSpecifics> specifics({passkey_});
  std::vector<password_manager::PasskeyCredential> passkeyCredentials =
      password_manager::PasskeyCredential::FromCredentialSpecifics(specifics);
  if (passkeyCredentials.empty()) {
    return;
  }

  password_manager::CredentialUIEntry credential(passkeyCredentials[0]);

  // Attempting to show the passkey details right away can result in a race
  // condition between the reauthentication module and the infobar. Dispatching
  // this ensures it runs after the infobar animation completes.
  dispatch_async(dispatch_get_main_queue(), ^{
    [settings_handler_ showPasswordDetailsForCredential:credential
                                             inEditMode:NO];
  });
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"

#import <utility>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

// static
std::unique_ptr<IOSCredentialProviderInfoBarDelegate>
IOSCredentialProviderInfoBarDelegate::Create(std::string account_string) {
  return std::make_unique<IOSCredentialProviderInfoBarDelegate>(
      std::move(account_string));
}

IOSCredentialProviderInfoBarDelegate::IOSCredentialProviderInfoBarDelegate(
    std::string account_string)
    : account_string_(std::move(account_string)) {}

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

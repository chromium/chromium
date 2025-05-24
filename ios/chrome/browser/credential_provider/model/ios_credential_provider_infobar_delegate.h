// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_IOS_CREDENTIAL_PROVIDER_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_IOS_CREDENTIAL_PROVIDER_INFOBAR_DELEGATE_H_

#import <CoreFoundation/CoreFoundation.h>

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ui/base/models/image_model.h"

@protocol SettingsCommands;

class IOSCredentialProviderInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static std::unique_ptr<IOSCredentialProviderInfoBarDelegate> Create(
      std::string account_string,
      sync_pb::WebauthnCredentialSpecifics passkey,
      id<SettingsCommands> settings_handler);

  explicit IOSCredentialProviderInfoBarDelegate(
      std::string account_string,
      sync_pb::WebauthnCredentialSpecifics passkey,
      id<SettingsCommands> settings_handler);

  IOSCredentialProviderInfoBarDelegate(
      const IOSCredentialProviderInfoBarDelegate&) = delete;
  IOSCredentialProviderInfoBarDelegate& operator=(
      const IOSCredentialProviderInfoBarDelegate&) = delete;

  ~IOSCredentialProviderInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  ui::ImageModel GetIcon() const override;
  bool UseIconBackgroundTint() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

 private:
  void ShowPasskeyDetails() const;

  // User account identification string.
  std::string account_string_;

  // Passkey which triggered the inforbar.
  sync_pb::WebauthnCredentialSpecifics passkey_;

  // Settings handler to open the passkey details menu.
  __weak id<SettingsCommands> settings_handler_;
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_IOS_CREDENTIAL_PROVIDER_INFOBAR_DELEGATE_H_

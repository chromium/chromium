// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/test/mock_ios_chrome_save_passwords_infobar_delegate.h"

#import <optional>

#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

namespace {
std::unique_ptr<password_manager::PasswordFormManagerForUI> CreateFormManager(
    password_manager::PasswordForm* form,
    GURL* url) {
  std::unique_ptr<password_manager::MockPasswordFormManagerForUI> form_manager =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();
  EXPECT_CALL(*form_manager, GetPendingCredentials())
      .WillRepeatedly(testing::ReturnRef(*form));
  EXPECT_CALL(*form_manager, GetURL()).WillRepeatedly(testing::ReturnRef(*url));
  EXPECT_CALL(*form_manager, GetMetricsRecorder())
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(*form_manager, GetCredentialSource())
      .WillRepeatedly(testing::Return(
          password_manager::metrics_util::CredentialSourceType::kUnknown));
  return form_manager;
}
}  // namespace

// static
std::unique_ptr<MockIOSChromeSavePasswordInfoBarDelegate>
MockIOSChromeSavePasswordInfoBarDelegate::Create(
    NSString* username,
    NSString* password,
    const GURL& url,
    std::optional<std::string> account_to_store_password) {
  std::unique_ptr<password_manager::PasswordForm> form =
      std::make_unique<password_manager::PasswordForm>();
  form->username_value = base::SysNSStringToUTF16(username);
  form->password_value = base::SysNSStringToUTF16(password);
  return base::WrapUnique(new MockIOSChromeSavePasswordInfoBarDelegate(
      std::move(form), std::make_unique<GURL>(url), account_to_store_password));
}

MockIOSChromeSavePasswordInfoBarDelegate::
    MockIOSChromeSavePasswordInfoBarDelegate(
        std::unique_ptr<password_manager::PasswordForm> form,
        std::unique_ptr<GURL> url,
        std::optional<std::string> account_to_store_password)
    : IOSChromeSavePasswordInfoBarDelegate(
          account_to_store_password,
          /*password_update=*/false,
          account_to_store_password.has_value()
              ? password_manager::features_util::
                    PasswordAccountStorageUserState::kSyncUser
              : password_manager::features_util::
                    PasswordAccountStorageUserState::kSignedOutUser,
          CreateFormManager(form.get(), url.get()),
          [[CommandDispatcher alloc] init]),
      form_(std::move(form)),
      url_(std::move(url)) {}

MockIOSChromeSavePasswordInfoBarDelegate::
    ~MockIOSChromeSavePasswordInfoBarDelegate() = default;

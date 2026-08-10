// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/infobars/test/mock_ios_chrome_save_passwords_infobar_delegate.h"

#import <optional>

#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/sync/test/test_sync_service.h"

namespace {

using ::testing::Return;
using ::testing::ReturnRef;

std::unique_ptr<password_manager::MockPasswordFormManagerForUI>
CreateFormManager(
    password_manager::PasswordForm* form,
    GURL* url,
    password_manager::PasswordFormMetricsRecorder* metrics_recorder) {
  std::unique_ptr<password_manager::MockPasswordFormManagerForUI> form_manager =
      std::make_unique<password_manager::MockPasswordFormManagerForUI>();
  EXPECT_CALL(*form_manager, GetPendingCredentials())
      .WillRepeatedly(ReturnRef(*form));
  EXPECT_CALL(*form_manager, GetURL()).WillRepeatedly(ReturnRef(*url));
  EXPECT_CALL(*form_manager, GetMetricsRecorder())
      .WillRepeatedly(Return(metrics_recorder));
  EXPECT_CALL(*form_manager, GetCredentialSource())
      .WillRepeatedly(Return(
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
    const syncer::SyncService* sync_service,
    password_manager::PasswordFormMetricsRecorder* metrics_recorder) {
  std::unique_ptr<password_manager::PasswordForm> form =
      std::make_unique<password_manager::PasswordForm>();
  form->username_value = base::SysNSStringToUTF16(username);
  form->password_value = base::SysNSStringToUTF16(password);
  auto url_ptr = std::make_unique<GURL>(url);
  std::unique_ptr<password_manager::MockPasswordFormManagerForUI> form_manager =
      CreateFormManager(form.get(), url_ptr.get(), metrics_recorder);
  password_manager::MockPasswordFormManagerForUI* mock_form_manager_ptr =
      form_manager.get();
  return base::WrapUnique(new MockIOSChromeSavePasswordInfoBarDelegate(
      std::move(form), std::move(url_ptr), sync_service,
      std::move(form_manager), mock_form_manager_ptr));
}

MockIOSChromeSavePasswordInfoBarDelegate::
    MockIOSChromeSavePasswordInfoBarDelegate(
        std::unique_ptr<password_manager::PasswordForm> form,
        std::unique_ptr<GURL> url,
        const syncer::SyncService* sync_service,
        std::unique_ptr<password_manager::MockPasswordFormManagerForUI>
            form_manager,
        password_manager::MockPasswordFormManagerForUI* mock_form_manager_ptr)
    : IOSChromeSavePasswordInfoBarDelegate(
          /*password_update=*/false,
          std::move(form_manager),
          ukm::kInvalidSourceId,
          /*is_replacement=*/false,
          /*sync_presenter_handler=*/nil,
          /*settings_commands_handler=*/nil,
          /*profile_store=*/nullptr,
          /*account_store=*/nullptr,
          sync_service),
      form_(std::move(form)),
      url_(std::move(url)),
      mock_form_manager_(mock_form_manager_ptr) {}

MockIOSChromeSavePasswordInfoBarDelegate::
    ~MockIOSChromeSavePasswordInfoBarDelegate() = default;

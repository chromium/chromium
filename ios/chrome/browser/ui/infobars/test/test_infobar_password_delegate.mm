// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/test/test_infobar_password_delegate.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/password_manager/core/browser/credential_manager_password_form_manager.h"
#import "components/password_manager/core/browser/fake_form_fetcher.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_form_manager_for_ui.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/stub_form_saver.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "testing/gmock/include/gmock/gmock.h"

using password_manager::PasswordForm;
using base::ASCIIToUTF16;

namespace {

class MockDelegate
    : public password_manager::CredentialManagerPasswordFormManagerDelegate {
 public:
  MOCK_METHOD0(OnProvisionalSaveComplete, void());
};

class MockFormSaver : public password_manager::StubFormSaver {
 public:
  MockFormSaver() = default;

  MockFormSaver(const MockFormSaver&) = delete;
  MockFormSaver& operator=(const MockFormSaver&) = delete;

  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD3(
      Save,
      void(PasswordForm pending,
           const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
               matches,
           const std::u16string& old_password));
  MOCK_METHOD3(
      Update,
      void(PasswordForm pending,
           const std::vector<raw_ptr<const PasswordForm, VectorExperimental>>&
               matches,
           const std::u16string& old_password));

  // Convenience downcasting method.
  static MockFormSaver& Get(
      password_manager::PasswordFormManager* form_manager) {
    return *static_cast<MockFormSaver*>(
        form_manager->profile_store_form_saver());
  }
};

std::unique_ptr<password_manager::CredentialManagerPasswordFormManager>
CreateFormManager() {
  PasswordForm form_to_save;
  form_to_save.url = GURL("https://example.com/path");
  form_to_save.signon_realm = "https://example.com/";
  form_to_save.username_value = u"user1";
  form_to_save.password_value = u"pass1";
  form_to_save.scheme = PasswordForm::Scheme::kHtml;
  form_to_save.type = PasswordForm::Type::kApi;
  MockDelegate delegate;
  password_manager::StubPasswordManagerClient client;

  std::unique_ptr<password_manager::FakeFormFetcher> fetcher(
      new password_manager::FakeFormFetcher());
  std::unique_ptr<MockFormSaver> saver(new MockFormSaver());
  return std::make_unique<
      password_manager::CredentialManagerPasswordFormManager>(
      &client, std::make_unique<PasswordForm>(form_to_save), &delegate,
      std::make_unique<MockFormSaver>(),
      std::make_unique<password_manager::FakeFormFetcher>());
}

}  // namespace

TestInfobarPasswordDelegate::TestInfobarPasswordDelegate(
    NSString* infobar_message)
    : IOSChromeSavePasswordInfoBarDelegate(
          "foobar@gmail.com",
          false,
          password_manager::features_util::PasswordAccountStorageUserState::
              kSyncUser,
          CreateFormManager(),
          nullptr),
      infobar_message_(infobar_message) {}

bool TestInfobarPasswordDelegate::Create(
    infobars::InfoBarManager* infobar_manager) {
  DCHECK(infobar_manager);
  return !!infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(this)));
}

TestInfobarPasswordDelegate::InfoBarIdentifier
TestInfobarPasswordDelegate::GetIdentifier() const {
  return TEST_INFOBAR;
}

std::u16string TestInfobarPasswordDelegate::GetMessageText() const {
  return base::SysNSStringToUTF16(infobar_message_);
}

int TestInfobarPasswordDelegate::GetButtons() const {
  return ConfirmInfoBarDelegate::BUTTON_OK;
}

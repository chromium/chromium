// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"

#import <cstdint>
#import <memory>
#import <string>
#import <string_view>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/run_until.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ios/chrome/browser/credential_provider/model/fake_credential_provider_settings_handler.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr std::string_view kAccountString = "foo@gmail.com";
constexpr std::string_view kRpId = "example.com";
constexpr std::string_view kCredentialId = "credential_id_1234";

// Test fixture for IOSCredentialProviderInfoBarDelegateTest.
class IOSCredentialProviderInfoBarDelegateTest : public PlatformTest {
 public:
  void SetUp() override {
    account_string_ = std::string(kAccountString);
    passkey_.set_rp_id(kRpId);
    passkey_.set_credential_id(kCredentialId);
    passkey_.set_user_id("user_id");
    passkey_.set_user_name("user@example.com");
    passkey_.set_user_display_name("User");

    command_dispatcher_ = [[CommandDispatcher alloc] init];
    settings_handler_ = [[FakeCredentialProviderSettingsHandler alloc] init];
    settings_commands_handler_ =
        [settings_handler_ registerWithCommandDispatcher:command_dispatcher_];
    delegate_ = std::make_unique<IOSCredentialProviderInfoBarDelegate>(
        account_string_, passkey_, settings_commands_handler_);

    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::string account_string_;
  sync_pb::WebauthnCredentialSpecifics passkey_;
  CommandDispatcher* command_dispatcher_ = nil;
  FakeCredentialProviderSettingsHandler* settings_handler_ = nil;
  id<SettingsCommands> settings_commands_handler_ = nil;
  // Infobar delegate to test.
  std::unique_ptr<IOSCredentialProviderInfoBarDelegate> delegate_;
  web::FakeWebState web_state_;
};

TEST_F(IOSCredentialProviderInfoBarDelegateTest, GetTitleText) {
  std::u16string titleText =
      l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_SAVED);
  EXPECT_EQ(titleText, delegate_->GetTitleText());
}

TEST_F(IOSCredentialProviderInfoBarDelegateTest, GetMessageText) {
  std::u16string messageText = base::SysNSStringToUTF16(
      l10n_util::GetNSStringF(IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                              base::UTF8ToUTF16(account_string_)));
  EXPECT_EQ(messageText, delegate_->GetMessageText());
}

// Tests that `DisplayForWebState` adds a confirmation infobar when `web_state`
// is visible, and accepting the infobar dispatches `SettingsCommands` with the
// saved passkey details.
TEST_F(IOSCredentialProviderInfoBarDelegateTest,
       DisplayForVisibleWebStateAndAcceptDispatchesSettingsCommand) {
  web_state_.WasShown();

  EXPECT_TRUE(IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
      &web_state_, account_string_, passkey_, settings_commands_handler_));

  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(&web_state_);
  ASSERT_EQ(1u, infobar_manager->infobars().size());
  ConfirmInfoBarDelegate* confirm_delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_delegate);
  EXPECT_EQ(infobars::InfoBarDelegate::CREDENTIAL_PROVIDER_INFOBAR_DELEGATE_IOS,
            confirm_delegate->GetIdentifier());
  EXPECT_EQ(delegate_->GetMessageText(), confirm_delegate->GetMessageText());

  EXPECT_EQ(0u, settings_handler_.showPasswordDetailsCallCount);
  EXPECT_TRUE(confirm_delegate->Accept());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return settings_handler_.showPasswordDetailsCallCount == 1u; }));

  EXPECT_FALSE(settings_handler_.calledInEditMode);
  ASSERT_TRUE(settings_handler_.lastCredential.has_value());
  ASSERT_TRUE(settings_handler_.lastCredential->IsPasskey());
  EXPECT_EQ(kRpId, settings_handler_.lastCredential->passkey_info->rp_id);
  std::vector<uint8_t> expected_credential_id(kCredentialId.begin(),
                                              kCredentialId.end());
  EXPECT_EQ(expected_credential_id,
            settings_handler_.lastCredential->passkey_info->credential_id);
}

// Tests that `DisplayForWebState` does not add an infobar when `web_state` is
// hidden.
TEST_F(IOSCredentialProviderInfoBarDelegateTest, DisplayForHiddenWebStateNoOp) {
  EXPECT_FALSE(IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
      &web_state_, account_string_, passkey_, settings_commands_handler_));

  EXPECT_EQ(0u,
            InfoBarManagerImpl::FromWebState(&web_state_)->infobars().size());
}

// Tests that `DisplayForWebState` returns false without crashing when
// `web_state` is null, lacks an `InfoBarManagerImpl`, or receives an empty
// `account_string`.
TEST_F(IOSCredentialProviderInfoBarDelegateTest,
       DisplayForNullOrUnattachedWebStateOrEmptyAccountNoOp) {
  EXPECT_FALSE(IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
      /*web_state=*/nullptr, account_string_, passkey_,
      settings_commands_handler_));

  web::FakeWebState unattached_web_state;
  unattached_web_state.WasShown();
  EXPECT_FALSE(IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
      &unattached_web_state, account_string_, passkey_,
      settings_commands_handler_));

  web_state_.WasShown();
  EXPECT_FALSE(IOSCredentialProviderInfoBarDelegate::DisplayForWebState(
      &web_state_, /*account_string=*/"", passkey_,
      settings_commands_handler_));
  EXPECT_EQ(0u,
            InfoBarManagerImpl::FromWebState(&web_state_)->infobars().size());
}

}  // namespace

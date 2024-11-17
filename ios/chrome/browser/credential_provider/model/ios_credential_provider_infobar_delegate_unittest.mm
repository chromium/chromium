// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/ios_credential_provider_infobar_delegate.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

static constexpr char kAccountString[] = "foo@gmail.com";

// Test fixture for IOSCredentialProviderInfoBarDelegateTest.
class IOSCredentialProviderInfoBarDelegateTest : public PlatformTest {
 public:
  void SetUp() override {
    account_string_ = kAccountString;
    delegate_ = std::make_unique<IOSCredentialProviderInfoBarDelegate>(
        account_string_, passkey_, /*settings_handler=*/nil);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::string account_string_;
  sync_pb::WebauthnCredentialSpecifics passkey_;
  // Infobar delegate to test.
  std::unique_ptr<IOSCredentialProviderInfoBarDelegate> delegate_;
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

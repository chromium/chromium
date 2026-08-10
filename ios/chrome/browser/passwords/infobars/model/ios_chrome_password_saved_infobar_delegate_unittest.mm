// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/infobars/model/ios_chrome_password_saved_infobar_delegate.h"

#import <string_view>

#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

@interface FakeSettingsCommands : NSObject
@property(nonatomic, assign) BOOL wasCalled;
@end

@implementation FakeSettingsCommands

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode {
  self.wasCalled = YES;
}

@end

namespace {

constexpr char16_t kTestAccount[] = u"user@example.com";

class IOSChromePasswordSavedInfoBarDelegateTest : public PlatformTest {
 protected:
  IOSChromePasswordSavedInfoBarDelegateTest() {
    settings_commands_handler_ = [[FakeSettingsCommands alloc] init];
    delegate_ = std::make_unique<IOSChromePasswordSavedInfoBarDelegate>(
        kTestAccount, (id<SettingsCommands>)settings_commands_handler_,
        password_manager::CredentialUIEntry());
  }

  FakeSettingsCommands* settings_commands_handler_;
  std::unique_ptr<IOSChromePasswordSavedInfoBarDelegate> delegate_;
};

// Tests that the infobar returns correct identifier.
TEST_F(IOSChromePasswordSavedInfoBarDelegateTest, Identifier) {
  EXPECT_EQ(delegate_->GetIdentifier(),
            infobars::InfoBarDelegate::PASSWORD_SAVED_INFOBAR_DELEGATE_IOS);
}

// Tests that the correct title is displayed.
TEST_F(IOSChromePasswordSavedInfoBarDelegateTest, TitleText) {
  EXPECT_EQ(delegate_->GetTitleText(),
            l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_SAVED_INFOBAR_TITLE));
}

// Tests that the correct message is displayed.
TEST_F(IOSChromePasswordSavedInfoBarDelegateTest, MessageText) {
  EXPECT_EQ(delegate_->GetMessageText(),
            l10n_util::GetStringFUTF16(IDS_IOS_PASSWORD_SAVED_INFOBAR_MESSAGE,
                                       kTestAccount));
}

// Tests that the correct buttons are displayed.
TEST_F(IOSChromePasswordSavedInfoBarDelegateTest, Buttons) {
  EXPECT_EQ(delegate_->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_EQ(
      delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
      l10n_util::GetStringUTF16(IDS_IOS_PASSWORD_SAVED_INFOBAR_VIEW_BUTTON));
}

// Tests that GetIcon returns a non-empty image model.
TEST_F(IOSChromePasswordSavedInfoBarDelegateTest, Icon) {
  EXPECT_FALSE(delegate_->GetIcon().IsEmpty());
}

// Tests that Accept() correctly routes to the settings commands handler.
TEST_F(IOSChromePasswordSavedInfoBarDelegateTest,
       AcceptTriggersSettingsCommand) {
  ASSERT_FALSE(settings_commands_handler_.wasCalled);
  EXPECT_TRUE(delegate_->Accept());
  EXPECT_TRUE(settings_commands_handler_.wasCalled);
  EXPECT_FALSE(delegate_->Accept());
}

}  // namespace

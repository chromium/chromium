// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/passwords/password_infobar_modal_overlay_mediator.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/passwords/model/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/ui/infobars/modals/test/fake_infobar_password_modal_consumer.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
// Consts used in tests.
const char kUrlHost[] = "chromium.test";
NSString* const kUsername = @"username";
NSString* const kPassword = @"password";
NSString* const kMaskedPassword = @"••••••••";
const char kAccount[] = "foobar@gmail.com";
}

// Test fixture for PasswordInfobarModalOverlayMediator.
class PasswordInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  PasswordInfobarModalOverlayMediatorTest()
      : delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {}

  ~PasswordInfobarModalOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_->delegate());
  }

  void InitInfobar(
      std::optional<std::string> account_to_store_password = std::nullopt) {
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(
            kUsername, kPassword, GURL(std::string("http://") + kUrlHost),
            account_to_store_password));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kModal);
    consumer_ = [[FakeInfobarPasswordModalConsumer alloc] init];
    mediator_ = [[PasswordInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = delegate_;
    mediator_.consumer = consumer_;
  }

 protected:
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  PasswordInfobarModalOverlayMediator* mediator_ = nil;
  FakeInfobarPasswordModalConsumer* consumer_;
};

// Tests that a PasswordInfobarModalOverlayMediator correctly sets up its
// consumer when the password is saved to an account.
TEST_F(PasswordInfobarModalOverlayMediatorTest, SetUpConsumerSavingToAccount) {
  InitInfobar(kAccount);

  EXPECT_NSEQ(kUsername, consumer_.username);
  EXPECT_NSEQ(kMaskedPassword, consumer_.maskedPassword);
  EXPECT_NSEQ(kPassword, consumer_.unmaskedPassword);
  EXPECT_NSEQ(
      l10n_util::GetNSStringF(IDS_SAVE_PASSWORD_FOOTER_DISPLAYING_USER_EMAIL,
                              base::UTF8ToUTF16(std::string(kAccount))),
      consumer_.detailsTextMessage);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kUrlHost), consumer_.URL);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
              consumer_.saveButtonText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_MODAL_BLOCK_BUTTON),
      consumer_.cancelButtonText);
  EXPECT_EQ(mock_delegate().IsCurrentPasswordSaved(),
            consumer_.currentCredentialsSaved);
}

// Tests that a PasswordInfobarModalOverlayMediator correctly sets up its
// consumer for when the password is saved only to the device.
TEST_F(PasswordInfobarModalOverlayMediatorTest, SetUpConsumerSavingLocally) {
  InitInfobar();

  EXPECT_NSEQ(kUsername, consumer_.username);
  EXPECT_NSEQ(kMaskedPassword, consumer_.maskedPassword);
  EXPECT_NSEQ(kPassword, consumer_.unmaskedPassword);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING),
              consumer_.detailsTextMessage);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kUrlHost), consumer_.URL);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON),
              consumer_.saveButtonText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_MODAL_BLOCK_BUTTON),
      consumer_.cancelButtonText);
  EXPECT_EQ(mock_delegate().IsCurrentPasswordSaved(),
            consumer_.currentCredentialsSaved);
}

// Tests that `-updateCredentialsWithUsername:password:` calls the
// `UpdateCredentials()` delegate method before accepting the infobar and
// dismissing the overlay.
TEST_F(PasswordInfobarModalOverlayMediatorTest, UpdateCredentials) {
  InitInfobar();

  EXPECT_CALL(mock_delegate(), UpdateCredentials(kUsername, kPassword));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ updateCredentialsWithUsername:kUsername password:kPassword];
}

// Tests that `-neverSaveCredentialsForCurrentSite` calls the `Cancel()`
// delegate method then stops the overlay.
TEST_F(PasswordInfobarModalOverlayMediatorTest, NeverSaveCredentials) {
  InitInfobar();

  EXPECT_CALL(mock_delegate(), Cancel());
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ neverSaveCredentialsForCurrentSite];
}

// Tests that `-presentPasswordSettings` calls the `Cancel()` delegate method
// then stops the overlay.
TEST_F(PasswordInfobarModalOverlayMediatorTest, PresentPasswordSettings) {
  InitInfobar();

  id commands_handler = OCMStrictProtocolMock(@protocol(SettingsCommands));
  [mock_delegate().GetDispatcher()
      startDispatchingToTarget:commands_handler
                   forProtocol:@protocol(SettingsCommands)];
  [[commands_handler expect] showSavedPasswordsSettingsFromViewController:nil
                                                         showCancelButton:YES];

  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);

  [mediator_ presentPasswordSettings];
  [commands_handler verify];
}

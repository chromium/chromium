// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_mediator.h"

#import "base/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/test/fake_infobar_password_modal_consumer.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_infobar_modal_responses::UpdateCredentialsInfo;
using password_infobar_modal_responses::NeverSaveCredentials;
using password_infobar_modal_responses::PresentPasswordSettings;

namespace {
// Consts used in tests.
const char kUrlSpec[] = "http://chromium.test";
NSString* const kUsername = @"username";
NSString* const kPassword = @"password";
NSString* const kMaskedPassword = @"••••••••";
}

// Test fixture for PasswordInfobarModalOverlayMediator.
class PasswordInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  PasswordInfobarModalOverlayMediatorTest()
      : url_(kUrlSpec),
        infobar_(InfobarType::kInfobarTypePasswordSave,
                 MockIOSChromeSavePasswordInfoBarDelegate::Create(kUsername,
                                                                  kPassword,
                                                                  url_)),
        callback_installer_(&callback_receiver_,
                            {InfobarModalMainActionResponse::ResponseSupport(),
                             NeverSaveCredentials::ResponseSupport(),
                             PresentPasswordSettings::ResponseSupport()}),
        delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    request_ = OverlayRequest::CreateWithConfig<
        PasswordInfobarModalOverlayRequestConfig>(&infobar_);
    callback_installer_.InstallCallbacks(request_.get());
    mediator_ = [[PasswordInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = delegate_;
  }

  ~PasswordInfobarModalOverlayMediatorTest() override {
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_.delegate());
  }

 protected:
  GURL url_;
  InfoBarIOS infobar_;
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  PasswordInfobarModalOverlayMediator* mediator_ = nil;
};

// Tests that a PasswordInfobarModalOverlayMediator correctly sets up its
// consumer.
TEST_F(PasswordInfobarModalOverlayMediatorTest, SetUpConsumer) {
  FakeInfobarPasswordModalConsumer* consumer =
      [[FakeInfobarPasswordModalConsumer alloc] init];
  mediator_.consumer = consumer;
  EXPECT_NSEQ(kUsername, consumer.username);
  EXPECT_NSEQ(kMaskedPassword, consumer.maskedPassword);
  EXPECT_NSEQ(kPassword, consumer.unmaskedPassword);
  EXPECT_NSEQ(mock_delegate().GetDetailsMessageText(),
              consumer.detailsTextMessage);
  EXPECT_NSEQ(mock_delegate().GetURLHostText(), consumer.URL);
  EXPECT_NSEQ(base::SysUTF16ToNSString(mock_delegate().GetButtonLabel(
                  ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer.saveButtonText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(mock_delegate().GetButtonLabel(
                  ConfirmInfoBarDelegate::BUTTON_CANCEL)),
              consumer.cancelButtonText);
  EXPECT_EQ(mock_delegate().IsCurrentPasswordSaved(),
            consumer.currentCredentialsSaved);
}

// Tests that `-updateCredentialsWithUsername:password:` dispatches an
// UpdateCredentials response before accepting the infobar and dismissing the
// overlay.
TEST_F(PasswordInfobarModalOverlayMediatorTest, UpdateCredentials) {
  // Since the UpdateCredentials response is not stateless, it is verified using
  // a block rather than the mock callback receiver.
  __block NSString* username = nil;
  __block NSString* password = nil;
  request_->GetCallbackManager()->AddDispatchCallback(
      OverlayDispatchCallback(base::BindRepeating(^(OverlayResponse* response) {
                                UpdateCredentialsInfo* info =
                                    response->GetInfo<UpdateCredentialsInfo>();
                                ASSERT_TRUE(info);
                                username = info->username();
                                password = info->password();
                              }),
                              UpdateCredentialsInfo::ResponseSupport()));
  // Notify the mediator to update the credentials.  In addition to dispatching
  // the UpdateCredentialsInfo response, this should also trigger the main
  // action and dismissal.
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(),
                       InfobarModalMainActionResponse::ResponseSupport()));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ updateCredentialsWithUsername:kUsername password:kPassword];
  // Verify that the update credentials callback was executed with the passed
  // username and password.
  EXPECT_NSEQ(kUsername, username);
  EXPECT_NSEQ(kPassword, password);
}

// Tests that `-neverSaveCredentialsForCurrentSite` dispatches a
// NeverSaveCredentials response then stops the overlay.
TEST_F(PasswordInfobarModalOverlayMediatorTest, NeverSaveCredentials) {
  EXPECT_CALL(callback_receiver_,
              DispatchCallback(request_.get(),
                               NeverSaveCredentials::ResponseSupport()));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ neverSaveCredentialsForCurrentSite];
}

// Tests that `-presentPasswordSettings` dispatches a PresentPasswordSettings
// response then stops the overlay.
TEST_F(PasswordInfobarModalOverlayMediatorTest, PresentPasswordSettings) {
  EXPECT_CALL(callback_receiver_,
              DispatchCallback(request_.get(),
                               PresentPasswordSettings::ResponseSupport()));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ presentPasswordSettings];
}

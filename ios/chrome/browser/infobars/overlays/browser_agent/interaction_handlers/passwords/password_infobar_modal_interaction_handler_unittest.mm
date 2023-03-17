// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_interaction_handler.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for PasswordInfobarModalInteractionHandler.
class PasswordInfobarModalInteractionHandlerTest : public PlatformTest {
 public:
  PasswordInfobarModalInteractionHandlerTest()
      : mock_command_receiver_(
            OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands))),
        infobar_(
            InfobarType::kInfobarTypePasswordSave,
            MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username",
                                                             @"password")) {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_command_receiver_
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    handler_ = std::make_unique<PasswordInfobarModalInteractionHandler>(
        browser_.get(), password_modal::PasswordAction::kSave);
  }
  ~PasswordInfobarModalInteractionHandlerTest() override {
    [browser_->GetCommandDispatcher()
        stopDispatchingToTarget:mock_command_receiver_];
    EXPECT_OCMOCK_VERIFY(mock_command_receiver_);
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_.delegate());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_command_receiver_ = nil;
  InfoBarIOS infobar_;
  std::unique_ptr<PasswordInfobarModalInteractionHandler> handler_;
};

// Tests that UpdateCredentials() forwards the call to the mock delegate.
TEST_F(PasswordInfobarModalInteractionHandlerTest, UpdateCredentials) {
  NSString* username = @"username";
  NSString* password = @"password";
  EXPECT_CALL(mock_delegate(), UpdateCredentials(username, password));
  handler_->UpdateCredentials(&infobar_, username, password);
}

// Tests that NeverSaveCredentials() forwards the call to the mock delegate.
TEST_F(PasswordInfobarModalInteractionHandlerTest, NeverSaveCredentials) {
  EXPECT_CALL(mock_delegate(), Cancel());
  handler_->NeverSaveCredentials(&infobar_);
}

// Tests that PresentPasswordsSettings() forwards the call to the mock delegate.
TEST_F(PasswordInfobarModalInteractionHandlerTest, PresentPasswordsSettings) {
  OCMExpect([mock_command_receiver_
      showSavedPasswordsSettingsFromViewController:nil
                                  showCancelButton:YES
                                startPasswordCheck:NO]);
  handler_->PresentPasswordsSettings(&infobar_);
}

// Tests PerformMainAction() calls Accept() on the mock delegate and resets
// the infobar to be accepted.
TEST_F(PasswordInfobarModalInteractionHandlerTest, MainAction) {
  ASSERT_FALSE(infobar_.accepted());
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  handler_->PerformMainAction(&infobar_);
  EXPECT_TRUE(infobar_.accepted());
}

// Tests that InfobarVisibilityChanged() calls InfobarPresenting() and
// InfobarDismissed() on the mock delegate.
TEST_F(PasswordInfobarModalInteractionHandlerTest, InfobarVisibilityChanged) {
  EXPECT_CALL(mock_delegate(), InfobarPresenting(/*automatic=*/false));
  handler_->InfobarVisibilityChanged(&infobar_, true);
  EXPECT_CALL(mock_delegate(), InfobarDismissed());
  handler_->InfobarVisibilityChanged(&infobar_, false);
}

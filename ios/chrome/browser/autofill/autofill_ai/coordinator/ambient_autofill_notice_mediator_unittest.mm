// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/ambient_autofill_notice_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AmbientAutofillNoticeMediatorTest : public PlatformTest {
 protected:
  AmbientAutofillNoticeMediatorTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();
    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    mock_autofill_commands_ = OCMProtocolMock(@protocol(AutofillCommands));

    autofill_client_ = std::make_unique<autofill::TestAutofillClientIOS>(
        web_state_.get(), nil);

    AutofillBottomSheetTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = AutofillBottomSheetTabHelper::FromWebState(web_state_.get());
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  id mock_autofill_commands_;
  std::unique_ptr<autofill::TestAutofillClientIOS> autofill_client_;
  std::unique_ptr<web::WebState> web_state_;
  raw_ptr<AutofillBottomSheetTabHelper> tab_helper_;
};

// Tests that acknowledging the notice triggers element refocus and dismisses
// the bottom sheet.
TEST_F(AmbientAutofillNoticeMediatorTest,
       AcknowledgeNoticeRefocusesAndDismisses) {
  autofill::FormActivityParams params;
  params.frame_id = "frame123";

  AmbientAutofillNoticeMediator* mediator =
      [[AmbientAutofillNoticeMediator alloc]
          initWithWebState:web_state_->GetWeakPtr()
                    params:params
           autofillHandler:mock_autofill_commands_];

  OCMExpect([mock_autofill_commands_ dismissAmbientAutofillNotice]);

  [mediator didAcknowledgeNotice];

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_);
}

// Tests that tapping the settings option dismisses the notice bottom sheet.
TEST_F(AmbientAutofillNoticeMediatorTest, TapSettingsDismisses) {
  autofill::FormActivityParams params;

  AmbientAutofillNoticeMediator* mediator =
      [[AmbientAutofillNoticeMediator alloc]
          initWithWebState:web_state_->GetWeakPtr()
                    params:params
           autofillHandler:mock_autofill_commands_];

  OCMExpect([mock_autofill_commands_ dismissAmbientAutofillNotice]);

  [mediator didTapSettings];

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_);
}

// Tests that swiping down to dismiss the notice sheet manual action dismisses
// it.
TEST_F(AmbientAutofillNoticeMediatorTest, SwipeDownDismisses) {
  autofill::FormActivityParams params;

  AmbientAutofillNoticeMediator* mediator =
      [[AmbientAutofillNoticeMediator alloc]
          initWithWebState:web_state_->GetWeakPtr()
                    params:params
           autofillHandler:mock_autofill_commands_];

  OCMExpect([mock_autofill_commands_ dismissAmbientAutofillNotice]);

  [mediator didDismissNotice];

  EXPECT_OCMOCK_VERIFY(mock_autofill_commands_);
}

// Tests that markNoticeShown successfully calls
// MarkPersonalContextAmbientAutofillNoticeAsAcknowledged on the client.
TEST_F(AmbientAutofillNoticeMediatorTest, MarkNoticeShownUpdatesClient) {
  autofill::FormActivityParams params;

  AmbientAutofillNoticeMediator* mediator =
      [[AmbientAutofillNoticeMediator alloc]
          initWithWebState:web_state_->GetWeakPtr()
                    params:params
           autofillHandler:mock_autofill_commands_];

  EXPECT_FALSE(
      autofill_client_
          ->is_personal_context_ambient_autofill_notice_acknowledged());

  [mediator markNoticeShown];

  EXPECT_TRUE(autofill_client_
                  ->is_personal_context_ambient_autofill_notice_acknowledged());
}

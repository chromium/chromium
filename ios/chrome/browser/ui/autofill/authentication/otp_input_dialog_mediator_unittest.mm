// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_consumer.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_content.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::CardUnmaskChallengeOption;

class OtpInputDialogMediatorTest : public PlatformTest {
 protected:
  OtpInputDialogMediatorTest() {
    consumer_ = OCMProtocolMock(@protocol(OtpInputDialogConsumer));
    CardUnmaskChallengeOption option = CardUnmaskChallengeOption(
        CardUnmaskChallengeOption::ChallengeOptionId("123"),
        autofill::CardUnmaskChallengeOptionType::kSmsOtp,
        /*challenge_info=*/u"xxx-xxx-3547",
        /*challenge_input_length=*/6U);
    model_controller_ =
        std::make_unique<autofill::CardUnmaskOtpInputDialogControllerImpl>(
            option, /*delegate=*/nullptr);
    mediator_ = std::make_unique<OtpInputDialogMediator>(
        model_controller_->GetImplWeakPtr());
  }

  id<OtpInputDialogConsumer> consumer_;
  std::unique_ptr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      model_controller_;
  std::unique_ptr<OtpInputDialogMediator> mediator_;
};

// Tests consumer receives the correct contents.
TEST_F(OtpInputDialogMediatorTest, SetConsumer) {
  OCMExpect([consumer_
      setContent:[OCMArg checkWithBlock:^BOOL(OtpInputDialogContent* content) {
        EXPECT_TRUE([content.windowTitle
            isEqualToString:base::SysUTF16ToNSString(
                                model_controller_->GetWindowTitle())]);
        EXPECT_TRUE([content.textFieldPlaceholder
            isEqualToString:base::SysUTF16ToNSString(
                                model_controller_
                                    ->GetTextfieldPlaceholderText())]);
        EXPECT_TRUE([content.confirmButtonLabel
            isEqualToString:base::SysUTF16ToNSString(
                                model_controller_->GetOkButtonLabel())]);
        return YES;
      }]]);

  mediator_->SetConsumer(consumer_);

  EXPECT_OCMOCK_VERIFY((id)consumer_);
}

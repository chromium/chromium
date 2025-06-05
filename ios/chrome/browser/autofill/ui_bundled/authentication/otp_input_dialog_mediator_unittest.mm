// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_content.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mutator.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using autofill::CardUnmaskChallengeOption;

// Mock version of CardUnmaskOtpInputDialogController.
class MockOtpUnmaskDelegate : public autofill::OtpUnmaskDelegate {
 public:
  MockOtpUnmaskDelegate() = default;
  ~MockOtpUnmaskDelegate() = default;

  MOCK_METHOD(void,
              OnUnmaskPromptAccepted,
              (const std::u16string& otp),
              (override));
  MOCK_METHOD(void,
              OnUnmaskPromptClosed,
              (bool user_closed_dialog),
              (override));
  MOCK_METHOD(void, OnNewOtpRequested, (), (override));

  base::WeakPtr<MockOtpUnmaskDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockOtpUnmaskDelegate> weak_ptr_factory_{this};
};

class OtpInputDialogMediatorTest
    : public PlatformTest,
      public ::testing::WithParamInterface<autofill::CreditCard::RecordType> {
 protected:
  OtpInputDialogMediatorTest() {
    consumer_ = OCMProtocolMock(@protocol(OtpInputDialogConsumer));
    delegate_ = OCMProtocolMock(@protocol(OtpInputDialogMediatorDelegate));
    CardUnmaskChallengeOption option = CardUnmaskChallengeOption(
        CardUnmaskChallengeOption::ChallengeOptionId("123"),
        autofill::CardUnmaskChallengeOptionType::kSmsOtp,
        /*challenge_info=*/u"xxx-xxx-3547",
        /*challenge_input_length=*/6U);
    autofill::CreditCard::RecordType card_type = GetParam();
    model_controller_ =
        std::make_unique<autofill::CardUnmaskOtpInputDialogControllerImpl>(
            card_type, option, unmask_delegate_.GetWeakPtr());
    mediator_ = std::make_unique<OtpInputDialogMediator>(
        model_controller_->GetImplWeakPtr(), delegate_);
  }

  ~OtpInputDialogMediatorTest() {
    EXPECT_OCMOCK_VERIFY(consumer_);
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

  id<OtpInputDialogConsumer> consumer_;
  id<OtpInputDialogMediatorDelegate> delegate_;
  testing::NiceMock<MockOtpUnmaskDelegate> unmask_delegate_;
  std::unique_ptr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      model_controller_;
  std::unique_ptr<OtpInputDialogMediator> mediator_;
};

// Tests consumer receives the correct contents.
TEST_P(OtpInputDialogMediatorTest, SetConsumer) {
  OCMExpect([consumer_
      setContent:[OCMArg checkWithBlock:^BOOL(OtpInputDialogContent* content) {
        EXPECT_NSEQ(
            content.windowTitle,
            base::SysUTF16ToNSString(model_controller_->GetWindowTitle()));
        EXPECT_NSEQ(content.textFieldPlaceholder,
                    base::SysUTF16ToNSString(
                        model_controller_->GetTextfieldPlaceholderText()));
        EXPECT_NSEQ(
            content.confirmButtonLabel,
            base::SysUTF16ToNSString(model_controller_->GetOkButtonLabel()));
        return YES;
      }]]);

  mediator_->SetConsumer(consumer_);

  EXPECT_OCMOCK_VERIFY((id)consumer_);
}

// TODO(crbug.com/422436003): re-enable.
TEST_P(OtpInputDialogMediatorTest, DISABLED_DidTapConfirmButton) {
  NSString* otp = @"123456";
  OCMExpect([consumer_ showPendingState]);
  EXPECT_CALL(unmask_delegate_,
              OnUnmaskPromptAccepted(base::SysNSStringToUTF16(otp)));

  [mediator_->AsMutator() didTapConfirmButton:otp];
}

TEST_P(OtpInputDialogMediatorTest, DidTapCancelButton) {
  OCMExpect([delegate_ dismissDialog]);
  EXPECT_CALL(unmask_delegate_,
              OnUnmaskPromptClosed(/*user_closed_dialog=*/true));

  [mediator_->AsMutator() didTapCancelButton];
}

// TODO(crbug.com/422435813): re-enable
TEST_P(OtpInputDialogMediatorTest, DISABLED_OnOtpInputChanges) {
  OCMExpect([consumer_ setConfirmButtonEnabled:NO]);

  [mediator_->AsMutator() onOtpInputChanges:@"12345"];

  OCMExpect([consumer_ setConfirmButtonEnabled:YES]);

  [mediator_->AsMutator() onOtpInputChanges:@"123456"];
}

TEST_P(OtpInputDialogMediatorTest, Dismiss) {
  OCMExpect([delegate_ dismissDialog]);
  EXPECT_CALL(unmask_delegate_,
              OnUnmaskPromptClosed(/*user_closed_dialog=*/true));

  mediator_->Dismiss(/*show_confirmation_before_closing=*/false,
                     /*user_closed_dialog=*/true);
}

TEST_P(OtpInputDialogMediatorTest, NewCodeRequested) {
  EXPECT_CALL(unmask_delegate_, OnNewOtpRequested());

  [mediator_->AsMutator() didTapNewCodeLink];
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    OtpInputDialogMediatorTest,
    testing::Values(autofill::CreditCard::RecordType::kLocalCard,
                    autofill::CreditCard::RecordType::kMaskedServerCard));

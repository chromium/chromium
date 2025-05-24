// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

class AutofillProgressDialogMediatorTest : public PlatformTest {
 protected:
  AutofillProgressDialogMediatorTest() {
    consumer_ = OCMProtocolMock(@protocol(AlertConsumer));
    delegate_ =
        OCMProtocolMock(@protocol(AutofillProgressDialogMediatorDelegate));
    model_controller_ = std::make_unique<
        autofill::AutofillProgressDialogControllerImpl>(
        autofill::AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog,
        base::DoNothing());
    mediator_ = std::make_unique<AutofillProgressDialogMediator>(
        model_controller_.get(), delegate_);
  }

  base::test::TaskEnvironment task_environment_;
  id<AlertConsumer> consumer_;
  id<AutofillProgressDialogMediatorDelegate> delegate_;
  std::unique_ptr<autofill::AutofillProgressDialogControllerImpl>
      model_controller_;
  std::unique_ptr<AutofillProgressDialogMediator> mediator_;
};

// Tests consumer receives the correct contents.
TEST_F(AutofillProgressDialogMediatorTest, SetConsumer) {
  OCMExpect([consumer_
      setTitle:base::SysUTF16ToNSString(model_controller_->GetLoadingTitle())]);
  OCMExpect([consumer_ setMessage:base::SysUTF16ToNSString(
                                      model_controller_->GetLoadingMessage())]);
  OCMExpect([consumer_ setActions:[OCMArg isNotNil]]);

  mediator_->SetConsumer(consumer_);

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests dismissing the dialog correctly invoke the dismissing command.
TEST_F(AutofillProgressDialogMediatorTest, DismissDialog) {
  OCMExpect([delegate_ dismissDialog]);

  mediator_->Dismiss(/*show_confirmation_before_closing=*/false,
                     /*is_canceled_by_user=*/true);

  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that when showing confirmation, the consumer is updated correctly.
TEST_F(AutofillProgressDialogMediatorTest,
       DismissDialog_ShowConfirmation_UpdatesConsumer) {
  NSString* expectedLabel = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_PROGRESS_DIALOG_CONFIRMATION_ACCESSIBILITY_ANNOUNCEMENT);
  mediator_->SetConsumer(consumer_);
  OCMExpect([consumer_ setConfirmationAccessibilityLabel:expectedLabel]);

  // Expectations for the consumer when showing confirmation.
  OCMExpect([consumer_ setProgressState:ProgressIndicatorStateSuccess]);
  OCMExpect([consumer_ setActions:@[]]);

  mediator_->Dismiss(/*show_confirmation_before_closing=*/true,
                     /*is_canceled_by_user=*/false);

  EXPECT_OCMOCK_VERIFY(consumer_);
}

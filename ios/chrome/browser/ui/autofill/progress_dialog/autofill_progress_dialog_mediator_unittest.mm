// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/progress_dialog/autofill_progress_dialog_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_progress_dialog_type.h"
#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "ios/chrome/browser/ui/alert_view/alert_action.h"
#import "ios/chrome/browser/ui/alert_view/alert_consumer.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AutofillProgressDialogMediatorTest : public PlatformTest {
 protected:
  AutofillProgressDialogMediatorTest() {
    consumer_ = OCMProtocolMock(@protocol(AlertConsumer));
    model_controller_ = std::make_unique<
        autofill::AutofillProgressDialogControllerImpl>(
        autofill::AutofillProgressDialogType::kVirtualCardUnmaskProgressDialog,
        base::DoNothing());
    mediator_ = std::make_unique<AutofillProgressDialogMediator>(
        model_controller_->GetImplWeakPtr());
  }

  id consumer_;
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

// TODO(b/324606723): Add test to verify OnDismissed is called upon mediator
// being destroyed.

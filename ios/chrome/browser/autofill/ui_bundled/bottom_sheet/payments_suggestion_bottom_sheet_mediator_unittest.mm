// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const char kTestNumber[] = "4234567890123456";  // Visa

const char kTestGuid[] = "00000000-0000-0000-0000-000000000001";

}  // namespace

class PaymentsSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  PaymentsSuggestionBottomSheetMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()) {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    test_web_state_->SetCurrentURL(GURL("http://foo.com"));

    consumer_ =
        OCMStrictProtocolMock(@protocol(PaymentsSuggestionBottomSheetConsumer));
  }

  void SetUp() override {
    personal_data_manager_.test_address_data_manager()
        .SetAutofillProfileEnabled(true);
    personal_data_manager_.test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);
  }

  void TearDown() override { [mediator_ disconnect]; }

  // Create a mediator.
  void CreateMediator() {
    web_state_list_->InsertWebState(
        std::move(test_web_state_),
        WebStateList::InsertionParams::Automatic().Activate());

    mediator_ = [[PaymentsSuggestionBottomSheetMediator alloc]
        initWithWebStateList:web_state_list_.get()
                      params:autofill::FormActivityParams()
         personalDataManager:&personal_data_manager_];
  }

  // Add credit card to personal data manager.
  autofill::CreditCard CreateCreditCard(
      std::string guid,
      std::string number = kTestNumber,
      int64_t instrument_id = 0,
      autofill::CreditCard::RecordType record_type =
          autofill::CreditCard::RecordType::kMaskedServerCard) {
    autofill::CreditCard card;
    autofill::test::SetCreditCardInfo(&card, "Jane Doe", number.c_str(),
                                      autofill::test::NextMonth().c_str(),
                                      autofill::test::NextYear().c_str(), "1");
    card.set_guid(guid);
    card.set_instrument_id(instrument_id);
    card.set_record_type(record_type);
    personal_data_manager_.test_payments_data_manager().AddServerCreditCard(
        card);
    return card;
  }

  // Create a mediator and make sure the personal data manager contains at least
  // 1 card.
  void CreateMediatorWithSuggestions() {
    CreateMediator();
    CreateCreditCard(kTestGuid);
    personal_data_manager_.payments_data_manager().SetSyncingForTest(true);
  }

  // Create a mediator and make sure the personal data manager contains at least
  // 1 local card.
  void CreateMediatorWithLocalCardOnlySuggestions() {
    CreateMediator();
    CreateCreditCard(kTestGuid, kTestNumber, 0,
                     autofill::CreditCard::RecordType::kLocalCard);
    personal_data_manager_.payments_data_manager().SetSyncingForTest(true);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  id consumer_;
  autofill::TestPersonalDataManager personal_data_manager_;
  PaymentsSuggestionBottomSheetMediator* mediator_;
};

// Tests PaymentsSuggestionBottomSheetMediator can be initialized.
TEST_F(PaymentsSuggestionBottomSheetMediatorTest, Init) {
  CreateMediator();
  EXPECT_TRUE(mediator_);
}

// Tests consumer when no suggestion is available.
TEST_F(PaymentsSuggestionBottomSheetMediatorTest, NoSuggestion) {
  CreateMediator();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ dismiss]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests consumer when suggestions are available (with non local card).
TEST_F(PaymentsSuggestionBottomSheetMediatorTest, WithSuggestions) {
  CreateMediatorWithSuggestions();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests consumer when suggestions are available (with local card).
TEST_F(PaymentsSuggestionBottomSheetMediatorTest,
       WithLocalCardOnlySuggestions) {
  CreateMediatorWithLocalCardOnlySuggestions();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:NO]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the mediator is correctly cleaned up when the WebStateList is
// destroyed. There are a lot of checked observer lists that could potentially
// cause a crash in the process, so this test ensures they're executed.
TEST_F(PaymentsSuggestionBottomSheetMediatorTest,
       CleansUpWhenWebStateListDestroyed) {
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  OCMExpect([consumer_ dismiss]);
  web_state_list_.reset();
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that filling the suggestion is only allowed once past the minimal
// delay before accepting filling. Tests each key moment, before view did
// appear, right after appearance, after some time but not enough to reach the
// minimal delay, and after the minimal delay.
TEST_F(PaymentsSuggestionBottomSheetMediatorTest, FillingDelay) {
  base::ScopedMockClockOverride mock_clock;
  base::HistogramTester histogram_tester;

  CreateMediator();

  OCMExpect([consumer_ activatePrimaryButton]);

  // Select a suggestion before the countdown even started, should be ignored.
  [mediator_ didTapOnPrimaryButton];

  // View did appear, now the countdown starts.
  [mediator_ paymentsBottomSheetViewDidAppear];

  // Try to select a suggestion right after view did appear while the countdown
  // is just about to start (no ticks yet), should be ignored.
  [mediator_ didTapOnPrimaryButton];

  // Advance time but not enough to reach the minimal delay so selecting the
  // suggestion is once again ignored.
  mock_clock.Advance(base::Milliseconds(250));
  [mediator_ didTapOnPrimaryButton];

  // Allow selecting a suggestion past the minimal delay.
  mock_clock.Advance(base::Milliseconds(250));
  [mediator_ didTapOnPrimaryButton];

  // Verify that the number of attempts is recorded under the "Accept" variant
  // of the histogram when a payment suggestion is accepted.
  [mediator_ logExitReason:PaymentsSuggestionBottomSheetExitReason::
                               kUsePaymentsSuggestion];
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.AcceptAttempts.Accept",
      /*sample=*/4,
      /*expected_bucket_count=*/1);

  // Verify that the number of attempts is recorded under the "Dismiss" variant
  // of the histogram when the sheet is dismissed without filling the
  // suggestion.
  [mediator_ logExitReason:PaymentsSuggestionBottomSheetExitReason::
                               kShowPaymentDetails];
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.AcceptAttempts.Dismiss",
      /*sample=*/4,
      /*expected_bucket_count=*/1);
}

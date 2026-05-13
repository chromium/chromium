// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/form_import/form_data_importer.h"
#import "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PaymentsScanSaveAndFillOfferBottomSheetMediatorTest
    : public PlatformTest {
 public:
  PaymentsScanSaveAndFillOfferBottomSheetMediatorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_web_state_(std::make_unique<web::FakeWebState>()) {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    test_web_state_->SetCurrentURL(GURL("http://foo.com"));

    test_web_state_->SetWebFramesManager(
        AutofillBottomSheetJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::make_unique<web::FakeWebFramesManager>());

    consumer_ = OCMStrictProtocolMock(
        @protocol(PaymentsScanSaveAndFillOfferBottomSheetConsumer));

    autofill_client_ = std::make_unique<autofill::TestAutofillClientIOS>(
        test_web_state_.get(), nil);
  }

  ~PaymentsScanSaveAndFillOfferBottomSheetMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    [mediator_ disconnect];
  }

  // Create a mediator.
  void CreateMediator() {
    web::WebState* web_state = test_web_state_.get();
    web_state_list_->InsertWebState(
        std::move(test_web_state_),
        WebStateList::InsertionParams::Automatic().Activate());

    mediator_ = [[PaymentsScanSaveAndFillOfferBottomSheetMediator alloc]
        initWithParams:autofill::FormActivityParams()
              webState:web_state];
    mediator_.consumer = consumer_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  id consumer_;
  PaymentsScanSaveAndFillOfferBottomSheetMediator* mediator_;
  std::unique_ptr<autofill::TestAutofillClientIOS> autofill_client_;
};

// Tests that `didAccpetScanCardSuggestion` successfully forwards the scan card
// suggestion to the cross provider.
TEST_F(PaymentsScanSaveAndFillOfferBottomSheetMediatorTest,
       DidAccpetScanCardSuggestion) {
  // Initialize the mediator before configuring its properties.
  CreateMediator();

  id providerMock = OCMProtocolMock(@protocol(FormInputSuggestionsProvider));
  [mediator_ setProvider:providerMock];

  // We use `ignoringNonObjectArgs` to safely mock over the C++ struct `params`.
  autofill::FormActivityParams dummy_params;
  [[[providerMock expect] ignoringNonObjectArgs]
      didSelectSuggestion:[OCMArg checkWithBlock:^BOOL(
                                      FormSuggestion* suggestion) {
        return suggestion.type ==
               autofill::SuggestionType::kSaveAndFillCreditCardEntry;
      }]
                  atIndex:0
                   params:dummy_params
               completion:nil];

  [mediator_ didAcceptScanCardSuggestion];

  EXPECT_OCMOCK_VERIFY(providerMock);
}

TEST_F(PaymentsScanSaveAndFillOfferBottomSheetMediatorTest, ExitReasonLogging) {
  base::HistogramTester histogram_tester;
  CreateMediator();

  [mediator_ logExitReason:ScanCardSuggestionBottomSheetExitReason::kIgnore];
  histogram_tester.ExpectUniqueSample(
      "IOS.ScanCardBottomSheet.ExitReason",
      static_cast<int>(ScanCardSuggestionBottomSheetExitReason::kIgnore), 1);

  [mediator_
      logExitReason:ScanCardSuggestionBottomSheetExitReason::kAcceptSuggestion];
  histogram_tester.ExpectBucketCount(
      "IOS.ScanCardBottomSheet.ExitReason",
      static_cast<int>(
          ScanCardSuggestionBottomSheetExitReason::kAcceptSuggestion),
      1);
}

TEST_F(PaymentsScanSaveAndFillOfferBottomSheetMediatorTest,
       TimeToSelectionLogging) {
  base::HistogramTester histogram_tester;
  CreateMediator();

  [mediator_ scanCardBottomSheetViewDidAppear];

  const auto time_to_selection = base::Milliseconds(500);
  task_environment_.FastForwardBy(time_to_selection);

  [mediator_ didAcceptScanCardSuggestion];

  histogram_tester.ExpectTimeBucketCount(
      "IOS.ScanCardBottomSheet.TimeToSelection", time_to_selection, 1);
}

// Tests that `didAcceptScanCardSuggestion` sets the
// card_submitted_through_save_and_fill flag.
TEST_F(PaymentsScanSaveAndFillOfferBottomSheetMediatorTest,
       DidAcceptScanCardSuggestionSetsFlag) {
  CreateMediator();

  autofill::payments::PaymentsFormDataImporter& payments_importer =
      autofill_client_->GetFormDataImporter()->GetPaymentsFormDataImporter();
  EXPECT_FALSE(payments_importer.fetched_payments_data_context()
                   .card_submitted_through_save_and_fill);

  [mediator_ didAcceptScanCardSuggestion];

  EXPECT_TRUE(payments_importer.fetched_payments_data_context()
                  .card_submitted_through_save_and_fill);
}

// Tests that `didCancelScanCardSuggestion` sets the
// card_submitted_through_save_and_fill flag.
TEST_F(PaymentsScanSaveAndFillOfferBottomSheetMediatorTest,
       DidCancelScanCardSuggestionSetsFlag) {
  CreateMediator();

  autofill::payments::PaymentsFormDataImporter& payments_importer =
      autofill_client_->GetFormDataImporter()->GetPaymentsFormDataImporter();
  EXPECT_FALSE(payments_importer.fetched_payments_data_context()
                   .card_submitted_through_save_and_fill);

  [mediator_ didCancelScanCardSuggestion];

  EXPECT_TRUE(payments_importer.fetched_payments_data_context()
                  .card_submitted_through_save_and_fill);
}

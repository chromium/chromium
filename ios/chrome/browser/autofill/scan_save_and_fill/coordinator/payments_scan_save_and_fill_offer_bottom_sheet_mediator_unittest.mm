// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"

#import <memory>

#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PaymentsScanSaveAndFillOfferBottomSheetMediatorTest
    : public PlatformTest {
 public:
  PaymentsScanSaveAndFillOfferBottomSheetMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()) {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    test_web_state_->SetCurrentURL(GURL("http://foo.com"));

    consumer_ = OCMStrictProtocolMock(
        @protocol(PaymentsScanSaveAndFillOfferBottomSheetConsumer));
  }

  ~PaymentsScanSaveAndFillOfferBottomSheetMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    [mediator_ disconnect];
  }

  // Create a mediator.
  void CreateMediator() {
    web_state_list_->InsertWebState(
        std::move(test_web_state_),
        WebStateList::InsertionParams::Automatic().Activate());

    mediator_ = [[PaymentsScanSaveAndFillOfferBottomSheetMediator alloc]
        initWithParams:autofill::FormActivityParams()];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  id consumer_;
  PaymentsScanSaveAndFillOfferBottomSheetMediator* mediator_;
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
                   params:dummy_params];

  [mediator_ didAcceptScanCardSuggestion];

  EXPECT_OCMOCK_VERIFY(providerMock);
}

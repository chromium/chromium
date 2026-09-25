// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/payments/coordinator/credit_card_suggestion_bottom_sheet_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/test_utils/autofill_test_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_controller.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/payments/ui/credit_card_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@class CreditCardData;
@class FormSuggestion;

@interface CreditCardSuggestionBottomSheetMediator (Testing)
- (void)onSuggestionsRetrieved:(NSArray<FormSuggestion*>*)suggestions;
- (void)fillCreditCard:(CreditCardData*)creditCardData
               atIndex:(NSInteger)index
         crossProvider:(id<FormInputSuggestionsProvider>)crossProvider;
- (void)disableBottomSheetAndRefocus:(BOOL)shouldRefocus;
- (void)refocus;
@end

namespace {

const char kTestNumber[] = "4234567890123456";  // Visa
const char kTestGuid[] = "00000000-0000-0000-0000-000000000001";

class CreditCardSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  CreditCardSuggestionBottomSheetMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()) {
    scoped_feature_list_.InitAndDisableFeature(kAutofillPaymentsSheetV2Ios);

    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    test_web_state_->SetCurrentURL(GURL("http://foo.com"));

    consumer_ = OCMStrictProtocolMock(
        @protocol(CreditCardSuggestionBottomSheetConsumer));
    delegate_ = OCMStrictProtocolMock(
        @protocol(CreditCardSuggestionBottomSheetMediatorDelegate));

    FormSuggestionTabHelper::CreateForWebState(test_web_state_.get(), @[]);
  }

  ~CreditCardSuggestionBottomSheetMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(consumer_);
    EXPECT_OCMOCK_VERIFY(delegate_);
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

    mediator_ = [[CreditCardSuggestionBottomSheetMediator alloc]
        initWithWebStateList:web_state_list_.get()
                      params:autofill::FormActivityParams()
         personalDataManager:&personal_data_manager_];
    mediator_.delegate = delegate_;
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
  id delegate_;
  autofill::TestPersonalDataManager personal_data_manager_;
  CreditCardSuggestionBottomSheetMediator* mediator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests CreditCardSuggestionBottomSheetMediator can be initialized.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest, Init) {
  CreateMediator();
  EXPECT_TRUE(mediator_);
}

// Tests consumer when no suggestion is available.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest, NoSuggestion) {
  CreateMediator();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ dismiss]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests consumer when suggestions are available (with non local card).
TEST_F(CreditCardSuggestionBottomSheetMediatorTest, WithSuggestions) {
  CreateMediatorWithSuggestions();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests consumer when suggestions are available (with local card).
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
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
// TODO(crbug.com/422436424): re-enable.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       DISABLED_CleansUpWhenWebStateListDestroyed) {
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  OCMExpect([delegate_
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:mediator_]);
  web_state_list_.reset();
  EXPECT_OCMOCK_VERIFY(consumer_);
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Tests that filling the suggestion is only allowed once past the minimal
// delay before accepting filling. Tests each key moment, before view did
// appear, right after appearance, after some time but not enough to reach the
// minimal delay, and after the minimal delay.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest, FillingDelay) {
  base::ScopedMockClockOverride mock_clock;

  CreateMediatorWithSuggestions();
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

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

  // Once a minimal delay has passed, the primary button becomes active. We
  // expect this call to happen a single time, at this stage.
  OCMExpect([consumer_ activatePrimaryButton]);
  // Allow selecting a suggestion past the minimal delay.
  mock_clock.Advance(base::Milliseconds(250));
  [mediator_ didTapOnPrimaryButton];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the payment sheet is aborted when there are no suggestions that
// could be retrieved, when in stateless mode.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       Stateless_AbortWhenNoSuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kStatelessFormSuggestionController,
                            kAutofillPaymentsSheetStateless},
      /*disabled_features=*/{});

  // Mock -retrieveSuggestionsForForm via swizzling.
  id swizzle_block = ^void(id self, const autofill::FormActivityParams& params,
                           web::WebState* web_state,
                           FormSuggestionsReadyCompletion completion) {
    if (completion) {
      completion(@[], self);
    }
  };
  ScopedBlockSwizzler swizzler(
      [FormSuggestionController class],
      @selector(retrieveSuggestionsForForm:webState:accessoryViewUpdateBlock:),
      swizzle_block);

  base::HistogramTester histogram_tester;

  CreateMediator();

  id mediator_mock = OCMPartialMock(mediator_);
  OCMExpect([mediator_mock disableBottomSheetAndRefocus:YES]);

  [mediator_mock didSelectCreditCard:nil atIndex:0];

  EXPECT_OCMOCK_VERIFY(mediator_mock);
  [mediator_mock stopMocking];
  histogram_tester.ExpectUniqueSample(
      "IOS.PaymentsBottomSheet.ExitReason",
      /*sample=*/PaymentsSuggestionBottomSheetExitReason::kBadProvider,
      /*expected_bucket_count=*/1);
}

// Test that cross-document navigation start dismisses the bottom sheet and
// disables it without refocusing upon card selection.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       DismissesAndInvalidatesOnCrossDocumentNavigation) {
  base::HistogramTester histogram_tester;
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  // Expect dismissal on navigation start.
  OCMExpect([delegate_
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:mediator_]);

  web::FakeNavigationContext cross_doc_context;
  cross_doc_context.SetIsSameDocument(false);

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  ASSERT_TRUE(active_web_state);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationStarted(&cross_doc_context);

  id mediator_mock = OCMPartialMock(mediator_);
  OCMExpect([mediator_mock disableBottomSheetAndRefocus:NO]);
  OCMReject([mediator_mock fillCreditCard:[OCMArg any]
                                  atIndex:0
                            crossProvider:[OCMArg any]]);

  // Selecting a credit card when context is invalidated should disable the
  // sheet without refocusing, and should not log an exit reason metric.
  [mediator_mock didSelectCreditCard:nil atIndex:0];

  EXPECT_OCMOCK_VERIFY(mediator_mock);
  [mediator_mock stopMocking];
  histogram_tester.ExpectTotalCount("IOS.PaymentsBottomSheet.ExitReason", 0);
}

// Test that cross-document navigation finish dismisses the bottom sheet and
// disables it without refocusing upon card selection when navigation has
// committed.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       DismissesAndInvalidatesOnCrossDocumentNavigationFinished) {
  base::HistogramTester histogram_tester;
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  OCMExpect([delegate_
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:mediator_]);

  web::FakeNavigationContext cross_doc_context;
  cross_doc_context.SetIsSameDocument(false);
  cross_doc_context.SetHasCommitted(true);

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  ASSERT_TRUE(active_web_state);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationFinished(&cross_doc_context);

  id mediator_mock = OCMPartialMock(mediator_);
  OCMExpect([mediator_mock disableBottomSheetAndRefocus:NO]);
  OCMReject([mediator_mock fillCreditCard:[OCMArg any]
                                  atIndex:0
                            crossProvider:[OCMArg any]]);

  // Selecting a credit card when context is invalidated should disable the
  // sheet without refocusing, and should not log an exit reason metric.
  [mediator_mock didSelectCreditCard:nil atIndex:0];

  EXPECT_OCMOCK_VERIFY(mediator_mock);
  [mediator_mock stopMocking];
  histogram_tester.ExpectTotalCount("IOS.PaymentsBottomSheet.ExitReason", 0);
}

// Test that cross-document navigation finish does not dismiss the bottom sheet
// when the navigation has not committed.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       PreservesContextOnCrossDocumentNavigationFinishedUncommitted) {
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  OCMReject([delegate_
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:mediator_]);

  web::FakeNavigationContext cross_doc_context;
  cross_doc_context.SetIsSameDocument(false);
  cross_doc_context.SetHasCommitted(false);

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  ASSERT_TRUE(active_web_state);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationFinished(&cross_doc_context);

  EXPECT_OCMOCK_VERIFY(consumer_);
  EXPECT_OCMOCK_VERIFY(delegate_);
}

// Test that same-document navigation does not dismiss the bottom sheet and
// allows card selection to fill credit card details.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       PreservesContextOnSameDocumentNavigation) {
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  web::FakeNavigationContext same_doc_context;
  same_doc_context.SetIsSameDocument(true);

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  ASSERT_TRUE(active_web_state);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationStarted(&same_doc_context);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationFinished(&same_doc_context);

  // Swizzle FormSuggestionController type to return
  // SuggestionProviderTypeAutofill.
  id type_block = ^SuggestionProviderType(id self) {
    return SuggestionProviderTypeAutofill;
  };
  ScopedBlockSwizzler type_swizzler([FormSuggestionController class],
                                    @selector(type), type_block);

  id mediator_mock = OCMPartialMock(mediator_);
  OCMExpect([mediator_mock fillCreditCard:[OCMArg any]
                                  atIndex:0
                            crossProvider:[OCMArg isNotNil]]);

  [mediator_mock didSelectCreditCard:nil atIndex:0];

  EXPECT_OCMOCK_VERIFY(mediator_mock);
  [mediator_mock stopMocking];
}

// Test that cross-document navigation invalidation takes precedence over empty
// suggestions, ensuring the fill context status is not downgraded.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       NavigationInvalidationTakesPrecedenceOverEmptySuggestions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kStatelessFormSuggestionController,
                            kAutofillPaymentsSheetStateless},
      /*disabled_features=*/{});
  base::HistogramTester histogram_tester;
  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);
  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  // Expect dismissal on navigation start.
  OCMExpect([delegate_
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:mediator_]);

  web::FakeNavigationContext cross_doc_context;
  cross_doc_context.SetIsSameDocument(false);

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  ASSERT_TRUE(active_web_state);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationStarted(&cross_doc_context);

  // An in-flight asynchronous suggestion callback arrives with empty
  // suggestions. The latch must prevent transitioning to
  // kInvalidatedNoSuggestions.
  [mediator_ onSuggestionsRetrieved:@[]];

  id mediator_mock = OCMPartialMock(mediator_);
  OCMExpect([mediator_mock disableBottomSheetAndRefocus:NO]);
  OCMReject([mediator_mock disableBottomSheetAndRefocus:YES]);
  OCMReject([mediator_mock fillCreditCard:[OCMArg any]
                                  atIndex:0
                            crossProvider:[OCMArg any]]);

  // If the status were downgraded to kInvalidatedNoSuggestions,
  // didSelectCreditCard would call disableBottomSheetAndRefocus:YES and log
  // kBadProvider. With kInvalidatedByWebStateChange preserved, it calls
  // disableBottomSheetAndRefocus:NO.
  [mediator_mock didSelectCreditCard:nil atIndex:0];

  EXPECT_OCMOCK_VERIFY(mediator_mock);
  [mediator_mock stopMocking];

  histogram_tester.ExpectTotalCount("IOS.PaymentsBottomSheet.ExitReason", 0);
}

// Test that calling `disableBottomSheetAndRefocus:YES` does not trigger
// refocus when the bottom sheet was dismissed due to navigation.
TEST_F(CreditCardSuggestionBottomSheetMediatorTest,
       DisableBottomSheetAndRefocus_NavigationSuppressesRefocus) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kAutofillPaymentsSheetV2Ios},
      /*disabled_features=*/{});

  CreateMediatorWithSuggestions();
  ASSERT_TRUE(mediator_);

  OCMExpect([consumer_ setCreditCardData:[OCMArg isNotNil]
                       showGooglePayLogo:YES]);
  [mediator_ setConsumer:consumer_];

  // Expect dismissal on navigation start.
  OCMExpect([delegate_
      creditCardSuggestionBottomSheetMediatorDidRequestDismissal:mediator_]);

  web::FakeNavigationContext cross_doc_context;
  cross_doc_context.SetIsSameDocument(false);

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  ASSERT_TRUE(active_web_state);
  static_cast<web::FakeWebState*>(active_web_state)
      ->OnNavigationStarted(&cross_doc_context);

  id mediator_mock = OCMPartialMock(mediator_);
  OCMReject([mediator_mock refocus]);

  // Requesting refocus:YES while dismissed due to navigation should suppress
  // refocus.
  [mediator_mock disableBottomSheetAndRefocus:YES];

  EXPECT_OCMOCK_VERIFY(mediator_mock);
  [mediator_mock stopMocking];
}

}  // namespace

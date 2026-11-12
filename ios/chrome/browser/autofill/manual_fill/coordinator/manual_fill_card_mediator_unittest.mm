// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/coordinator/manual_fill_card_mediator.h"

#import <memory>
#import <string_view>

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/run_until.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/payments/payments_service_url.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/test_utils/autofill_test_util.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credit_card.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_injection_handler.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/card_consumer.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/card_list_delegate.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell+Testing.h"
#import "ios/chrome/browser/autofill/model/manual_fill_virtual_card_cache.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface ManualFillCardItem (TestingCard)
@property(nonatomic, readonly) ManualFillCreditCard* card;
@end

@interface ManualFillCardMediator (Testing)
- (void)onPersonalDataChanged;
@end

using autofill::CreditCard;
using manual_fill::PaymentFieldType;

namespace {

constexpr char kCardNumber[] = "4234567890123456";  // Visa

constexpr char kCardGuid1[] = "00000000-0000-0000-0000-000000000001";
constexpr char kCardGuid2[] = "00000000-0000-0000-0000-000000000002";

constexpr std::string_view kSuggestionAcceptedAction =
    "ManualFallback_CreditCard_SuggestionAccepted";

// Returns the expected cell index accessibility label for the given
// `cell_index` and `cell_count`.
NSString* ExpectedCellIndexAccessibilityLabel(int cell_index, int cell_count) {
  return base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(IDS_IOS_MANUAL_FALLBACK_PAYMENT_CELL_INDEX),
          "position", cell_index + 1, "count", cell_count));
}

// Creates a `ManualFillCreditCard` with the given `record_type` and
// `can_fill_directly` state for testing mutator actions.
ManualFillCreditCard* CreateManualFillCreditCard(
    CreditCard::RecordType record_type,
    bool can_fill_directly) {
  return [[ManualFillCreditCard alloc]
                          initWithGUID:base::SysUTF8ToNSString(kCardGuid1)
                               network:@"Visa"
                                  icon:nil
                              bankName:nil
                            cardHolder:@"Jane Doe"
                                number:base::SysUTF8ToNSString(kCardNumber)
                      obfuscatedNumber:@"••••3456"
              networkAndLastFourDigits:nil
                        expirationYear:@"29"
                       expirationMonth:@"01"
                                   CVC:@"123"
                            recordType:record_type
      cardInfoRetrievalEnrollmentState:
          CreditCard::CardInfoRetrievalEnrollmentState::
              kRetrievalUnenrolledAndNotEligible
                       canFillDirectly:can_fill_directly];
}

// Helper class to use ChromeAutofillClientIOS in tests. This allows
// WithFakedFromWebState to instantiate the client correctly.
class TestChromeAutofillClient : public autofill::ChromeAutofillClientIOS {
 public:
  using ChromeAutofillClientIOS::ChromeAutofillClientIOS;
};

}  // namespace

// A fake implementation of ManualFillContentInjector for testing.
@interface FakeContentInjector : NSObject <ManualFillContentInjector>
@property(nonatomic, assign) url::Origin activeOrigin;
@end

@implementation FakeContentInjector
- (url::Origin)activeWebFrameOrigin {
  return self.activeOrigin;
}
- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS {
  return YES;
}
- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS
           jumpToNextField:(BOOL)jumpToNextField
                actionType:(autofill::mojom::FieldActionType)actionType {
}
- (void)autofillFormWithCredential:(ManualFillCredential*)credential
                      shouldReauth:(BOOL)shouldReauth {
}
- (void)autofillFormWithSuggestion:(FormSuggestion*)formSuggestion
                           atIndex:(NSInteger)index {
}
- (BOOL)isActiveFormAPasswordForm {
  return NO;
}
@end

// Test fixture for testing the ManualFillCardMediator class.
class ManualFillCardMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    test_personal_data_manager_.test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);
    test_personal_data_manager_.test_payments_data_manager()
        .SetAutofillWalletImportEnabled(true);

    mediator_ = [[ManualFillCardMediator alloc]
        initWithPersonalDataManager:&test_personal_data_manager_
             showAutofillFormButton:NO
                           webState:nullptr];

    consumer_ = OCMProtocolMock(@protocol(ManualFillCardConsumer));
    content_injector_ = OCMProtocolMock(@protocol(ManualFillContentInjector));
    navigation_delegate_ = OCMProtocolMock(@protocol(CardListDelegate));
    mediator_.consumer = consumer_;
    mediator_.contentInjector = content_injector_;
    mediator_.navigationDelegate = navigation_delegate_;
  }

  void TearDown() override { [mediator_ disconnect]; }

  ManualFillCardMediator* mediator() { return mediator_; }

  OCMockObject<ManualFillCardConsumer>* consumer() { return consumer_; }

  OCMockObject<ManualFillContentInjector>* content_injector() {
    return content_injector_;
  }

  OCMockObject<CardListDelegate>* navigation_delegate() {
    return navigation_delegate_;
  }

  CreditCard CreateAndSaveCreditCard(std::string guid,
                                     bool enrolled_for_virtual_card = false) {
    CreditCard card;
    autofill::test::SetCreditCardInfo(&card, "Jane Doe", kCardNumber,
                                      autofill::test::NextMonth().c_str(),
                                      autofill::test::NextYear().c_str(), "1");
    card.set_guid(guid);
    card.set_server_id(guid);
    card.set_instrument_id(0);
    card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
    if (enrolled_for_virtual_card) {
      card.set_virtual_card_enrollment_state(
          CreditCard::VirtualCardEnrollmentState::kEnrolled);
    }
    test_personal_data_manager_.test_payments_data_manager()
        .AddServerCreditCard(card);
    return card;
  }

  // Sets up expectations for a successful "Autofill Form" button tap and
  // triggers it on `mediator_`.
  void ExpectAndTapAutofillFormButton(ManualFillCreditCard* card,
                                      NSInteger cell_index,
                                      autofill::SuggestionType expected_type) {
    OCMExpect([content_injector() canUserInjectInPasswordField:NO
                                                 requiresHTTPS:YES])
        .andReturn(YES);
    OCMExpect([content_injector()
        autofillFormWithSuggestion:[OCMArg checkWithBlock:^BOOL(
                                               FormSuggestion* suggestion) {
          EXPECT_EQ(suggestion.type, expected_type);
          EXPECT_EQ(
              std::get<autofill::Suggestion::Guid>(suggestion.payload),
              autofill::Suggestion::Guid(base::SysNSStringToUTF8(card.GUID)));
          return YES;
        }]
                           atIndex:cell_index]);
    [mediator() didTapAutofillFormButtonForCard:card cellIndex:cell_index];
  }

  // Sets up `content_injector_` expectation for a direct chip fill of
  // `expected_value`.
  void ExpectDirectChipFill(NSString* expected_value, bool requires_https) {
    OCMExpect([content_injector()
        userDidPickContent:expected_value
             passwordField:NO
             requiresHTTPS:requires_https
           jumpToNextField:YES
                actionType:autofill::mojom::FieldActionType::kReplaceAll]);
  }

  // Sets up `content_injector_` and `navigation_delegate_` expectations for a
  // chip tap that requests the full credit card for `field_type`.
  void ExpectFullCardRequest(ManualFillCreditCard* card,
                             PaymentFieldType field_type) {
    OCMExpect([content_injector() canUserInjectInPasswordField:NO
                                                 requiresHTTPS:YES])
        .andReturn(YES);
    OCMExpect([navigation_delegate() requestFullCreditCard:card
                                                 fieldType:field_type]);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  OCMockObject<ManualFillCardConsumer>* consumer_;
  OCMockObject<ManualFillContentInjector>* content_injector_;
  OCMockObject<CardListDelegate>* navigation_delegate_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  ManualFillCardMediator* mediator_;
};

// Tests that the right index and cell index accessibility label are passed to
// ManualFillCardItems upon creation when there's a saved card that's enrolled
// for virtual card.
TEST_F(ManualFillCardMediatorTest, CreateManualFillCardItemsWithVirtualCard) {
  // Save a server card that's enrolled for virtual card. This should result in
  // creating two ManualFillCardItems: one for the virtual card and another for
  // the server card.
  CreateAndSaveCreditCard(kCardGuid1, /*enrolled_for_virtual_card=*/true);

  OCMExpect([consumer()
      presentCards:[OCMArg checkWithBlock:^(
                               NSArray<ManualFillCardItem*>* card_items) {
        NSUInteger cell_count = card_items.count;
        EXPECT_EQ(cell_count, 3u);

        // Verify the cell index given to each item.
        EXPECT_EQ(card_items[0].cellIndex, 0);
        EXPECT_EQ(card_items[1].cellIndex, 1);
        EXPECT_EQ(card_items[2].cellIndex, 2);

        // Verify the cell index accessibility label given to each item.
        EXPECT_NSEQ(
            card_items[0].cellIndexAccessibilityLabel,
            ExpectedCellIndexAccessibilityLabel(/*cell_index=*/0, cell_count));
        EXPECT_NSEQ(
            card_items[1].cellIndexAccessibilityLabel,
            ExpectedCellIndexAccessibilityLabel(/*cell_index=*/1, cell_count));
        EXPECT_NSEQ(
            card_items[2].cellIndexAccessibilityLabel,
            ExpectedCellIndexAccessibilityLabel(/*cell_index=*/2, cell_count));

        return YES;
      }]]);

  // Save an additional card.
  CreateAndSaveCreditCard(kCardGuid2);
  RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(consumer());
}

// Tests that the mediator notifies the delegate when a full card request
// succeeds. This signal is used to re-show the manual fallback UI if it was
// dismissed during the unmasking process.
TEST_F(ManualFillCardMediatorTest,
       OnFullCardRequestSucceeded_NotifiesDelegate) {
  // Set up ScopedTestingWebClient with FakeWebClient.
  web::ScopedTestingWebClient web_client(
      std::make_unique<web::FakeWebClient>());

  // Create a REAL WebState via Profile.
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateTestSyncService));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  web::WebState::CreateParams params(profile.get());
  auto web_state = web::WebState::Create(params);

  // Set up InfoBarManager.
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state.get());

  // Attach the Client.
  auto client = std::make_unique<
      autofill::WithFakedFromWebState<TestChromeAutofillClient>>(
      profile.get(), web_state.get(), infobar_manager, /*bridge=*/nil);

  // Create Test Data.
  CreditCard card = CreateAndSaveCreditCard(kCardGuid1);

  // Expect the call.
  OCMExpect([navigation_delegate() cardSelectionFinished]);

  // Invoke.
  [mediator() onFullCardRequestSucceeded:card
                               fieldType:PaymentFieldType::kCardNumber
                             forWebState:web_state.get()];

  // Verify.
  EXPECT_OCMOCK_VERIFY(navigation_delegate());
}

// Tests that the mediator uses the cached unmasked virtual card if available.
TEST_F(ManualFillCardMediatorTest,
       CreateManualFillCardItemsWithCachedVirtualCard) {
  // Set up ScopedTestingWebClient with FakeWebClient.
  web::ScopedTestingWebClient web_client(
      std::make_unique<web::FakeWebClient>());

  // Create a REAL WebState.
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateTestSyncService));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  web::WebState::CreateParams params(profile.get());
  auto web_state = web::WebState::Create(params);

  // Re-create mediator with the web_state.
  [mediator() disconnect];
  mediator_ = [[ManualFillCardMediator alloc]
      initWithPersonalDataManager:&test_personal_data_manager_
           showAutofillFormButton:NO
                         webState:web_state.get()];
  mediator_.consumer = consumer();

  // Save a server card that's enrolled for virtual card.
  CreditCard card =
      CreateAndSaveCreditCard(kCardGuid1, /*enrolled_for_virtual_card=*/true);

  // Create an unmasked virtual card and put it in the cache.
  CreditCard unmaskedCard = CreditCard::CreateVirtualCard(card);
  unmaskedCard.SetRawInfo(autofill::CREDIT_CARD_NUMBER, u"4234567890123456");
  unmaskedCard.set_cvc(u"123");

  // Use FakeContentInjector to return a specific origin.
  FakeContentInjector* fake_injector = [[FakeContentInjector alloc] init];
  url::Origin test_origin = url::Origin::Create(GURL("https://example.com"));
  fake_injector.activeOrigin = test_origin;
  mediator_.contentInjector = fake_injector;

  ManualFillVirtualCardCache::CreateForWebState(web_state.get());
  ManualFillVirtualCardCache::FromWebState(web_state.get())
      ->CacheUnmaskedCard(unmaskedCard, test_origin);

  auto captured_card_items =
      std::make_shared<NSArray<ManualFillCardItem*>*>(nil);
  OCMExpect([consumer()
      presentCards:[OCMArg checkWithBlock:^(
                               NSArray<ManualFillCardItem*>* card_items) {
        *captured_card_items = card_items;
        return YES;
      }]]);

  // Trigger postCardsToConsumer by notifying personal data changed.
  [mediator() onPersonalDataChanged];

  EXPECT_TRUE(base::test::RunUntil(
      [captured_card_items]() { return *captured_card_items != nil; }));

  EXPECT_OCMOCK_VERIFY(consumer());

  EXPECT_EQ((*captured_card_items).count, 2u);

  // The first item should be the virtual card.
  // Verify it has the unmasked number and CVC.
  EXPECT_NSEQ((*captured_card_items)[0].card.number,
              base::SysUTF8ToNSString(kCardNumber));
  EXPECT_NSEQ((*captured_card_items)[0].card.CVC, @"123");

  // Disconnect to avoid dangling pointer to local web_state.
  [mediator() disconnect];
}

// Tests that tapping the "Autofill Form" button forwards the suggestion to the
// content injector and records metrics when injection is allowed.
TEST_F(ManualFillCardMediatorTest, DidTapAutofillFormButton) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ManualFillCreditCard* local_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kLocalCard, /*can_fill_directly=*/true);
  ManualFillCreditCard* virtual_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kVirtualCard, /*can_fill_directly=*/false);

  ExpectAndTapAutofillFormButton(local_card, /*cell_index=*/1,
                                 autofill::SuggestionType::kCreditCardEntry);
  histogram_tester.ExpectUniqueSample(
      manual_fill::kAcceptedCardSuggestionHistogram, 1, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionAcceptedAction), 1);

  ExpectAndTapAutofillFormButton(
      virtual_card, /*cell_index=*/2,
      autofill::SuggestionType::kVirtualCreditCardEntry);
  histogram_tester.ExpectBucketCount(
      manual_fill::kAcceptedCardSuggestionHistogram, 2, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionAcceptedAction), 2);

  EXPECT_OCMOCK_VERIFY(content_injector());
}

// Tests that tapping chip buttons for a card that can be filled directly
// injects the field values and records user action metrics.
TEST_F(ManualFillCardMediatorTest, DidTapChipButtons_FillsDirectly) {
  base::UserActionTester user_action_tester;
  ManualFillCreditCard* local_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kLocalCard, /*can_fill_directly=*/true);
  ManualFillCreditCard* virtual_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kVirtualCard, /*can_fill_directly=*/false);
  OCMStub([content_injector() canUserInjectInPasswordField:NO
                                             requiresHTTPS:YES])
      .andReturn(YES);

  ExpectDirectChipFill(local_card.number, /*requires_https=*/true);
  [mediator() didTapChipForCard:local_card
                      fieldType:ManualFillCardFieldType::kCardNumber];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_CreditCard_SelectCardNumber"),
            1);

  ExpectDirectChipFill(local_card.cardHolder, /*requires_https=*/false);
  [mediator() didTapChipForCard:local_card
                      fieldType:ManualFillCardFieldType::kCardholderName];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_CreditCard_SelectCardholderName"),
            1);

  ExpectDirectChipFill(virtual_card.cardHolder, /*requires_https=*/false);
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kCardholderName];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_VirtualCard_SelectCardholderName"),
            1);

  ExpectDirectChipFill(local_card.expirationMonth, /*requires_https=*/false);
  [mediator() didTapChipForCard:local_card
                      fieldType:ManualFillCardFieldType::kExpirationMonth];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_CreditCard_SelectExpirationMonth"),
            1);

  ExpectDirectChipFill(local_card.expirationYear, /*requires_https=*/false);
  [mediator() didTapChipForCard:local_card
                      fieldType:ManualFillCardFieldType::kExpirationYear];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_CreditCard_SelectExpirationYear"),
            1);

  ExpectDirectChipFill(local_card.CVC, /*requires_https=*/true);
  [mediator() didTapChipForCard:local_card
                      fieldType:ManualFillCardFieldType::kCVC];
  EXPECT_EQ(
      user_action_tester.GetActionCount("ManualFallback_CreditCard_SelectCvc"),
      1);

  EXPECT_OCMOCK_VERIFY(content_injector());
}

// Tests that tapping sensitive chips on masked server cards and virtual cards
// requests the full credit card when injection is allowed.
TEST_F(ManualFillCardMediatorTest, DidTapChipButtons_RequestsFullCreditCard) {
  base::UserActionTester user_action_tester;
  ManualFillCreditCard* server_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kMaskedServerCard, /*can_fill_directly=*/false);
  ManualFillCreditCard* virtual_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kVirtualCard, /*can_fill_directly=*/false);

  ExpectFullCardRequest(server_card, PaymentFieldType::kCardNumber);
  [mediator() didTapChipForCard:server_card
                      fieldType:ManualFillCardFieldType::kCardNumber];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_CreditCard_SelectCardNumber"),
            1);

  ExpectFullCardRequest(server_card, PaymentFieldType::kCVC);
  [mediator() didTapChipForCard:server_card
                      fieldType:ManualFillCardFieldType::kCVC];
  EXPECT_EQ(
      user_action_tester.GetActionCount("ManualFallback_CreditCard_SelectCvc"),
      1);

  ExpectFullCardRequest(virtual_card, PaymentFieldType::kCardNumber);
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kCardNumber];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_VirtualCard_SelectCardNumber"),
            1);

  ExpectFullCardRequest(virtual_card, PaymentFieldType::kCVC);
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kCVC];
  EXPECT_EQ(
      user_action_tester.GetActionCount("ManualFallback_VirtualCard_SelectCvc"),
      1);

  ExpectFullCardRequest(virtual_card, PaymentFieldType::kExpirationMonth);
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kExpirationMonth];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_VirtualCard_SelectExpirationMonth"),
            1);

  ExpectFullCardRequest(virtual_card, PaymentFieldType::kExpirationYear);
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kExpirationYear];
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_VirtualCard_SelectExpirationYear"),
            1);

  EXPECT_OCMOCK_VERIFY(content_injector());
  EXPECT_OCMOCK_VERIFY(navigation_delegate());
}

// Tests that tapping the "Autofill Form" button or sensitive chip buttons does
// not inject content or request the full credit card when injection is
// disallowed.
TEST_F(ManualFillCardMediatorTest, DidTapButtons_RespectsInjectionCheck) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ManualFillCreditCard* server_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kMaskedServerCard, /*can_fill_directly=*/false);
  ManualFillCreditCard* virtual_card = CreateManualFillCreditCard(
      CreditCard::RecordType::kVirtualCard, /*can_fill_directly=*/false);

  OCMStub([content_injector() canUserInjectInPasswordField:NO
                                             requiresHTTPS:YES])
      .andReturn(NO);
  OCMReject([content_injector() autofillFormWithSuggestion:[OCMArg any]
                                                   atIndex:0])
      .ignoringNonObjectArgs();
  OCMReject(
      [content_injector()
          userDidPickContent:[OCMArg any]
               passwordField:NO
               requiresHTTPS:NO
             jumpToNextField:NO
                  actionType:autofill::mojom::FieldActionType::kReplaceAll])
      .ignoringNonObjectArgs();
  OCMReject([navigation_delegate()
                requestFullCreditCard:[OCMArg any]
                            fieldType:PaymentFieldType::kCardNumber])
      .ignoringNonObjectArgs();

  [mediator() didTapAutofillFormButtonForCard:server_card cellIndex:0];
  [mediator() didTapChipForCard:server_card
                      fieldType:ManualFillCardFieldType::kCardNumber];
  [mediator() didTapChipForCard:server_card
                      fieldType:ManualFillCardFieldType::kCVC];
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kExpirationMonth];
  [mediator() didTapChipForCard:virtual_card
                      fieldType:ManualFillCardFieldType::kExpirationYear];

  histogram_tester.ExpectTotalCount(
      manual_fill::kAcceptedCardSuggestionHistogram, 0);
  EXPECT_EQ(user_action_tester.GetActionCount(kSuggestionAcceptedAction), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_CreditCard_SelectCardNumber"),
            0);
  EXPECT_EQ(
      user_action_tester.GetActionCount("ManualFallback_CreditCard_SelectCvc"),
      0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_VirtualCard_SelectExpirationMonth"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ManualFallback_VirtualCard_SelectExpirationYear"),
            0);
  EXPECT_OCMOCK_VERIFY(content_injector());
  EXPECT_OCMOCK_VERIFY(navigation_delegate());
}

// Tests that tapping the virtual card "Learn more" link opens the support URL
// with the localized link title.
TEST_F(ManualFillCardMediatorTest, DidTapVirtualCardLearnMore) {
  GURL expected_url = autofill::payments::GetVirtualCardEnrollmentSupportUrl();
  NSString* expected_title = l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL);

  OCMExpect([navigation_delegate()
        openURL:[OCMArg checkWithBlock:^BOOL(CrURL* url) {
          return url.gurl == expected_url;
        }]
      withTitle:expected_title]);

  [mediator() didTapVirtualCardLearnMore];

  EXPECT_OCMOCK_VERIFY(navigation_delegate());
}

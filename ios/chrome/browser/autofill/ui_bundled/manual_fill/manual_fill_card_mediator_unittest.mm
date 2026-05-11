// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_mediator.h"

#import <memory>

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/run_until.h"
#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/model/manual_fill_virtual_card_cache.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_cell+Testing.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

@interface ManualFillCardItem (TestingCard)
@property(nonatomic, readonly) ManualFillCreditCard* card;
@end

@interface ManualFillCardMediator (Testing)
- (void)onPersonalDataChanged;
@end

using autofill::CreditCard;

namespace {

constexpr char kCardNumber[] = "4234567890123456";  // Visa

constexpr char kCardGuid1[] = "00000000-0000-0000-0000-000000000001";
constexpr char kCardGuid2[] = "00000000-0000-0000-0000-000000000002";

// Returns the expected cell index accessibility label for the given
// `cell_index` and `cell_count`.
NSString* ExpectedCellIndexAccessibilityLabel(int cell_index, int cell_count) {
  return base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(IDS_IOS_MANUAL_FALLBACK_PAYMENT_CELL_INDEX),
          "position", cell_index + 1, "count", cell_count));
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
             requiresHTTPS:(BOOL)requiresHTTPS {
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
             reauthenticationModule:nil
             showAutofillFormButton:NO
                           webState:nullptr];

    consumer_ = OCMProtocolMock(@protocol(ManualFillCardConsumer));
    mediator_.consumer = consumer_;
  }

  void TearDown() override { [mediator_ disconnect]; }

  ManualFillCardMediator* mediator() { return mediator_; }

  id consumer() { return consumer_; }

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

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  id consumer_;
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

// Tests that successfully retrieving a virtual card caches the unmasked card
// in the WebState's virtual card cache.
TEST_F(ManualFillCardMediatorTest,
       OnFullCardRequestSucceeded_CachesVirtualCard) {
  // Set up ScopedTestingWebClient with FakeWebClient.
  web::ScopedTestingWebClient web_client(
      std::make_unique<web::FakeWebClient>());

  // Create a REAL WebState.
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  web::WebState::CreateParams params(profile.get());
  auto web_state = web::WebState::Create(params);

  // Set up InfoBarManager.
  InfoBarManagerImpl::CreateForWebState(web_state.get());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state.get());

  // Attach Client.
  auto client = std::make_unique<
      autofill::WithFakedFromWebState<TestChromeAutofillClient>>(
      profile.get(), web_state.get(), infobar_manager, /*bridge=*/nil);

  // Prepare test data.
  CreditCard card = autofill::test::GetVirtualCard();
  card.set_server_id("test_server_id");
  card.set_record_type(CreditCard::RecordType::kVirtualCard);

  url::Origin test_origin = url::Origin::Create(GURL("https://example.com"));
  // Set the origin in the cache before triggering the success callback.
  ManualFillVirtualCardCache::CreateForWebState(web_state.get());
  ManualFillVirtualCardCache::FromWebState(web_state.get())
      ->SetUnmaskingOrigin(test_origin);

  // Simulate the request.
  [mediator()
      onFullCardRequestSucceeded:card
                       fieldType:manual_fill::PaymentFieldType::kCardNumber
                     forWebState:web_state.get()];

  // Verify the cache.
  ManualFillVirtualCardCache* cache =
      ManualFillVirtualCardCache::FromWebState(web_state.get());
  ASSERT_TRUE(cache);

  const CreditCard* cached_card =
      cache->GetUnmaskedCard(card.server_id(), test_origin);
  ASSERT_TRUE(cached_card);
  EXPECT_EQ(cached_card->number(), card.number());
}

// Tests that successfully retrieving a non-virtual card (e.g. server card)
// does NOT cache it.
TEST_F(ManualFillCardMediatorTest,
       OnFullCardRequestSucceeded_DoesNotCacheServerCard) {
  web::ScopedTestingWebClient web_client(
      std::make_unique<web::FakeWebClient>());
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  web::WebState::CreateParams params(profile.get());
  auto web_state = web::WebState::Create(params);

  InfoBarManagerImpl::CreateForWebState(web_state.get());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state.get());

  auto client = std::make_unique<
      autofill::WithFakedFromWebState<TestChromeAutofillClient>>(
      profile.get(), web_state.get(), infobar_manager, /*bridge=*/nil);

  CreditCard card = autofill::test::GetMaskedServerCard();
  card.set_server_id("test_server_id");

  [mediator()
      onFullCardRequestSucceeded:card
                       fieldType:manual_fill::PaymentFieldType::kCardNumber
                     forWebState:web_state.get()];

  ManualFillVirtualCardCache* cache =
      ManualFillVirtualCardCache::FromWebState(web_state.get());

  if (cache) {
    EXPECT_EQ(nullptr, cache->GetUnmaskedCard(card.server_id(), url::Origin()));
  }
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
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
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

  // Set up Mock Delegate.
  id mock_delegate = OCMProtocolMock(@protocol(CardListDelegate));
  mediator().navigationDelegate = mock_delegate;

  // Create Test Data.
  CreditCard card = CreateAndSaveCreditCard(kCardGuid1);

  // Expect the call.
  OCMExpect([mock_delegate cardSelectionFinished]);

  // Invoke.
  [mediator()
      onFullCardRequestSucceeded:card
                       fieldType:manual_fill::PaymentFieldType::kCardNumber
                     forWebState:web_state.get()];

  // Verify.
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that the mediator uses the cached unmasked virtual card if available.
TEST_F(ManualFillCardMediatorTest,
       CreateManualFillCardItemsWithCachedVirtualCard) {
  // Set up ScopedTestingWebClient with FakeWebClient.
  web::ScopedTestingWebClient web_client(
      std::make_unique<web::FakeWebClient>());

  // Create a REAL WebState.
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  web::WebState::CreateParams params(profile.get());
  auto web_state = web::WebState::Create(params);

  // Re-create mediator with the web_state.
  [mediator() disconnect];
  mediator_ = [[ManualFillCardMediator alloc]
      initWithPersonalDataManager:&test_personal_data_manager_
           reauthenticationModule:nil
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

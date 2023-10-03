// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_mediator.h"

#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/prefs/pref_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const char kTestNumber[] = "4234567890123456";  // Visa

const autofill::Suggestion::Suggestion::BackendId kTestGuid =
    autofill::Suggestion::Suggestion::BackendId(
        "00000000-0000-0000-0000-000000000001");

}  // namespace

class PaymentsSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  PaymentsSuggestionBottomSheetMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()),
        personal_data_manager_(
            std::make_unique<autofill::PersonalDataManager>("en",
                                                            std::string())) {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);

    test_web_state_->SetCurrentURL(GURL("http://foo.com"));

    TestChromeBrowserState::Builder builder;
    chrome_browser_state_ = builder.Build();

    consumer_ =
        OCMProtocolMock(@protocol(PaymentsSuggestionBottomSheetConsumer));
  }

  ~PaymentsSuggestionBottomSheetMediatorTest() override {
    if (personal_data_manager_) {
      personal_data_manager_->Shutdown();
    }
    personal_data_manager_.reset();
  }

  void SetUp() override {
    PrefService* pref_service = chrome_browser_state_->GetPrefs();
    autofill::prefs::SetAutofillProfileEnabled(pref_service, true);
    autofill::prefs::SetAutofillPaymentMethodsEnabled(pref_service, true);
    personal_data_manager_->SetPrefService(pref_service);
    personal_data_manager_->SetSyncServiceForTest(&sync_service_);
  }

  void TearDown() override { [mediator_ disconnect]; }

  // Create a mediator.
  void CreateMediator() {
    web_state_list_->InsertWebState(0, std::move(test_web_state_),
                                    WebStateList::INSERT_ACTIVATE,
                                    WebStateOpener());

    mediator_ = [[PaymentsSuggestionBottomSheetMediator alloc]
        initWithWebStateList:web_state_list_.get()
                      params:autofill::FormActivityParams()
         personalDataManager:personal_data_manager_.get()];
  }

  // Add credit card to personal data manager.
  autofill::CreditCard CreateCreditCard(
      std::string guid,
      std::string number = kTestNumber,
      int64_t instrument_id = 0,
      autofill::CreditCard::RecordType record_type =
          autofill::CreditCard::RecordType::kMaskedServerCard) {
    autofill::CreditCard card = autofill::CreditCard();
    autofill::test::SetCreditCardInfo(&card, "Jane Doe", number.c_str(),
                                      autofill::test::NextMonth().c_str(),
                                      autofill::test::NextYear().c_str(), "1");
    card.set_guid(guid);
    card.set_instrument_id(instrument_id);
    card.set_record_type(record_type);

    std::unique_ptr<autofill::CreditCard> server_credit_card =
        std::make_unique<autofill::CreditCard>(card);
    personal_data_manager_->server_credit_cards_.push_back(
        std::move(server_credit_card));
    personal_data_manager_->NotifyPersonalDataObserver();

    return card;
  }

  // Create a mediator and make sure the personal data manager contains at least
  // 1 card.
  void CreateMediatorWithSuggestions() {
    CreateMediator();
    CreateCreditCard(kTestGuid.value());
    personal_data_manager_->SetSyncingForTest(true);
  }

  // Create a mediator and make sure the personal data manager contains at least
  // 1 local card.
  void CreateMediatorWithLocalCardOnlySuggestions() {
    CreateMediator();
    CreateCreditCard(kTestGuid.value(), kTestNumber, 0,
                     autofill::CreditCard::RecordType::kLocalCard);
    personal_data_manager_->SetSyncingForTest(true);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  syncer::TestSyncService sync_service_;
  id consumer_;
  std::unique_ptr<autofill::PersonalDataManager> personal_data_manager_;
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
  [mediator_ setConsumer:consumer_];

  OCMExpect([consumer_ dismiss]);
  web_state_list_.reset();
  EXPECT_OCMOCK_VERIFY(consumer_);
}

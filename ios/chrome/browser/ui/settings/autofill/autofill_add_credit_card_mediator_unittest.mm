// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/payments_data_manager.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using autofill::CreditCard;
using testing::AllOf;
using testing::Contains;
using testing::Eq;
using testing::Property;
using testing::SizeIs;

static NSString* const kTestCardName = @"TestName";
static NSString* const kTestCardNumber = @"4111111111111111";
static NSString* const kTestExpirationMonth = @"01";
static NSString* const kTestCardNickname = @"nickname";

class AutofillAddCreditCardMediatorTest : public PlatformTest {
 protected:
  AutofillAddCreditCardMediatorTest() {
    add_credit_card_mediator_delegate_mock_ =
        OCMProtocolMock(@protocol(AddCreditCardMediatorDelegate));

    add_credit_card_mediator_ = [[AutofillAddCreditCardMediator alloc]
           initWithDelegate:add_credit_card_mediator_delegate_mock_
        personalDataManager:&personal_data_manager_];
  }

  NSString* TestExpirationYear() {
    return base::SysUTF8ToNSString(autofill::test::NextYear());
  }

  autofill::TestPersonalDataManager personal_data_manager_;
  AutofillAddCreditCardMediator* add_credit_card_mediator_;
  id add_credit_card_mediator_delegate_mock_;
};

// Test saving a credit card with invalid card number.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestSavingCreditCardWithInvalidNumber) {
  // `creditCardMediatorHasInvalidCardNumber` expected to be called by
  // `add_credit_card_mediator_` if the credit card has invalid number.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidCardNumber:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:kTestCardName
                       cardNumber:@"4111111111111112"  // This is an invalid
                                                       // card number.
                  expirationMonth:kTestExpirationMonth
                   expirationYear:TestExpirationYear()
                     cardNickname:kTestCardNickname];

  // A credit card with invalid number shouldn't be saved so the number of
  // credit cards has to equal zero.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(0));

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a credit card with invalid expiration month.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestSavingCreditCardWithInvalidMonth) {
  // `creditCardMediatorHasInvalidExpirationDate` expected to be called by
  // `add_credit_card_mediator_` if the credit card has invalid expiration date.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidExpirationDate:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:kTestCardName
                       cardNumber:kTestCardNumber
                  expirationMonth:@"15"  // This is an invalid month.
                   expirationYear:TestExpirationYear()
                     cardNickname:kTestCardNickname];

  //  A credit card with invalid expiration date shouldn't be saved so the
  //  number of credit cards has to equal zero.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(0));

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a credit card with invalid expiration year.
TEST_F(AutofillAddCreditCardMediatorTest, TestSavingCreditCardWithInvalidYear) {
  // `creditCardMediatorHasInvalidExpirationDate` expected to be called by
  // `add_credit_card_mediator_` if the credit card has invalid expiration date.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidExpirationDate:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:kTestCardName
                       cardNumber:kTestCardNumber
                  expirationMonth:kTestExpirationMonth
                   expirationYear:base::SysUTF8ToNSString(
                                      autofill::test::LastYear())  // This is an
                                                                   // invalid
                                                                   // year.
                     cardNickname:kTestCardNickname];

  // A credit card with invalid expiration date shouldn't be saved so the number
  // of credit cards has to equal zero.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(0));

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a credit card with invalid nickname.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestSavingCreditCardWithInvalidNickname) {
  // `creditCardMediatorHasInvalidExpirationDate` expected to be called by
  // `add_credit_card_mediator_` if the credit card has invalid expiration date.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorHasInvalidNickname:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:kTestCardName
                       cardNumber:kTestCardNumber
                  expirationMonth:kTestExpirationMonth
                   expirationYear:TestExpirationYear()
                     cardNickname:@"cvc123"];  // This is an invalid nickname.

  // A credit card with invalid nickname shouldn't be saved so the number
  // of credit cards has to equal zero.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(0));

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving a valid credit card.
TEST_F(AutofillAddCreditCardMediatorTest, TestSavingValidCreditCard) {
  base::UserActionTester user_action_tester;

  // `creditCardMediatorDidFinish` expected to be called by
  // `add_credit_card_mediator_` if the credit card has valid data.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorDidFinish:[OCMArg any]]);

  [add_credit_card_mediator_ addCreditCardViewController:nil
                             addCreditCardWithHolderName:kTestCardName
                                              cardNumber:kTestCardNumber
                                         expirationMonth:kTestExpirationMonth
                                          expirationYear:TestExpirationYear()
                                            cardNickname:kTestCardNickname];

  // A valid credit card expected to be savd so the number of credit cards has
  // to equal one.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(1));

  EXPECT_EQ(
      user_action_tester.GetActionCount("MobileAddCreditCard.CreditCardAdded"),
      1);

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving duplicated local credit card with the same card number.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestAlreadyExistsLocalCreditCardNumber) {
  // Add an existing local credit card.
  CreditCard existing_credit_card = autofill::test::GetCreditCard();
  personal_data_manager_.payments_data_manager().AddCreditCard(
      existing_credit_card);
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(1));

  // As long as the card number is the same, the existing card will be updated.
  NSString* card_number =
      base::SysUTF16ToNSString(existing_credit_card.number());

  NSString* updated_card_name = @"Updated Card Name";
  NSString* updated_expiration_month =
      existing_credit_card.expiration_month() == 1 ? @"02" : @"01";
  NSString* updated_expiration_year =
      base::SysUTF8ToNSString(autofill::test::TenYearsFromNow());
  NSString* updated_card_nickname = @"updatednickname";

  // `creditCardMediatorDidFinish` expected to be called by
  // `add_credit_card_mediator_` if the credit card has valid data.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorDidFinish:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:updated_card_name
                       cardNumber:card_number
                  expirationMonth:updated_expiration_month
                   expirationYear:updated_expiration_year
                     cardNickname:updated_card_nickname];

  // A duplicated credit card is expected to be updated (not saved) as a new
  // card so the number of credit cards has to remain equal to one.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(1));
  const CreditCard* credit_card =
      personal_data_manager_.payments_data_manager().GetCreditCards()[0];

  EXPECT_EQ(credit_card->GetRawInfo(autofill::CREDIT_CARD_NUMBER),
            base::SysNSStringToUTF16(card_number));
  EXPECT_EQ(credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL),
            base::SysNSStringToUTF16(updated_card_name));
  EXPECT_EQ(credit_card->Expiration2DigitMonthAsString(),
            base::SysNSStringToUTF16(updated_expiration_month));
  EXPECT_EQ(credit_card->Expiration4DigitYearAsString(),
            base::SysNSStringToUTF16(updated_expiration_year));
  EXPECT_EQ(credit_card->nickname(),
            base::SysNSStringToUTF16(updated_card_nickname));

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test saving duplicated credit card with the same card number as an existing
// server card.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestAlreadyExistsServerCreditCardNumber) {
  // Add an existing server credit card.
  CreditCard server_credit_card = autofill::test::GetCreditCard();
  server_credit_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);

  personal_data_manager_.payments_data_manager().AddCreditCard(
      server_credit_card);
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(1));

  NSString* card_number = base::SysUTF16ToNSString(server_credit_card.number());

  NSString* updated_card_name = @"Updated Card Holder";
  NSString* updated_expiration_month =
      base::SysUTF8ToNSString(autofill::test::NextMonth());
  NSString* updated_expiration_year =
      base::SysUTF8ToNSString(autofill::test::TenYearsFromNow());
  NSString* updated_card_nickname = @"updatednickname";

  // `creditCardMediatorDidFinish` expected to be called by
  // `add_credit_card_mediator_` if the credit card has valid data.
  OCMExpect([add_credit_card_mediator_delegate_mock_
      creditCardMediatorDidFinish:[OCMArg any]]);

  [add_credit_card_mediator_
      addCreditCardViewController:nil
      addCreditCardWithHolderName:updated_card_name
                       cardNumber:card_number
                  expirationMonth:updated_expiration_month
                   expirationYear:updated_expiration_year
                     cardNickname:updated_card_nickname];

  // Server credit cards should not be updated. There should be a new credit
  // card in storage.
  ASSERT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(2));
  // The existing server card should still be there.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              Contains(testing::Pointee(server_credit_card)));

  auto get_card_name_full = [](const CreditCard* card) {
    return card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL);
  };

  // A new local card should be saved.
  EXPECT_THAT(
      personal_data_manager_.payments_data_manager().GetCreditCards(),
      Contains(AllOf(
          Property(&CreditCard::record_type,
                   Eq(CreditCard::RecordType::kLocalCard)),
          Property(&CreditCard::number, Eq(server_credit_card.number())),
          testing::ResultOf(get_card_name_full,
                            Eq(base::SysNSStringToUTF16(updated_card_name))),
          Property(&CreditCard::number, Eq(server_credit_card.number())),
          Property(&CreditCard::Expiration2DigitMonthAsString,
                   Eq(base::SysNSStringToUTF16(updated_expiration_month))),
          Property(&CreditCard::Expiration4DigitYearAsString,
                   Eq(base::SysNSStringToUTF16(updated_expiration_year))),
          Property(&CreditCard::nickname,
                   Eq(base::SysNSStringToUTF16(updated_card_nickname))))));

  [add_credit_card_mediator_delegate_mock_ verify];
}

// Test that the metrics for saving a credit card are recorded.
TEST_F(AutofillAddCreditCardMediatorTest, TestMetricsWhenSavingCreditCard) {
  base::HistogramTester histogram_tester;

  personal_data_manager_.payments_data_manager().AddCreditCard(
      autofill::test::GetCreditCard2());
  // Required for adding the server card.
  personal_data_manager_.payments_data_manager().SetSyncingForTest(true);
  personal_data_manager_.payments_data_manager().AddServerCreditCardForTest(
      std::make_unique<CreditCard>(autofill::test::GetMaskedServerCard()));

  int number_of_credit_cards =
      personal_data_manager_.payments_data_manager().GetCreditCards().size();
  EXPECT_EQ(number_of_credit_cards, 2);

  [add_credit_card_mediator_ addCreditCardViewController:nil
                             addCreditCardWithHolderName:kTestCardName
                                              cardNumber:kTestCardNumber
                                         expirationMonth:kTestExpirationMonth
                                          expirationYear:TestExpirationYear()
                                            cardNickname:kTestCardNickname];

  // Expect the metric to add a record based on the number of existing cards.
  histogram_tester.ExpectUniqueSample("Autofill.PaymentMethods.SettingsPage."
                                      "StoredCreditCardCountBeforeCardAdded",
                                      number_of_credit_cards, 1);
}

// Test that the metrics for saving a credit card for the first time through the
// settings are recorded accurately.
TEST_F(AutofillAddCreditCardMediatorTest,
       TestMetricsWhenSavingFirstCreditCard) {
  base::HistogramTester histogram_tester;

  // Ensure that there are no existing credit cards.
  EXPECT_THAT(personal_data_manager_.payments_data_manager().GetCreditCards(),
              SizeIs(0));

  [add_credit_card_mediator_ addCreditCardViewController:nil
                             addCreditCardWithHolderName:kTestCardName
                                              cardNumber:kTestCardNumber
                                         expirationMonth:kTestExpirationMonth
                                          expirationYear:TestExpirationYear()
                                            cardNickname:kTestCardNickname];

  // Expect the metric to add a record for a stored credit card count of 0.
  histogram_tester.ExpectUniqueSample("Autofill.PaymentMethods.SettingsPage."
                                      "StoredCreditCardCountBeforeCardAdded",
                                      0, 1);
}

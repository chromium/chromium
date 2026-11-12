// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell.h"

#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credit_card.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell+Testing.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell_mutator.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Returns a card sufficient to populate a `ManualFillCardCell`.
ManualFillCreditCard* CreateTestCard() {
  return [[ManualFillCreditCard alloc]
                          initWithGUID:@"00000000-0000-0000-0000-000000000001"
                               network:@"Visa"
                                  icon:nil
                              bankName:nil
                            cardHolder:@"Test User"
                                number:@"4111111111111111"
                      obfuscatedNumber:@"••••1111"
              networkAndLastFourDigits:nil
                        expirationYear:@"2100"
                       expirationMonth:@"01"
                                   CVC:@"123"
                            recordType:autofill::CreditCard::RecordType::
                                           kLocalCard
      cardInfoRetrievalEnrollmentState:
          autofill::CreditCard::CardInfoRetrievalEnrollmentState::
              kRetrievalUnenrolledAndNotEligible
                       canFillDirectly:YES];
}

}  // namespace

class ManualFillCardCellTest : public PlatformTest {
 protected:
  void SetUp() override {
    mutator_ = OCMProtocolMock(@protocol(ManualFillCardCellMutator));
    card_ = CreateTestCard();
    cell_ = [[ManualFillCardCell alloc] init];
    [cell_ setUpWithCreditCard:card_
                            mutator:mutator_
                        menuActions:@[]
                          cellIndex:0
        cellIndexAccessibilityLabel:@""
             showAutofillFormButton:YES];
  }

  OCMockObject<ManualFillCardCellMutator>* mutator_;
  ManualFillCreditCard* card_;
  ManualFillCardCell* cell_;
};

// Tests that tapping the cell buttons forwards the expected actions to the
// mutator.
TEST_F(ManualFillCardCellTest, TappingButtonsNotifiesMutator) {
  OCMExpect([mutator_ didTapAutofillFormButtonForCard:card_ cellIndex:0]);
  [cell_.autofillFormButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  OCMExpect([mutator_ didTapChipForCard:card_
                              fieldType:ManualFillCardFieldType::kCardNumber]);
  [cell_.cardNumberButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  OCMExpect([mutator_
      didTapChipForCard:card_
              fieldType:ManualFillCardFieldType::kCardholderName]);
  [cell_.cardholderButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  OCMExpect([mutator_ didTapChipForCard:card_
                              fieldType:ManualFillCardFieldType::kCVC]);
  [cell_.cvcButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  OCMExpect([mutator_
      didTapChipForCard:card_
              fieldType:ManualFillCardFieldType::kExpirationMonth]);
  [cell_.expirationMonthButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  OCMExpect([mutator_
      didTapChipForCard:card_
              fieldType:ManualFillCardFieldType::kExpirationYear]);
  [cell_.expirationYearButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mutator_);
}

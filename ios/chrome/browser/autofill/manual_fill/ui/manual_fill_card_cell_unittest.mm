// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell.h"

#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credit_card.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/card_list_delegate.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_card_cell+Testing.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Returns a card sufficient to populate a `ManualFillCardCell`.
ManualFillCreditCard* TestLocalCard() {
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
                                   CVC:nil
                            recordType:autofill::CreditCard::RecordType::
                                           kLocalCard
      cardInfoRetrievalEnrollmentState:
          autofill::CreditCard::CardInfoRetrievalEnrollmentState::
              kRetrievalUnenrolledAndNotEligible
                       canFillDirectly:YES];
}

// Returns a masked server card that cannot be filled directly.
ManualFillCreditCard* TestServerCard() {
  return [[ManualFillCreditCard alloc]
                          initWithGUID:@"00000000-0000-0000-0000-000000000002"
                               network:@"Visa"
                                  icon:nil
                              bankName:nil
                            cardHolder:@"Test User"
                                number:@"4111111111111111"
                      obfuscatedNumber:@"••••1111"
              networkAndLastFourDigits:nil
                        expirationYear:@"2100"
                       expirationMonth:@"01"
                                   CVC:@"•••"
                            recordType:autofill::CreditCard::RecordType::
                                           kMaskedServerCard
      cardInfoRetrievalEnrollmentState:
          autofill::CreditCard::CardInfoRetrievalEnrollmentState::
              kRetrievalUnenrolledAndNotEligible
                       canFillDirectly:NO];
}

// Returns a virtual card.
ManualFillCreditCard* TestVirtualCard() {
  return [[ManualFillCreditCard alloc]
                          initWithGUID:@"00000000-0000-0000-0000-000000000003"
                               network:@"Visa"
                                  icon:nil
                              bankName:nil
                            cardHolder:@"Test User"
                                number:@"4111111111111111"
                      obfuscatedNumber:@"••••1111"
              networkAndLastFourDigits:nil
                        expirationYear:@"2100"
                       expirationMonth:@"01"
                                   CVC:nil
                            recordType:autofill::CreditCard::RecordType::
                                           kVirtualCard
      cardInfoRetrievalEnrollmentState:
          autofill::CreditCard::CardInfoRetrievalEnrollmentState::
              kRetrievalUnenrolledAndNotEligible
                       canFillDirectly:NO];
}

// Returns the "Autofill Form" button of `cell`.
UIButton* AutofillFormButton(ManualFillCardCell* cell) {
  return cell.autofillFormButton;
}

// Returns the CVC button of `cell`.
UIButton* CvcButton(ManualFillCardCell* cell) {
  return cell.cvcButton;
}

// Returns the expiration month button of `cell`.
UIButton* ExpirationMonthButton(ManualFillCardCell* cell) {
  return cell.expirationMonthButton;
}

// Returns the expiration year button of `cell`.
UIButton* ExpirationYearButton(ManualFillCardCell* cell) {
  return cell.expirationYearButton;
}

}  // namespace

class ManualFillCardCellTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    content_injector_ = OCMProtocolMock(@protocol(ManualFillContentInjector));
    navigation_delegate_ = OCMProtocolMock(@protocol(CardListDelegate));
    card_ = TestLocalCard();
    cell_ = [[ManualFillCardCell alloc] init];
    [cell_ setUpWithCreditCard:card_
                    contentInjector:content_injector_
                 navigationDelegate:navigation_delegate_
                        menuActions:@[]
                          cellIndex:0
        cellIndexAccessibilityLabel:@""
             showAutofillFormButton:YES];
  }

  void TearDown() override {
    [cell_ prepareForReuse];
    cell_ = nil;
    card_ = nil;
    navigation_delegate_ = nil;
    content_injector_ = nil;
    PlatformTest::TearDown();
  }

  id<ManualFillContentInjector> content_injector_;
  id<CardListDelegate> navigation_delegate_;
  ManualFillCreditCard* card_;
  ManualFillCardCell* cell_;
};

// Tests that tapping the "Autofill Form" button forwards the suggestion to the
// content injector when injection is allowed.
TEST_F(ManualFillCardCellTest, AutofillFormButtonForwardsSuggestion) {
  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(YES);
  OCMExpect([content_injector_ autofillFormWithSuggestion:[OCMArg any]
                                                  atIndex:0]);

  [AutofillFormButton(cell_)
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
}

// Tests that tapping the "Autofill Form" button does not forward the suggestion
// to the content injector when injection is not allowed.
TEST_F(ManualFillCardCellTest, AutofillFormButtonRespectsInjectionCheck) {
  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(NO);
  OCMReject([content_injector_ autofillFormWithSuggestion:[OCMArg any]
                                                  atIndex:0]);

  [AutofillFormButton(cell_)
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
}

// Tests that tapping the CVC button for a server card does not request the full
// credit card when injection is not allowed.
TEST_F(ManualFillCardCellTest, CvcButtonForServerCardRespectsInjectionCheck) {
  ManualFillCreditCard* server_card = TestServerCard();
  [cell_ prepareForReuse];
  [cell_ setUpWithCreditCard:server_card
                  contentInjector:content_injector_
               navigationDelegate:navigation_delegate_
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:@""
           showAutofillFormButton:YES];

  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(NO);
  OCMReject([navigation_delegate_
      requestFullCreditCard:[OCMArg any]
                  fieldType:manual_fill::PaymentFieldType::kCVC]);

  [CvcButton(cell_) sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
  EXPECT_OCMOCK_VERIFY(navigation_delegate_);
}

// Tests that tapping the CVC button for a server card requests the full credit
// card when injection is allowed.
TEST_F(ManualFillCardCellTest, CvcButtonForServerCardRequestsFullCreditCard) {
  ManualFillCreditCard* server_card = TestServerCard();
  [cell_ prepareForReuse];
  [cell_ setUpWithCreditCard:server_card
                  contentInjector:content_injector_
               navigationDelegate:navigation_delegate_
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:@""
           showAutofillFormButton:YES];

  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(YES);
  OCMExpect([navigation_delegate_
      requestFullCreditCard:[OCMArg any]
                  fieldType:manual_fill::PaymentFieldType::kCVC]);

  [CvcButton(cell_) sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
  EXPECT_OCMOCK_VERIFY(navigation_delegate_);
}

// Tests that tapping the expiration month button for a virtual card does not
// request the full credit card when injection is not allowed.
TEST_F(ManualFillCardCellTest,
       ExpirationMonthButtonForVirtualCardRespectsInjectionCheck) {
  ManualFillCreditCard* virtual_card = TestVirtualCard();
  [cell_ prepareForReuse];
  [cell_ setUpWithCreditCard:virtual_card
                  contentInjector:content_injector_
               navigationDelegate:navigation_delegate_
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:@""
           showAutofillFormButton:YES];

  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(NO);
  OCMReject([navigation_delegate_
      requestFullCreditCard:[OCMArg any]
                  fieldType:manual_fill::PaymentFieldType::kExpirationMonth]);

  [ExpirationMonthButton(cell_)
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
  EXPECT_OCMOCK_VERIFY(navigation_delegate_);
}

// Tests that tapping the expiration month button for a virtual card requests
// the full credit card when injection is allowed.
TEST_F(ManualFillCardCellTest,
       ExpirationMonthButtonForVirtualCardRequestsFullCreditCard) {
  ManualFillCreditCard* virtual_card = TestVirtualCard();
  [cell_ prepareForReuse];
  [cell_ setUpWithCreditCard:virtual_card
                  contentInjector:content_injector_
               navigationDelegate:navigation_delegate_
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:@""
           showAutofillFormButton:YES];

  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(YES);
  OCMExpect([navigation_delegate_
      requestFullCreditCard:[OCMArg any]
                  fieldType:manual_fill::PaymentFieldType::kExpirationMonth]);

  [ExpirationMonthButton(cell_)
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
  EXPECT_OCMOCK_VERIFY(navigation_delegate_);
}

// Tests that tapping the expiration year button for a virtual card does not
// request the full credit card when injection is not allowed.
TEST_F(ManualFillCardCellTest,
       ExpirationYearButtonForVirtualCardRespectsInjectionCheck) {
  ManualFillCreditCard* virtual_card = TestVirtualCard();
  [cell_ prepareForReuse];
  [cell_ setUpWithCreditCard:virtual_card
                  contentInjector:content_injector_
               navigationDelegate:navigation_delegate_
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:@""
           showAutofillFormButton:YES];

  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(NO);
  OCMReject([navigation_delegate_
      requestFullCreditCard:[OCMArg any]
                  fieldType:manual_fill::PaymentFieldType::kExpirationYear]);

  [ExpirationYearButton(cell_)
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
  EXPECT_OCMOCK_VERIFY(navigation_delegate_);
}

// Tests that tapping the expiration year button for a virtual card requests
// the full credit card when injection is allowed.
TEST_F(ManualFillCardCellTest,
       ExpirationYearButtonForVirtualCardRequestsFullCreditCard) {
  ManualFillCreditCard* virtual_card = TestVirtualCard();
  [cell_ prepareForReuse];
  [cell_ setUpWithCreditCard:virtual_card
                  contentInjector:content_injector_
               navigationDelegate:navigation_delegate_
                      menuActions:@[]
                        cellIndex:0
      cellIndexAccessibilityLabel:@""
           showAutofillFormButton:YES];

  OCMExpect([content_injector_ canUserInjectInPasswordField:NO
                                              requiresHTTPS:YES])
      .andReturn(YES);
  OCMExpect([navigation_delegate_
      requestFullCreditCard:[OCMArg any]
                  fieldType:manual_fill::PaymentFieldType::kExpirationYear]);

  [ExpirationYearButton(cell_)
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(content_injector_);
  EXPECT_OCMOCK_VERIFY(navigation_delegate_);
}

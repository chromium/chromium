// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_test_utils.h"
#import "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_cell+Testing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

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

}  // namespace

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
             showAutofillFormButton:NO];

    consumer_ = OCMProtocolMock(@protocol(ManualFillCardConsumer));
    mediator_.consumer = consumer_;
  }

  ManualFillCardMediator* mediator() { return mediator_; }

  id consumer() { return consumer_; }

  CreditCard CreateAndSaveCreditCard(std::string guid,
                                     bool enrolled_for_virtual_card = false) {
    CreditCard card;
    autofill::test::SetCreditCardInfo(&card, "Jane Doe", kCardNumber,
                                      autofill::test::NextMonth().c_str(),
                                      autofill::test::NextYear().c_str(), "1");
    card.set_guid(guid);
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

 private:
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

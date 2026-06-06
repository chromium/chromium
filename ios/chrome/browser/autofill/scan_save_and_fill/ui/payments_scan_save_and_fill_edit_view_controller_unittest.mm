// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_edit_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/task_environment.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mutator.h"
#import "ios/chrome/browser/autofill/ui_bundled/util/autofill_credit_card_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

@interface PaymentsScanSaveAndFillEditViewController (Testing)
- (void)tableViewItemDidChange:(TableViewTextEditItem*)item;
- (void)updateSaveButtonStatus;
- (void)validateAndReconfigureItems:(NSArray<TableViewItem*>*)items;
- (void)didTapSave;
- (void)didTapCancel;
@end

@interface TableViewTextEditItem (Testing)
@property(nonatomic, assign) BOOL hasValidText;
@end

namespace {
// Test constants for sections to avoid magic numbers.
const NSInteger kSectionIdentifierCardDetails = 0;
}  // namespace

@interface FakePaymentsScanSaveAndFillEditMutator
    : NSObject <SaveCardBottomSheetMutator>
@property(nonatomic, assign) BOOL didCancelCalled;
@property(nonatomic, assign) BOOL saveAndFillCalled;
@property(nonatomic, assign) autofill::payments::PaymentsAutofillClient::
    UserProvidedCardSaveAndFillDetails savedDetails;
@property(nonatomic, weak) id<SaveCardBottomSheetConsumer> consumer;
@end

@implementation FakePaymentsScanSaveAndFillEditMutator {
  NSString* _cardNumber;
  NSString* _expirationDate;
  NSString* _cardholderName;
  NSString* _cvc;
  NSString* _nickname;
}

- (void)didAccept {
  // Normal save, not called by this view controller for save-and-fill.
}

- (void)didCancel {
  self.didCancelCalled = YES;
}

- (void)didUpdateValue:(NSString*)value
              forField:(AutofillCreditCardUIType)type {
  switch (type) {
    case AutofillCreditCardUIType::kNumber:
      _cardNumber = value;
      break;
    case AutofillCreditCardUIType::kExpMonth:
      _expirationDate = value;
      break;
    case AutofillCreditCardUIType::kSecurityCode:
      _cvc = value;
      break;
    case AutofillCreditCardUIType::kNickname:
      _nickname = value;
      break;
    case AutofillCreditCardUIType::kFullName:
      _cardholderName = value;
      break;
    default:
      break;
  }

  BOOL isValid = [self isValidValue:value forField:type];
  [self.consumer setField:type isValid:isValid errorMessage:nil];
}

- (void)didTapSave {
  self.saveAndFillCalled = YES;
  autofill::payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
      details;
  details.card_number = base::SysNSStringToUTF16(_cardNumber);
  details.cardholder_name = base::SysNSStringToUTF16(_cardholderName);
  details.cvc = base::SysNSStringToUTF16(_cvc);
  if (_cvc.length > 0) {
    details.security_code = base::SysNSStringToUTF16(_cvc);
  }
  if (_nickname.length > 0) {
    details.nickname = base::SysNSStringToUTF16(_nickname);
  }

  // Parse expiration date.
  NSString* cleanExpDate = [[_expirationDate
      componentsSeparatedByCharactersInSet:[[NSCharacterSet
                                               decimalDigitCharacterSet]
                                               invertedSet]]
      componentsJoinedByString:@""];
  NSString* parsedMonth = @"";
  NSString* parsedYear = @"";
  if (cleanExpDate.length >= 4) {
    parsedMonth = [cleanExpDate substringToIndex:2];
    parsedYear = [cleanExpDate substringFromIndex:2];
  }
  details.expiration_date_month = base::SysNSStringToUTF16(parsedMonth);
  details.expiration_date_year = base::SysNSStringToUTF16(parsedYear);

  self.savedDetails = details;
}

- (BOOL)isValidValue:(NSString*)value forField:(AutofillCreditCardUIType)type {
  switch (type) {
    case AutofillCreditCardUIType::kNumber:
      return value.length >= 12;
    case AutofillCreditCardUIType::kExpMonth: {
      NSString* digitsOnlyExpDate = [[value
          componentsSeparatedByCharactersInSet:[[NSCharacterSet
                                                   decimalDigitCharacterSet]
                                                   invertedSet]]
          componentsJoinedByString:@""];
      if (digitsOnlyExpDate.length >= 4) {
        NSString* expMonth = [digitsOnlyExpDate substringToIndex:2];
        NSString* expYear = [digitsOnlyExpDate substringFromIndex:2];

        int month = [expMonth intValue];
        int year = [expYear intValue];

        return month >= 1 && month <= 12 && year >= 0 &&
               (expYear.length == 2 || expYear.length == 4);
      }
      return NO;
    }
    case AutofillCreditCardUIType::kNickname:
      return YES;  // Nickname is optional
    case AutofillCreditCardUIType::kSecurityCode:
      return value.length == 0 || value.length == 3 || value.length == 4;
    default:
      return YES;
  }
}

@end

class PaymentsScanSaveAndFillEditViewControllerTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;

  void SetUp() override {
    PlatformTest::SetUp();

    // Initialize Mocks and Fakes.
    mock_data_source_ =
        OCMProtocolMock(@protocol(SaveCardBottomSheetDataSource));
    mock_delegate_ = OCMProtocolMock(@protocol(SaveCardBottomSheetDelegate));
    mock_mutator_ = OCMProtocolMock(@protocol(SaveCardBottomSheetMutator));
    fake_mutator_ = [[FakePaymentsScanSaveAndFillEditMutator alloc] init];

    // Instantiate the target ViewController.
    view_controller_ = [[PaymentsScanSaveAndFillEditViewController alloc] init];
    fake_mutator_.consumer = view_controller_;

    // Inject properties.
    view_controller_.dataSource = mock_data_source_;
    view_controller_.mutator = mock_mutator_;
    view_controller_.delegate = mock_delegate_;
  }

  void TearDown() override {
    [view_controller_ viewDidDisappear:NO];
    PlatformTest::TearDown();
  }

  void CreateController() { [view_controller_ loadViewIfNeeded]; }

  TableViewTextEditItem* GetItem(int section, int row) {
    NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();
    NSArray* cardDetailsItems =
        [snapshot itemIdentifiersInSectionWithIdentifier:@(section)];
    return base::apple::ObjCCastStrict<TableViewTextEditItem>(
        cardDetailsItems[row]);
  }

  // Helper method to extract the diffable data source snapshot.
  NSDiffableDataSourceSnapshot* GetSnapshot() {
    UITableViewDiffableDataSource* dataSource =
        base::apple::ObjCCastStrict<UITableViewDiffableDataSource>(
            view_controller_.tableView.dataSource);
    return [dataSource snapshot];
  }

  PaymentsScanSaveAndFillEditViewController* view_controller_;
  id mock_data_source_;
  id mock_delegate_;
  id mock_mutator_;
  FakePaymentsScanSaveAndFillEditMutator* fake_mutator_;
};

// Tests if the UI model is loaded correctly with the expected sections and
// cells.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestLoadModel) {
  // Triggers viewDidLoad and loadModel.
  [view_controller_ loadViewIfNeeded];

  // Retrieve the snapshot representing the current UI state.
  NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();

  // Verify the number of sections (only Card Details).
  EXPECT_EQ(1u, snapshot.sectionIdentifiers.count);

  // Card Details.
  NSArray* cardDetailsItems = [snapshot
      itemIdentifiersInSectionWithIdentifier:@(kSectionIdentifierCardDetails)];
  EXPECT_EQ(5u, cardDetailsItems.count);
}

// Tests if the UI Model is updated correctly when data is passed from the
// Scanner Consumer.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestScannerUpdatesUI) {
  // Triggers viewDidLoad and loadModel.
  [view_controller_ loadViewIfNeeded];

  // Simulate incoming scanned results.
  [view_controller_ setCreditCardNumber:@"4111222233334444"
                        expirationMonth:@"12"
                         expirationYear:@"29"];

  NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();
  NSArray* cardDetailsItems = [snapshot
      itemIdentifiersInSectionWithIdentifier:@(kSectionIdentifierCardDetails)];

  // Card Number.
  TableViewTextEditItem* numberItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[0]);
  EXPECT_NSEQ(@"4111222233334444", numberItem.textFieldValue);

  // Expiration Date.
  TableViewTextEditItem* expItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[1]);
  EXPECT_NSEQ(@"12/29", expItem.textFieldValue);
}

// Tests if the UI model correctly configures placeholder texts for the fields.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestPlaceholders) {
  [view_controller_ loadViewIfNeeded];

  NSDiffableDataSourceSnapshot* snapshot = GetSnapshot();
  NSArray* cardDetailsItems =
      [snapshot itemIdentifiersInSectionWithIdentifier:@(0)];

  // Card Number.
  TableViewTextEditItem* numberItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[0]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER),
              numberItem.textFieldPlaceholder);

  // Expiration Date.
  TableViewTextEditItem* expItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[1]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AUTOFILL_SCAN_CARD_EXP_DATE),
              expItem.fieldNameLabelText);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_SCAN_CARD_PLACEHOLDER_EXPIRY_DATE),
              expItem.textFieldPlaceholder);

  // Cardholder Name.
  TableViewTextEditItem* nameItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[2]);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_HOLDER_NAME),
              nameItem.textFieldPlaceholder);

  // CVC.
  TableViewTextEditItem* cvcItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[3]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_OPTIONAL),
      cvcItem.textFieldPlaceholder);

  // Nickname.
  TableViewTextEditItem* nicknameItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(cardDetailsItems[4]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_OPTIONAL),
      nicknameItem.textFieldPlaceholder);
}

// Tests the behavior when the "Cancel" button is tapped.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestDidTapCancel) {
  [view_controller_ loadViewIfNeeded];

  // Expect that cancel was called on the mutator.
  [[mock_mutator_ expect] didCancel];

  // Simulate tapping the cancel button.
  UIBarButtonItem* cancelItem =
      view_controller_.navigationItem.leftBarButtonItem;
  ASSERT_TRUE(cancelItem);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [cancelItem.target performSelector:cancelItem.action withObject:cancelItem];
#pragma clang diagnostic pop

  // Verify that expectations are met.
  [mock_mutator_ verify];
}

// Tests the behavior when the "Save and Fill" button is tapped.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestDidTapSave) {
  CreateController();
  view_controller_.mutator = fake_mutator_;

  // Initially populate fields using scanner simulation.
  [view_controller_ setCreditCardNumber:@"4111222233334444"
                        expirationMonth:@"12"
                         expirationYear:@"29"];

  // Populate optional fields manually.
  TableViewTextEditItem* nameItem = GetItem(kSectionIdentifierCardDetails, 2);
  nameItem.textFieldValue = @"John Doe";
  TableViewTextEditItem* cvcItem = GetItem(kSectionIdentifierCardDetails, 3);
  cvcItem.textFieldValue = @"123";
  TableViewTextEditItem* nicknameItem =
      GetItem(kSectionIdentifierCardDetails, 4);
  nicknameItem.textFieldValue = @"My Card";

  [view_controller_
      validateAndReconfigureItems:@[ nameItem, cvcItem, nicknameItem ]];

  [view_controller_ performSelector:@selector(didTapSave)];

  // Wait for the mutator task to execute.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return fake_mutator_.saveAndFillCalled; }));

  // Verify that onUpdatedAndAcceptedForSaveAndFill was called with correct
  // details.
  EXPECT_TRUE(fake_mutator_.saveAndFillCalled);
  EXPECT_EQ(u"4111222233334444", fake_mutator_.savedDetails.card_number);
  EXPECT_EQ(u"12", fake_mutator_.savedDetails.expiration_date_month);
  EXPECT_EQ(u"29", fake_mutator_.savedDetails.expiration_date_year);
  EXPECT_EQ(u"John Doe", fake_mutator_.savedDetails.cardholder_name);
  EXPECT_EQ(u"123", fake_mutator_.savedDetails.cvc);
  EXPECT_TRUE(fake_mutator_.savedDetails.security_code.has_value());
  EXPECT_EQ(u"123", fake_mutator_.savedDetails.security_code.value());
  EXPECT_TRUE(fake_mutator_.savedDetails.nickname.has_value());
  EXPECT_EQ(u"My Card", fake_mutator_.savedDetails.nickname.value());

  // Verify that the save button transitioned to the loading state.
  UIButton* saveButton = [view_controller_ valueForKey:@"_saveButton"];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_NSEQ(nil, [saveButton valueForKey:@"title"]);
  EXPECT_TRUE(saveButton.configuration.showsActivityIndicator);
}

// Tests the behavior of showLoadingStateWithAccessibilityLabel.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestShowLoadingState) {
  CreateController();

  [view_controller_ showLoadingStateWithAccessibilityLabel:@"Loading"];

  UIButton* saveButton = [view_controller_ valueForKey:@"_saveButton"];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_NSEQ(nil, [saveButton valueForKey:@"title"]);
  EXPECT_TRUE(saveButton.configuration.showsActivityIndicator);
  EXPECT_NSEQ(@"Loading", saveButton.accessibilityLabel);
}

// Tests the behavior of showConfirmationState.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest,
       TestShowConfirmationState) {
  CreateController();

  [view_controller_ showConfirmationState];

  UIButton* saveButton = [view_controller_ valueForKey:@"_saveButton"];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_NSEQ(nil, [saveButton valueForKey:@"title"]);
  EXPECT_FALSE(saveButton.configuration.showsActivityIndicator);

  // PrimaryButtonImageCheckmark corresponds to confirmation state
  NSNumber* imageValue = [saveButton valueForKey:@"primaryButtonImage"];
  EXPECT_EQ(static_cast<int>(PrimaryButtonImageCheckmark),
            [imageValue intValue]);
}

// Tests if the expiration date properly validates.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest,
       TestExpirationDateValidation) {
  CreateController();
  view_controller_.mutator = fake_mutator_;

  TableViewTextEditItem* expItem = GetItem(kSectionIdentifierCardDetails, 1);
  id<TableViewTextEditItemDelegate> delegate =
      (id<TableViewTextEditItemDelegate>)view_controller_;

  // Initial load: empty, so it must be valid.
  EXPECT_TRUE(expItem.hasValidText);

  // Focus the field.
  [delegate tableViewItemDidBeginEditing:expItem];
  EXPECT_TRUE(expItem.hasValidText);

  // Enter invalid data "122". While editing, errors are visually suppressed.
  expItem.textFieldValue = @"122";
  [delegate tableViewItemDidChange:expItem];
  EXPECT_TRUE(expItem.hasValidText);

  // End editing: now it should resolve to invalid visual state.
  [delegate tableViewItemDidEndEditing:expItem];
  EXPECT_FALSE(expItem.hasValidText);

  // Re-focus the field: error should immediately vanish visually again.
  [delegate tableViewItemDidBeginEditing:expItem];
  EXPECT_TRUE(expItem.hasValidText);

  // Correct it while focused. Still valid visually.
  expItem.textFieldValue = @"12/29";
  [delegate tableViewItemDidChange:expItem];
  EXPECT_TRUE(expItem.hasValidText);

  // End editing: truly valid now.
  [delegate tableViewItemDidEndEditing:expItem];
  EXPECT_TRUE(expItem.hasValidText);
}

// Tests the validation timing and focus behavior.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestValidationTiming) {
  CreateController();
  view_controller_.mutator = fake_mutator_;

  TableViewTextEditItem* numberItem = GetItem(kSectionIdentifierCardDetails, 0);
  id<TableViewTextEditItemDelegate> delegate =
      (id<TableViewTextEditItemDelegate>)view_controller_;

  // Initial state: completely empty, must be visually valid (no red
  // exclamation).
  EXPECT_TRUE(numberItem.hasValidText);

  // Scenario: POPULATED BY OCR.
  // Field is filled automatically (NOT focused). Since it's non-empty and
  // invalid, validation SHOULD be displayed immediately.
  numberItem.textFieldValue = @"4111";  // Too short
  [view_controller_ validateAndReconfigureItems:@[ numberItem ]];
  EXPECT_FALSE(numberItem.hasValidText);

  // Begin editing: The error must IMMEDIATELY disappear when the user taps into
  // it.
  [delegate tableViewItemDidBeginEditing:numberItem];
  EXPECT_TRUE(numberItem.hasValidText);

  // While typing, errors remain hidden.
  numberItem.textFieldValue = @"411122";
  [delegate tableViewItemDidChange:numberItem];
  EXPECT_TRUE(numberItem.hasValidText);

  // End editing: The actual error reappears now that editing finished.
  [delegate tableViewItemDidEndEditing:numberItem];
  EXPECT_FALSE(numberItem.hasValidText);
}

// Tests if the save button's state correctly updates based on validations.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestSaveButtonState) {
  CreateController();
  [view_controller_ loadViewIfNeeded];

  UIButton* saveButton = [view_controller_ valueForKey:@"_saveButton"];

  [view_controller_ setSaveButtonEnabled:YES];
  EXPECT_TRUE(saveButton.enabled);

  [view_controller_ setSaveButtonEnabled:NO];
  EXPECT_FALSE(saveButton.enabled);
}

// Tests that the accept action is logged when the save button is tapped.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestMetricsOnSave) {
  base::HistogramTester histogram_tester;
  CreateController();

  [view_controller_ didTapSave];

  histogram_tester.ExpectUniqueSample(
      "IOS.ScanCardOfferToSave",
      static_cast<int>(ScanCardOfferToSaveAction::kAccept), 1);
}

// Tests that the reject action is logged when the cancel button is tapped.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestMetricsOnCancel) {
  base::HistogramTester histogram_tester;
  CreateController();

  [view_controller_ didTapCancel];

  histogram_tester.ExpectUniqueSample(
      "IOS.ScanCardOfferToSave",
      static_cast<int>(ScanCardOfferToSaveAction::kReject), 1);
}

// Tests that the ignore action is logged when the view disappears without any
// prior action.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestMetricsOnDismiss) {
  base::HistogramTester histogram_tester;
  CreateController();

  [view_controller_ viewDidDisappear:NO];

  histogram_tester.ExpectUniqueSample(
      "IOS.ScanCardOfferToSave",
      static_cast<int>(ScanCardOfferToSaveAction::kIgnore), 1);
}

// Tests that user actions are logged only once.
TEST_F(PaymentsScanSaveAndFillEditViewControllerTest,
       TestMetricsLoggedOnlyOnce) {
  base::HistogramTester histogram_tester;
  CreateController();

  [view_controller_ didTapSave];
  [view_controller_ viewDidDisappear:NO];

  histogram_tester.ExpectUniqueSample(
      "IOS.ScanCardOfferToSave",
      static_cast<int>(ScanCardOfferToSaveAction::kAccept), 1);

  histogram_tester.ExpectBucketCount(
      "IOS.ScanCardOfferToSave",
      static_cast<int>(ScanCardOfferToSaveAction::kIgnore), 0);
}

TEST_F(PaymentsScanSaveAndFillEditViewControllerTest, TestMetricsOnScan) {
  base::HistogramTester histogram_tester;
  [view_controller_ loadViewIfNeeded];

  NSCalendar* calendar = NSCalendar.currentCalendar;
  NSDateComponents* calendarComponents = [calendar components:NSCalendarUnitYear
                                                     fromDate:[NSDate date]];
  NSInteger currentYear = [calendarComponents year];
  NSString* expirationYear =
      [NSString stringWithFormat:@"%ld", (long)currentYear];

  // Simulate incoming scanned results.
  [view_controller_ setCreditCardNumber:@"4242424242424242"
                        expirationMonth:@"12"
                         expirationYear:expirationYear];

  histogram_tester.ExpectUniqueSample("IOS.ScanCardOfferToSave.ValidNumber",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("IOS.ScanCardOfferToSave.ValidExpMonth",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("IOS.ScanCardOfferToSave.ValidExpYear",
                                      true, 1);
  histogram_tester.ExpectTotalCount("IOS.ScanCard.EndToEndLatency", 1);
}

TEST_F(PaymentsScanSaveAndFillEditViewControllerTest,
       TestMetricsOnScanInvalid) {
  base::HistogramTester histogram_tester;
  [view_controller_ loadViewIfNeeded];

  // Simulate incoming scanned results.
  [view_controller_ setCreditCardNumber:@"122"
                        expirationMonth:@"13"
                         expirationYear:@"20"];

  histogram_tester.ExpectUniqueSample("IOS.ScanCardOfferToSave.ValidNumber",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("IOS.ScanCardOfferToSave.ValidExpMonth",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("IOS.ScanCardOfferToSave.ValidExpYear",
                                      false, 1);
}

TEST_F(PaymentsScanSaveAndFillEditViewControllerTest,
       TestMetricsOnSaveWithEdits) {
  base::HistogramTester histogram_tester;
  CreateController();
  [view_controller_ loadViewIfNeeded];

  // Populate initial scanned data.
  [view_controller_ setCreditCardNumber:@"1234567812345678"
                        expirationMonth:@"12"
                         expirationYear:@"26"];

  // Simulate manual edits.
  TableViewTextEditItem* numberItem =
      [view_controller_ valueForKey:@"_cardNumberItem"];
  numberItem.textFieldValue = @"1234567812340000";

  TableViewTextEditItem* expDateItem =
      [view_controller_ valueForKey:@"_expirationDateItem"];
  expDateItem.textFieldValue = @"11/26";

  [view_controller_ didTapSave];

  histogram_tester.ExpectUniqueSample("IOS.ScannedCard.NumberEdited", true, 1);
  histogram_tester.ExpectUniqueSample("IOS.ScannedCard.ExpMonthEdited", true,
                                      1);
  histogram_tester.ExpectUniqueSample("IOS.ScannedCard.ExpYearEdited", false,
                                      1);
}

TEST_F(PaymentsScanSaveAndFillEditViewControllerTest,
       TestMetricsOnSaveWithoutEdits) {
  base::HistogramTester histogram_tester;
  CreateController();
  [view_controller_ loadViewIfNeeded];

  // Populate initial scanned data.
  [view_controller_ setCreditCardNumber:@"1234567812345678"
                        expirationMonth:@"12"
                         expirationYear:@"26"];

  [view_controller_ didTapSave];

  histogram_tester.ExpectUniqueSample("IOS.ScannedCard.NumberEdited", false, 1);
  histogram_tester.ExpectUniqueSample("IOS.ScannedCard.ExpMonthEdited", false,
                                      1);
  histogram_tester.ExpectUniqueSample("IOS.ScannedCard.ExpYearEdited", false,
                                      1);
}

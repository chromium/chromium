// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/autofill_credit_card_edit_item.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_add_credit_card_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

NSString* const kAddCreditCardViewID = @"kAddCreditCardViewID";
NSString* const kSettingsAddCreditCardButtonID =
    @"kSettingsAddCreditCardButtonID";
NSString* const kSettingsAddCreditCardCancelButtonID =
    @"kSettingsAddCreditCardCancelButtonID";

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierName = kSectionIdentifierEnumZero,
  SectionIdentifierCreditCardDetails,
  SectionIdentifierCameraButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeName = kItemTypeEnumZero,
  ItemTypeCardNumber,
  ItemTypeExpirationMonth,
  ItemTypeExpirationYear,
  ItemTypeCardNickname,
  ItemTypeCardCvc,
  ItemTypeUseCameraButton,
};

}  // namespace

@interface AutofillAddCreditCardViewController () <
    TableViewTextEditItemDelegate>

@end

@implementation AutofillAddCreditCardViewController {
  // The AddCreditCardViewControllerDelegate for this ViewController.
  __weak id<AddCreditCardViewControllerDelegate> _delegate;

  // The card holder name updated with the text in tableview cell.
  NSString* _cardHolderName;

  // The card number in the UI.
  NSString* _cardNumber;

  // The expiration month in the UI.
  NSString* _expirationMonth;

  // The expiration year in the UI.
  NSString* _expirationYear;

  // The user provided nickname for the credit card.
  NSString* _cardNickname;

  // The card CVC in the UI.
  NSString* _cardCvc;
}

- (instancetype)initWithDelegate:
    (id<AddCreditCardViewControllerDelegate>)delegate {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _delegate = delegate;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.tableView.accessibilityIdentifier = kAddCreditCardViewID;

  self.navigationItem.title = l10n_util::GetNSString(
      IDS_IOS_CREDIT_CARD_SETTINGS_ADD_PAYMENT_METHOD_TITLE);

  // Adds 'Cancel' and 'Add' buttons to Navigation bar.
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_CANCEL_BUTTON)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(handleCancelButton:)];
  self.navigationItem.leftBarButtonItem.accessibilityIdentifier =
      kSettingsAddCreditCardCancelButtonID;

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_ADD_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(didTapAddButton:)];
  self.navigationItem.rightBarButtonItem.enabled = NO;
  self.navigationItem.rightBarButtonItem.accessibilityIdentifier =
      kSettingsAddCreditCardButtonID;

  [self loadModel];
}

- (BOOL)tableViewHasUserInput {
  [self updateCreditCardData];

  BOOL hasUserInput = _cardHolderName.length || _cardNumber.length ||
                      _cardCvc.length || _expirationMonth.length ||
                      _expirationYear.length || _cardNickname.length;

  return hasUserInput;
}

- (BOOL)canBecomeFirstResponder {
  return YES;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  AutofillCreditCardEditItem* cardHolderNameItem = [self cardHolderNameItem];
  AutofillCreditCardEditItem* cardNumberItem = [self cardNumberItem];
  AutofillCreditCardEditItem* expirationMonthItem = [self expirationMonthItem];
  AutofillCreditCardEditItem* expirationYearItem = [self expirationYearItem];
  AutofillCreditCardEditItem* cardCvcItem = [self cardCvcItem];

  [model addSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  [model addItem:cardNumberItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  [model addItem:expirationMonthItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  [model addItem:expirationYearItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  [model addItem:cardHolderNameItem
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  [model addItem:[self cardNicknameItem]
      toSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCvcStorageAndFilling)) {
    [model addItem:cardCvcItem
        toSectionWithIdentifier:SectionIdentifierCreditCardDetails];
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillCreditCardScannerIos)) {
    TableViewTextItem* cameraButtonItem =
        [[TableViewTextItem alloc] initWithType:ItemTypeUseCameraButton];
    cameraButtonItem.textColor = [UIColor colorNamed:kBlueColor];
    cameraButtonItem.text = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_ADD_CREDIT_CARD_OPEN_CAMERA_BUTTON_LABEL);
    cameraButtonItem.accessibilityTraits |= UIAccessibilityTraitButton;

    [model addSectionWithIdentifier:SectionIdentifierCameraButton];
    [model addItem:cameraButtonItem
        toSectionWithIdentifier:SectionIdentifierCameraButton];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
          ItemTypeUseCameraButton &&
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillCreditCardScannerIos)) {
    [self.presentationDelegate
        addCreditCardViewControllerRequestedCameraScan:self];
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypeUseCameraButton) {
    return indexPath;
  }
  return [super tableView:tableView willSelectRowAtIndexPath:indexPath];
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  // Sets a field to have valid text when a user begins editing so that the
  // error icon is not visible while a user edits a field.
  tableViewTextEditItem.hasValidText = YES;
  [self reconfigureCellsForItems:@[ tableViewTextEditItem ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewTextEditItem {
  // Checks the validity of the form and enables/disables the add button when
  // the user types a character that makes the form valid/invalid.
  [self updateCreditCardData];

  self.navigationItem.rightBarButtonItem.enabled =
      [_delegate addCreditCardViewController:self
                     isValidCreditCardNumber:_cardNumber
                             expirationMonth:_expirationMonth
                              expirationYear:_expirationYear
                                cardNickname:_cardNickname
                                     cardCvc:_cardCvc];

  [self reconfigureCellsForItems:@[ tableViewTextEditItem ]];
}

- (void)tableViewItemDidEndEditing:
    (TableViewTextEditItem*)tableViewTextEditItem {
  // Checks the validity of the field when a user ends editing and updates the
  // cells to display the error icon if the text is invalid.
  [self updateCreditCardData];

  // Considers a textfield to be valid if it has no data.
  if (tableViewTextEditItem.textFieldValue.length == 0) {
    tableViewTextEditItem.hasValidText = YES;
    [self reconfigureCellsForItems:@[ tableViewTextEditItem ]];
    return;
  }

  CHECK_NE(tableViewTextEditItem.type, ItemTypeUseCameraButton);
  switch (tableViewTextEditItem.type) {
    case ItemTypeCardNumber:
      tableViewTextEditItem.hasValidText =
          [_delegate addCreditCardViewController:self
                         isValidCreditCardNumber:_cardNumber];
      break;
    case ItemTypeExpirationMonth:
      tableViewTextEditItem.hasValidText =
          [_delegate addCreditCardViewController:self
                isValidCreditCardExpirationMonth:_expirationMonth];
      break;
    case ItemTypeExpirationYear:
      tableViewTextEditItem.hasValidText =
          [_delegate addCreditCardViewController:self
                 isValidCreditCardExpirationYear:_expirationYear];
      break;
    case ItemTypeCardNickname:
      tableViewTextEditItem.hasValidText =
          [_delegate addCreditCardViewController:self
                             isValidCardNickname:_cardNickname];
      break;
    case ItemTypeCardCvc:
      tableViewTextEditItem.hasValidText =
          [_delegate addCreditCardViewController:self isValidCardCvc:_cardCvc];
      break;
    default:
      // For the 'Name on card' and 'Security code' textfield.
      tableViewTextEditItem.hasValidText = YES;
  }
  [self reconfigureCellsForItems:@[ tableViewTextEditItem ]];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  // Use `ObjCCast` because `cell` might not be `TableViewTextEditCell`.
  // Set the delegate and style for only `TableViewTextEditCell` type of cell
  // not other types.
  TableViewTextEditCell* editCell =
      base::apple::ObjCCast<TableViewTextEditCell>(cell);
  editCell.textField.delegate = self;
  editCell.selectionStyle = UITableViewCellSelectionStyleNone;

  return cell;
}

#pragma mark - CreditCardScannerConsumer

- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear {
  if (cardNumber) {
    [self updateCellForItemType:ItemTypeCardNumber
            inSectionIdentifier:SectionIdentifierCreditCardDetails
                       withText:cardNumber];
  }

  if (expirationMonth) {
    [self updateCellForItemType:ItemTypeExpirationMonth
            inSectionIdentifier:SectionIdentifierCreditCardDetails
                       withText:expirationMonth];
  }

  if (expirationYear) {
    [self updateCellForItemType:ItemTypeExpirationYear
            inSectionIdentifier:SectionIdentifierCreditCardDetails
                       withText:expirationYear];
  }
}

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (itemType) {
    case ItemTypeName:
    case ItemTypeCardNumber:
    case ItemTypeExpirationMonth:
    case ItemTypeExpirationYear:
    case ItemTypeCardNickname:
    case ItemTypeCardCvc:
      return YES;
    case ItemTypeUseCameraButton:
      return NO;
  }
  NOTREACHED();
}

#pragma mark - Private

// Handles Add button to add a new credit card.
- (void)didTapAddButton:(id)sender {
  [self updateCreditCardData];
  [_delegate addCreditCardViewController:self
             addCreditCardWithHolderName:_cardHolderName
                              cardNumber:_cardNumber
                         expirationMonth:_expirationMonth
                          expirationYear:_expirationYear
                            cardNickname:_cardNickname
                                 cardCvc:_cardCvc];
}

// Updates credit card data properties with the text in TableView cells.
- (void)updateCreditCardData {
  _cardHolderName =
      [self readTextFromItemtype:ItemTypeName
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  _cardNumber = [self readTextFromItemtype:ItemTypeCardNumber
                         sectionIdentifier:SectionIdentifierCreditCardDetails];

  _expirationMonth =
      [self readTextFromItemtype:ItemTypeExpirationMonth
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  _expirationYear =
      [self readTextFromItemtype:ItemTypeExpirationYear
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  _cardNickname =
      [self readTextFromItemtype:ItemTypeCardNickname
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCvcStorageAndFilling)) {
    _cardCvc = [self readTextFromItemtype:ItemTypeCardCvc
                        sectionIdentifier:SectionIdentifierCreditCardDetails];
  }
}

// Reads and returns the data from the item with passed `itemType` and
// `sectionIdentifier`.
- (NSString*)readTextFromItemtype:(NSInteger)itemType
                sectionIdentifier:(NSInteger)sectionIdentifier {
  NSIndexPath* path =
      [self.tableViewModel indexPathForItemType:itemType
                              sectionIdentifier:sectionIdentifier];
  AutofillCreditCardEditItem* item =
      base::apple::ObjCCastStrict<AutofillCreditCardEditItem>(
          [self.tableViewModel itemAtIndexPath:path]);
  NSString* text = item.textFieldValue;
  return text;
}

// Updates TableView cell of `itemType` in `sectionIdentifier` textfieldValue
// with `text`.
- (void)updateCellForItemType:(NSInteger)itemType
          inSectionIdentifier:(NSInteger)sectionIdentifier
                     withText:(NSString*)text {
  NSIndexPath* path =
      [self.tableViewModel indexPathForItemType:itemType
                              sectionIdentifier:sectionIdentifier];
  AutofillCreditCardEditItem* item =
      base::apple::ObjCCastStrict<AutofillCreditCardEditItem>(
          [self.tableViewModel itemAtIndexPath:path]);
  [item updateTextFieldValue:text];
}

// Dimisses this view controller when Cancel button is tapped.
- (void)handleCancelButton:(id)sender {
  [_delegate addCreditCardViewControllerDidCancel:self];
}

// Returns initialized tableViewItem with passed arguments.
- (AutofillCreditCardEditItem*)
    createTableViewItemWithType:(NSInteger)itemType
             fieldNameLabelText:(NSString*)fieldNameLabelText
                 textFieldValue:(NSString*)textFieldValue
           textFieldPlaceholder:(NSString*)textFieldPlaceholder
                   keyboardType:(UIKeyboardType)keyboardType
       autofillCreditCardUIType:
           (AutofillCreditCardUIType)autofillCreditCardUIType {
  AutofillCreditCardEditItem* item =
      [[AutofillCreditCardEditItem alloc] initWithType:itemType];
  item.delegate = self;
  item.fieldNameLabelText = fieldNameLabelText;
  item.textFieldValue = textFieldValue;
  item.textFieldPlaceholder = textFieldPlaceholder;
  item.keyboardType = keyboardType;
  item.hideIcon = NO;
  item.textFieldEnabled = YES;
  item.autofillCreditCardUIType = autofillCreditCardUIType;
  return item;
}

- (AutofillCreditCardEditItem*)expirationYearItem {
  AutofillCreditCardEditItem* expirationYearItem =
      [self createTableViewItemWithType:ItemTypeExpirationYear
                     fieldNameLabelText:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_EXP_YEAR)
                         textFieldValue:_expirationYear
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRATION_YEAR)
                           keyboardType:UIKeyboardTypeNumberPad
               autofillCreditCardUIType:AutofillCreditCardUIType::kExpYear];
  return expirationYearItem;
}

- (AutofillCreditCardEditItem*)expirationMonthItem {
  AutofillCreditCardEditItem* expirationMonthItem =
      [self createTableViewItemWithType:ItemTypeExpirationMonth
                     fieldNameLabelText:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_EXP_MONTH)
                         textFieldValue:_expirationMonth
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH)
                           keyboardType:UIKeyboardTypeNumberPad
               autofillCreditCardUIType:AutofillCreditCardUIType::kExpMonth];
  return expirationMonthItem;
}

- (AutofillCreditCardEditItem*)cardNumberItem {
  AutofillCreditCardEditItem* cardNumberItem =
      [self createTableViewItemWithType:ItemTypeCardNumber
                     fieldNameLabelText:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_CARD_NUMBER)
                         textFieldValue:_cardNumber
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_NUMBER)
                           keyboardType:UIKeyboardTypeNumberPad
               autofillCreditCardUIType:AutofillCreditCardUIType::kNumber];
  return cardNumberItem;
}

- (AutofillCreditCardEditItem*)cardHolderNameItem {
  AutofillCreditCardEditItem* cardHolderNameItem =
      [self createTableViewItemWithType:ItemTypeName
                     fieldNameLabelText:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_CARDHOLDER)
                         textFieldValue:_cardHolderName
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_HOLDER_NAME)
                           keyboardType:UIKeyboardTypeDefault
               autofillCreditCardUIType:AutofillCreditCardUIType::kFullName];
  return cardHolderNameItem;
}

- (AutofillCreditCardEditItem*)cardNicknameItem {
  AutofillCreditCardEditItem* cardNicknameItem =
      [self createTableViewItemWithType:ItemTypeCardNickname
                     fieldNameLabelText:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_NICKNAME)
                         textFieldValue:_cardNickname
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_NICKNAME)
                           keyboardType:UIKeyboardTypeDefault
               autofillCreditCardUIType:AutofillCreditCardUIType::kUnknown];
  return cardNicknameItem;
}

- (AutofillCreditCardEditItem*)cardCvcItem {
  AutofillCreditCardEditItem* cardCvcItem =
      [self createTableViewItemWithType:ItemTypeCardCvc
                     fieldNameLabelText:l10n_util::GetNSString(
                                            IDS_IOS_AUTOFILL_SECURITY_CODE)
                         textFieldValue:_cardCvc
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CVC_OPTIONAL)
                           keyboardType:UIKeyboardTypeNumberPad
               autofillCreditCardUIType:AutofillCreditCardUIType::kUnknown];
  return cardCvcItem;
}

@end

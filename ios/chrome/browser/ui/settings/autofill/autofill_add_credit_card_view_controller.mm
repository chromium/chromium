// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/autofill_credit_card_edit_item.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller_delegate.h"
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
};

}  // namespace

@interface AutofillAddCreditCardViewController () <
    TableViewTextEditItemDelegate>

// The AddCreditCardViewControllerDelegate for this ViewController.
@property(nonatomic, weak) id<AddCreditCardViewControllerDelegate> delegate;

// The card holder name updated with the text in tableview cell.
@property(nonatomic, strong) NSString* cardHolderName;

// The card number in the UI.
@property(nonatomic, strong) NSString* cardNumber;

// The expiration month in the UI.
@property(nonatomic, strong) NSString* expirationMonth;

// The expiration year in the UI.
@property(nonatomic, strong) NSString* expirationYear;

// The user provided nickname for the credit card.
@property(nonatomic, strong) NSString* cardNickname;

@end

@implementation AutofillAddCreditCardViewController

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

  BOOL hasUserInput = self.cardHolderName.length || self.cardNumber.length ||
                      self.expirationMonth.length ||
                      self.expirationYear.length || self.cardNickname.length;

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
      [self.delegate addCreditCardViewController:self
                         isValidCreditCardNumber:self.cardNumber
                                 expirationMonth:self.expirationMonth
                                  expirationYear:self.expirationYear
                                    cardNickname:self.cardNickname];

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

  switch (tableViewTextEditItem.type) {
    case ItemTypeCardNumber:
      tableViewTextEditItem.hasValidText =
          [self.delegate addCreditCardViewController:self
                             isValidCreditCardNumber:self.cardNumber];
      break;
    case ItemTypeExpirationMonth:
      tableViewTextEditItem.hasValidText =
          [self.delegate addCreditCardViewController:self
                    isValidCreditCardExpirationMonth:self.expirationMonth];
      break;
    case ItemTypeExpirationYear:
      tableViewTextEditItem.hasValidText =
          [self.delegate addCreditCardViewController:self
                     isValidCreditCardExpirationYear:self.expirationYear];
      break;
    case ItemTypeCardNickname:
      tableViewTextEditItem.hasValidText =
          [self.delegate addCreditCardViewController:self
                                 isValidCardNickname:self.cardNickname];
      break;
    default:
      // For the 'Name on card' textfield.
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

#pragma mark - AutofillEditTableViewController

- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:cellPath];
  switch (itemType) {
    case ItemTypeName:
    case ItemTypeCardNumber:
    case ItemTypeExpirationMonth:
    case ItemTypeExpirationYear:
    case ItemTypeCardNickname:
      return YES;
  }
  NOTREACHED_IN_MIGRATION();
  return NO;
}

#pragma mark - Private

// Handles Add button to add a new credit card.
- (void)didTapAddButton:(id)sender {
  [self updateCreditCardData];
  [self.delegate addCreditCardViewController:self
                 addCreditCardWithHolderName:self.cardHolderName
                                  cardNumber:self.cardNumber
                             expirationMonth:self.expirationMonth
                              expirationYear:self.expirationYear
                                cardNickname:self.cardNickname];
}

// Updates credit card data properties with the text in TableView cells.
- (void)updateCreditCardData {
  self.cardHolderName =
      [self readTextFromItemtype:ItemTypeName
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  self.cardNumber =
      [self readTextFromItemtype:ItemTypeCardNumber
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  self.expirationMonth =
      [self readTextFromItemtype:ItemTypeExpirationMonth
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  self.expirationYear =
      [self readTextFromItemtype:ItemTypeExpirationYear
               sectionIdentifier:SectionIdentifierCreditCardDetails];

  self.cardNickname =
      [self readTextFromItemtype:ItemTypeCardNickname
               sectionIdentifier:SectionIdentifierCreditCardDetails];
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
  item.textFieldValue = text;
  [self reconfigureCellsForItems:@[ item ]];
}

// Dimisses this view controller when Cancel button is tapped.
- (void)handleCancelButton:(id)sender {
  [self.delegate addCreditCardViewControllerDidCancel:self];
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
                         textFieldValue:self.expirationYear
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
                         textFieldValue:self.expirationMonth
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
                         textFieldValue:self.cardNumber
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
                         textFieldValue:self.cardHolderName
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
                         textFieldValue:self.cardNickname
                   textFieldPlaceholder:
                       l10n_util::GetNSString(
                           IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_NICKNAME)
                           keyboardType:UIKeyboardTypeDefault
               autofillCreditCardUIType:AutofillCreditCardUIType::kUnknown];
  return cardNicknameItem;
}

@end

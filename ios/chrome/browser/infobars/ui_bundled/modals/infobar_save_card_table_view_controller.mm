// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/save_card_infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_modal_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
// Number of Months in a year.
const int kNumberOfMonthsInYear = 12;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Height of the Google pay logo.
CGFloat const kGooglePayLogoHeight = 26;
#endif

}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardLastDigits = kItemTypeEnumZero,
  ItemTypeCardHolderName,
  ItemTypeCardExpireMonth,
  ItemTypeCardExpireYear,
  ItemTypeCardCvc,
  ItemTypeCardLegalMessage,
};

@interface InfobarSaveCardTableViewController () <TableViewTextEditItemDelegate,
                                                  TableViewTextLinkCellDelegate,
                                                  UITextFieldDelegate>

// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Prefs updated by InfobarSaveCardModalConsumer.
// Cardholder name to be displayed.
@property(nonatomic, copy) NSString* cardholderName;
// Card Issuer icon image to be displayed.
@property(nonatomic, strong) UIImage* cardIssuerIcon;
// Card Network for accessibility label.
@property(nonatomic, copy) NSString* cardNetwork;
// Card Number to be displayed.
@property(nonatomic, copy) NSString* cardNumber;
// Card Expiration Month to be displayed
@property(nonatomic, copy) NSString* expirationMonth;
// Card Expiration Year to be displayed.
@property(nonatomic, copy) NSString* expirationYear;
// Card Security code to be displayed.
@property(nonatomic, strong) NSString* cardCvc;
// Card related Legal Messages to be displayed.
@property(nonatomic, copy)
    NSMutableArray<SaveCardMessageWithLinks*>* legalMessages;
// YES if the Card being displayed has been accepted to be saved.
@property(nonatomic, assign) BOOL currentCardSaveAccepted;
// Set to YES if the Modal should support editing.
@property(nonatomic, assign) BOOL supportsEditing;
// The email to identify the account where the card will be saved. Empty if none
// should be shown, e.g. if the card won't be saved to any account.
@property(nonatomic, copy) NSString* displayedTargetAccountEmail;
// Logo icon image to be displayed below the legal message.
@property(nonatomic, strong) UIImage* logoIcon;
// Accessibility description of the `logoIcon`
@property(nonatomic, copy) NSString* logoIconDescription;

// Item for displaying the last digits of the card to be saved.
@property(nonatomic, strong) TableViewTextEditItem* cardLastDigitsItem;
// Item for displaying and editing the cardholder name.
@property(nonatomic, strong) TableViewTextEditItem* cardholderNameItem;
// Item for displaying and editing the expiration month.
@property(nonatomic, strong) TableViewTextEditItem* expirationMonthItem;
// Item for displaying and editing the expiration year.
@property(nonatomic, strong) TableViewTextEditItem* expirationYearItem;
// Item for displaying and editing the security code.
@property(nonatomic, strong) TableViewTextEditItem* cardCvcItem;

@end

@implementation InfobarSaveCardTableViewController {
  NSLayoutConstraint* _tableViewHeightConstraint;
}

- (instancetype)initWithStyle:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveCard];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  // The table view height will be set to its content view in
  // viewDidLayoutSubviews. Provide a default value here.
  _tableViewHeightConstraint = [self.tableView.heightAnchor
      constraintEqualToConstant:self.view.bounds.size.height];
  _tableViewHeightConstraint.active = YES;
  self.tableView.scrollEnabled = NO;
  self.tableView.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  [self loadModel];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _tableViewHeightConstraint.constant =
      self.tableView.contentSize.height +
      self.tableView.adjustedContentInset.top +
      self.tableView.adjustedContentInset.bottom;
}

#pragma mark - Properties

- (NSString*)currentCardholderName {
  return self.cardholderNameItem.textFieldValue;
}

- (NSString*)currentExpirationMonth {
  return self.expirationMonthItem.textFieldValue;
}

- (NSString*)currentExpirationYear {
  return self.expirationYearItem.textFieldValue;
}

- (NSString*)currentCardCVC {
  return self.cardCvcItem.textFieldValue;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  self.cardLastDigitsItem = [self
      textEditItemWithType:ItemTypeCardLastDigits
        fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER)
            textFieldValue:self.cardNumber
          textFieldEnabled:NO];
  self.cardLastDigitsItem.identifyingIcon = self.cardIssuerIcon;
  self.cardLastDigitsItem.cellAccessibilityLabel =
      [NSString stringWithFormat:@"%@, %@, %@",
                                 self.cardLastDigitsItem.fieldNameLabelText,
                                 self.cardLastDigitsItem.textFieldValue,
                                 self.cardNetwork];
  [model addItem:self.cardLastDigitsItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.cardholderNameItem =
      [self textEditItemWithType:ItemTypeCardHolderName
              fieldNameLabelText:l10n_util::GetNSString(
                                     IDS_IOS_AUTOFILL_CARDHOLDER_NAME)
                  textFieldValue:self.cardholderName
                textFieldEnabled:self.supportsEditing];
  [model addItem:self.cardholderNameItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.expirationMonthItem = [self
      textEditItemWithType:ItemTypeCardExpireMonth
        fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_MONTH)
            textFieldValue:self.expirationMonth
          textFieldEnabled:self.supportsEditing];
  [model addItem:self.expirationMonthItem
      toSectionWithIdentifier:SectionIdentifierContent];

  self.expirationYearItem = [self
      textEditItemWithType:ItemTypeCardExpireYear
        fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_YEAR)
            textFieldValue:self.expirationYear
          textFieldEnabled:self.supportsEditing];
  [model addItem:self.expirationYearItem
      toSectionWithIdentifier:SectionIdentifierContent];

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCvcStorageAndFilling)) {
    self.cardCvcItem =
        [self textEditItemWithType:ItemTypeCardCvc
                fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_CVC)
                    textFieldValue:self.cardCvc
                  textFieldEnabled:self.supportsEditing];
    self.cardCvcItem.keyboardType = UIKeyboardTypeNumberPad;
    self.cardCvcItem.customTextfieldAccessibilityIdentifier =
        kSaveCardModalCVCTextFieldIdentifier;
    [model addItem:self.cardCvcItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  // Add a `TableViewTextLinkItem` for each legal message and add logo to the
  // last item.
  for (size_t index = 0; index < self.legalMessages.count; index++) {
    SaveCardMessageWithLinks* message = self.legalMessages[index];
    TableViewTextLinkItem* legalMessageItem =
        [[TableViewTextLinkItem alloc] initWithType:ItemTypeCardLegalMessage];
    // Logo needs to be added once, at the end of all legal messages, within the
    // last `legalMessageItem`.
    if (index == (self.legalMessages.count - 1)) {
      legalMessageItem.logoImage = [self logoIconImage];
      legalMessageItem.logoImageDescription = self.logoIconDescription;
    }
    legalMessageItem.text = message.messageText;
    legalMessageItem.linkURLs = message.linkURLs;
    legalMessageItem.linkRanges = message.linkRanges;
    [model addItem:legalMessageItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  if (self.supportsEditing) {
    [self.cardholderNameItem
        setHasValidText:[self isCardholderNameValid:self.cardholderName]];
    [self.expirationMonthItem
        setHasValidText:[self isExpirationMonthValid:self.expirationMonth
                                             forYear:self.expirationYear]];
    [self.expirationYearItem
        setHasValidText:[self isExpirationYearValid:self.expirationYear]];
    [self updateSaveCardButtonState];
  }
}

#pragma mark - InfobarSaveCardModalConsumer

- (void)setupModalViewControllerWithPrefs:(NSDictionary*)prefs {
  self.cardholderName = prefs[kCardholderNamePrefKey];
  self.cardIssuerIcon = prefs[kCardIssuerIconNamePrefKey];
  self.cardNetwork = prefs[kCardNetworkPrefKey];
  self.cardNumber = prefs[kCardNumberPrefKey];
  self.expirationMonth = prefs[kExpirationMonthPrefKey];
  self.expirationYear = prefs[kExpirationYearPrefKey];
  self.cardCvc = prefs[kCardCvcPrefKey];
  self.legalMessages = prefs[kLegalMessagesPrefKey];
  self.currentCardSaveAccepted =
      [prefs[kCurrentCardSaveAcceptedPrefKey] boolValue];
  self.supportsEditing = [prefs[kSupportsEditingPrefKey] boolValue];
  self.displayedTargetAccountEmail = prefs[kDisplayedTargetAccountEmailPrefKey];
  self.logoIcon = prefs[kLogoIconPrefKey];
  [self.tableView reloadData];

  [self updateSaveCardButtonState];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeCardLastDigits: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardHolderName: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardExpireMonth: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardExpireYear: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardCvc: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardLegalMessage: {
      TableViewTextLinkCell* linkCell =
          base::apple::ObjCCast<TableViewTextLinkCell>(cell);
      linkCell.delegate = self;
      break;
    }
  }

  return cell;
}

// `uploadCompleted == NO` indicates loading state and `uploadCompleted == YES`
// indicates confirmation state. For `uploadCompleted == NO`, sets an activity
// indicator on the button to show card is being uploaded. For `uploadCompleted
// == YES`, sets a checkmark on the button to show card upload has completed.
// Also disables the button and hides its text.
- (void)showProgressWithUploadCompleted:(BOOL)uploadCompleted {
  [self.containerDelegate showProgressWithUploadCompleted:uploadCompleted];

  [self updateItemsInProgressState];

  NSMutableArray* items = [NSMutableArray arrayWithArray:@[
    self.cardLastDigitsItem, self.cardholderNameItem, self.expirationMonthItem,
    self.expirationYearItem
  ]];
  if (self.cardCvcItem) {
    [items addObject:self.cardCvcItem];
  }
  [self reconfigureCellsForItems:items];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem {
  ItemType itemType = static_cast<ItemType>(tableViewItem.type);
  switch (itemType) {
    case ItemTypeCardLastDigits:
      // Not editable.
      break;
    case ItemTypeCardHolderName:
      [self nameEditDidBegin];
      break;
    case ItemTypeCardExpireMonth:
      [self monthEditDidBegin];
      break;
    case ItemTypeCardExpireYear:
      [self yearEditDidBegin];
      break;
    case ItemTypeCardCvc:
      // No metrics recorded.
      break;
    case ItemTypeCardLegalMessage:
      NOTREACHED();
  }
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem {
  ItemType itemType = static_cast<ItemType>(tableViewItem.type);
  switch (itemType) {
    case ItemTypeCardLastDigits:
      // Not editable.
      break;
    case ItemTypeCardHolderName:
      [self nameDidChange];
      break;
    case ItemTypeCardExpireMonth:
      [self expireMonthDidChange];
      break;
    case ItemTypeCardExpireYear:
      [self expireYearDidChange];
      break;
    case ItemTypeCardCvc:
      [self cvcDidChange];
      break;
    case ItemTypeCardLegalMessage:
      NOTREACHED();
  }
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem {
  // No op.
}

#pragma mark - TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(CrURL*)URL {
  [self.containerDelegate dismissModalAndOpenURL:URL.gurl];
}

#pragma mark - Private Methods

// Updates `self.saveCardButtonItem` enabled state taking into account the
// current editable items.
- (void)updateSaveCardButtonState {
  BOOL newButtonState = [self isCurrentInputValid];
  // If card is already saved, disable the button.
  if (self.currentCardSaveAccepted) {
    newButtonState = NO;
  }
  [self.containerDelegate updateSaveButtonEnabled:newButtonState];
}

- (void)nameEditDidBegin {
  [SaveCardInfobarMetricsRecorder
      recordModalEvent:MobileMessagesSaveCardModalEvent::EditedCardHolderName];
}

- (void)monthEditDidBegin {
  [SaveCardInfobarMetricsRecorder
      recordModalEvent:MobileMessagesSaveCardModalEvent::EditedExpirationMonth];
}

- (void)yearEditDidBegin {
  [SaveCardInfobarMetricsRecorder
      recordModalEvent:MobileMessagesSaveCardModalEvent::EditedExpirationYear];
}

- (void)cvcEditDidBegin {
  [SaveCardInfobarMetricsRecorder
      recordModalEvent:MobileMessagesSaveCardModalEvent::EditedCvc];
}

- (void)nameDidChange {
  BOOL isNameValid =
      [self isCardholderNameValid:self.cardholderNameItem.textFieldValue];

  [self
      updateErrorStateForItem:self.cardholderNameItem
                      isValid:isNameValid
                 errorMessage:
                     IDS_IOS_AUTOFILL_INVALID_CARDHOLDER_NAME_ACCESSIBILITY_ANNOUNCEMENT];

  [self.cardholderNameItem setHasValidText:isNameValid];
  [self reconfigureCellsForItems:@[ self.cardholderNameItem ]];

  [self updateSaveCardButtonState];
}

- (void)expireMonthDidChange {
  BOOL isMonthValid =
      [self isExpirationMonthValid:self.expirationMonthItem.textFieldValue
                           forYear:self.expirationYearItem.textFieldValue];

  [self
      updateErrorStateForItem:self.expirationMonthItem
                      isValid:isMonthValid
                 errorMessage:
                     IDS_IOS_AUTOFILL_INVALID_EXPIRATION_DATE_ACCESSIBILITY_ANNOUNCEMENT];

  [self.expirationMonthItem setHasValidText:isMonthValid];
  [self reconfigureCellsForItems:@[ self.expirationMonthItem ]];

  [self updateSaveCardButtonState];
}

- (void)expireYearDidChange {
  BOOL isYearValid =
      [self isExpirationYearValid:self.expirationYearItem.textFieldValue];
  // Check if the card month is valid for the newly entered year.
  BOOL isMonthValid =
      [self isExpirationMonthValid:self.expirationMonthItem.textFieldValue
                           forYear:self.expirationYearItem.textFieldValue];

  // Update Year
  [self
      updateErrorStateForItem:self.expirationYearItem
                      isValid:isYearValid
                 errorMessage:
                     IDS_IOS_AUTOFILL_INVALID_EXPIRATION_DATE_ACCESSIBILITY_ANNOUNCEMENT];

  // Update Month (re-validate as it depends on year)
  [self
      updateErrorStateForItem:self.expirationMonthItem
                      isValid:isMonthValid
                 errorMessage:
                     IDS_IOS_AUTOFILL_INVALID_EXPIRATION_DATE_ACCESSIBILITY_ANNOUNCEMENT];

  [self.expirationYearItem setHasValidText:isYearValid];
  [self.expirationMonthItem setHasValidText:isMonthValid];
  [self reconfigureCellsForItems:@[
    self.expirationYearItem, self.expirationMonthItem
  ]];

  [self updateSaveCardButtonState];
}

- (void)cvcDidChange {
  BOOL isCvcValid = [self isCVCValid:self.cardCvcItem.textFieldValue];

  [self updateErrorStateForItem:self.cardCvcItem
                        isValid:isCvcValid
                   errorMessage:
                       IDS_IOS_AUTOFILL_INVALID_CVC_ACCESSIBILITY_ANNOUNCEMENT];
  [self.cardCvcItem
      setHasValidText:[self isCVCValid:self.cardCvcItem.textFieldValue]];
  [self reconfigureCellsForItems:@[ self.cardCvcItem ]];
  [self updateSaveCardButtonState];
}

// In progress state, disables the text and icon for the items of
// the type `TableViewTextEditItem`, since those fields are not editable while
// showing loading or confirmation.
- (void)updateItemsInProgressState {
  self.cardLastDigitsItem.identifyingIconEnabled = NO;
  self.cardLastDigitsItem.textFieldEnabled = NO;

  self.cardholderNameItem.identifyingIconEnabled = NO;
  self.cardholderNameItem.textFieldEnabled = NO;

  self.expirationMonthItem.identifyingIconEnabled = NO;
  self.expirationMonthItem.textFieldEnabled = NO;

  self.expirationYearItem.identifyingIconEnabled = NO;
  self.expirationYearItem.textFieldEnabled = NO;

  self.cardCvcItem.identifyingIconEnabled = NO;
  self.cardCvcItem.textFieldEnabled = NO;
}

#pragma mark - Helpers

- (UIImage*)logoIconImage {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGooglePaySymbol, kGooglePayLogoHeight));
#else
  return self.logoIcon;
#endif
}

- (TableViewTextEditItem*)textEditItemWithType:(ItemType)type
                            fieldNameLabelText:(NSString*)name
                                textFieldValue:(NSString*)value
                              textFieldEnabled:(BOOL)enabled {
  TableViewTextEditItem* textEditItem =
      [[TableViewTextEditItem alloc] initWithType:type];
  textEditItem.fieldNameLabelText = name;
  textEditItem.textFieldValue = value;
  textEditItem.textFieldEnabled = enabled;
  textEditItem.hideIcon = !enabled;
  textEditItem.returnKeyType = UIReturnKeyDone;
  textEditItem.delegate = self;
  textEditItem.textFieldDelegate = self;

  return textEditItem;
}

// YES if the current values of the Card are valid.
// TODO(crbug.com/40109422):Ideally the InfobarDelegate should validate
// the correctness of the input.
- (BOOL)isCurrentInputValid {
  if (![self isCardholderNameValid:self.cardholderNameItem.textFieldValue]) {
    return NO;
  }

  if (![self isExpirationMonthValid:self.expirationMonthItem.textFieldValue
                            forYear:self.expirationYearItem.textFieldValue]) {
    return NO;
  }

  if (![self isExpirationYearValid:self.expirationYearItem.textFieldValue]) {
    return NO;
  }

  if (self.cardCvcItem && ![self isCVCValid:self.cardCvcItem.textFieldValue]) {
    return NO;
  }

  return YES;
}

// YES if `cardholderName` is valid.
- (BOOL)isCardholderNameValid:(NSString*)cardholderName {
  // Check that the name is not empty or only whitespace.
  NSCharacterSet* set = [NSCharacterSet whitespaceCharacterSet];
  if (![[cardholderName stringByTrimmingCharactersInSet:set] length]) {
    return NO;
  }

  return YES;
}

// YES if `expirationMonth` is valid for `expirationYear`.
- (BOOL)isExpirationMonthValid:(NSString*)expirationMonth
                       forYear:(NSString*)expirationYear {
  NSNumber* expirationMonthNumber = [self numberFromString:expirationMonth];
  if (!expirationMonthNumber) {
    return NO;
  }

  int expirationMonthInteger = [expirationMonthNumber intValue];
  if (expirationMonthInteger <= 0 ||
      expirationMonthInteger > kNumberOfMonthsInYear) {
    return NO;
  }

  if ([self currentYearIntValue] ==
      [[self numberFromString:expirationYear] intValue]) {
    return expirationMonthInteger >= [self currentMonthIntValue];
  }

  return YES;
}

// YES if `expirationYear` is valid for the current date.
- (BOOL)isExpirationYearValid:(NSString*)expirationYear {
  NSNumber* expirationYearNumber = [self numberFromString:expirationYear];
  if (!expirationYearNumber) {
    return NO;
  }

  return [self currentYearIntValue] <= [expirationYearNumber intValue];
}

// YES if `cvc` is valid.
- (BOOL)isCVCValid:(NSString*)cvc {
  if (!cvc || cvc.length == 0) {
    return YES;
  }
  // Check that the CVC is 3 or 4 digits.
  if (cvc.length < 3 || cvc.length > 4) {
    return NO;
  }
  // Check that the CVC contains only digits.
  NSCharacterSet* nonDigitCharacterSet =
      [[NSCharacterSet decimalDigitCharacterSet] invertedSet];
  return
      [cvc rangeOfCharacterFromSet:nonDigitCharacterSet].location == NSNotFound;
}

// The current month int value.
- (int)currentMonthIntValue {
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  [dateFormatter setDateFormat:@"MM"];
  NSString* monthString = [dateFormatter stringFromDate:[NSDate date]];
  return [[self numberFromString:monthString] intValue];
}

// The current year int value.
- (int)currentYearIntValue {
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  [dateFormatter setDateFormat:@"yyyy"];
  NSString* yearString = [dateFormatter stringFromDate:[NSDate date]];
  return [[self numberFromString:yearString] intValue];
}

// Converts `string` into an NSNumber. returns nil if `string` is invalid.
- (NSNumber*)numberFromString:(NSString*)string {
  NSNumberFormatter* numberFormatter = [[NSNumberFormatter alloc] init];
  numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;
  return [numberFormatter numberFromString:string];
}

// Helper to update the item's error state and accessibility label.
- (void)updateErrorStateForItem:(TableViewTextEditItem*)item
                        isValid:(BOOL)isValid
                   errorMessage:(int)errorMessageID {
  [item setHasValidText:isValid];

  if (!isValid) {
    // Append the error message to the field name for the accessibility label.
    // e.g., "Name on Card, Invalid Name"
    NSString* errorMessage = l10n_util::GetNSString(errorMessageID);
    item.cellAccessibilityLabel = [NSString
        stringWithFormat:@"%@, %@", item.fieldNameLabelText, errorMessage];
  } else {
    // Reset to nil to use the default accessibility label (Field Name + Value).
    item.cellAccessibilityLabel = nil;
  }
}

@end

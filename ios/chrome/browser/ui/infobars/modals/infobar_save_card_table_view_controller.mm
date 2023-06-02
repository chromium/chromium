// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_table_view_controller.h"

#import "base/feature_list.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/autofill/message/save_card_message_with_links.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/cells/target_account_item.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Number of Months in a year.
const int kNumberOfMonthsInYear = 12;
}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardLastDigits = kItemTypeEnumZero,
  ItemTypeCardHolderName,
  ItemTypeCardExpireMonth,
  ItemTypeCardExpireYear,
  ItemTypeCardLegalMessage,
  ItemTypeCardSave,
  ItemTypeTargetAccount,
};

@interface InfobarSaveCardTableViewController () <TableViewTextLinkCellDelegate,
                                                  UITextFieldDelegate>

// InfobarSaveCardModalDelegate for this ViewController.
@property(nonatomic, strong) id<InfobarSaveCardModalDelegate>
    saveCardModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// Prefs updated by InfobarSaveCardModalConsumer.
// Cardholder name to be displayed.
@property(nonatomic, copy) NSString* cardholderName;
// Card Issuer icon image to be displayed.
@property(nonatomic, strong) UIImage* cardIssuerIcon;
// Card Number to be displayed.
@property(nonatomic, copy) NSString* cardNumber;
// Card Expiration Month to be displayed
@property(nonatomic, copy) NSString* expirationMonth;
// Card Expiration Year to be displayed.
@property(nonatomic, copy) NSString* expirationYear;
// Card related Legal Messages to be displayed.
@property(nonatomic, copy)
    NSMutableArray<SaveCardMessageWithLinks*>* legalMessages;
// YES if the Card being displayed has been saved.
@property(nonatomic, assign) BOOL currentCardSaved;
// Set to YES if the Modal should support editing.
@property(nonatomic, assign) BOOL supportsEditing;
// The email to identify the account where the card will be saved. Empty if none
// should be shown, e.g. if the card won't be saved to any account.
@property(nonatomic, copy) NSString* displayedTargetAccountEmail;
// The avatar to identify the account where the card will be saved. Null if none
// should be shown, e.g. if the card won't be saved to any account.
@property(nonatomic, strong) UIImage* displayedTargetAccountAvatar;

// Item for displaying and editing the cardholder name.
@property(nonatomic, strong) TableViewTextEditItem* cardholderNameItem;
// Item for displaying and editing the expiration month.
@property(nonatomic, strong) TableViewTextEditItem* expirationMonthItem;
// Item for displaying and editing the expiration year.
@property(nonatomic, strong) TableViewTextEditItem* expirationYearItem;
// Item for displaying the save card button .
@property(nonatomic, strong) TableViewTextButtonItem* saveCardButtonItem;

@end

@implementation InfobarSaveCardTableViewController

- (instancetype)initWithModalDelegate:
    (id<InfobarSaveCardModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _saveCardModalDelegate = modalDelegate;
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypeSaveCard];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismissInfobarModal)];
  cancelButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.saveCardModalDelegate modalInfobarWasDismissed:self];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.tableView.scrollEnabled =
      self.tableView.contentSize.height > self.view.frame.size.height;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierContent];

  TableViewTextEditItem* cardLastDigitsItem = [self
      textEditItemWithType:ItemTypeCardLastDigits
        fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARD_NUMBER)
            textFieldValue:self.cardNumber
          textFieldEnabled:NO];
  cardLastDigitsItem.identifyingIcon = self.cardIssuerIcon;
  [model addItem:cardLastDigitsItem
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

  // The extra legal line and account info should only be shown together.
  bool shouldShowExtraLegalLineAndAccountInfo =
      [self.displayedTargetAccountEmail length] > 0 &&
      self.displayedTargetAccountAvatar != nil;

  // Concatenate legal lines and maybe add the extra one.
  for (SaveCardMessageWithLinks* message in self.legalMessages) {
    TableViewTextLinkItem* legalMessageItem =
        [[TableViewTextLinkItem alloc] initWithType:ItemTypeCardLegalMessage];
    legalMessageItem.text = message.messageText;
    legalMessageItem.linkURLs = message.linkURLs;
    legalMessageItem.linkRanges = message.linkRanges;
    [model addItem:legalMessageItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }
  if (shouldShowExtraLegalLineAndAccountInfo) {
    TableViewTextLinkItem* legalMessageItem =
        [[TableViewTextLinkItem alloc] initWithType:ItemTypeCardLegalMessage];
    legalMessageItem.text =
        l10n_util::GetNSString(IDS_IOS_CARD_WILL_BE_SAVED_TO_ACCOUNT);
    [model addItem:legalMessageItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  if (shouldShowExtraLegalLineAndAccountInfo) {
    TargetAccountItem* targetTargetAccountItem =
        [[TargetAccountItem alloc] initWithType:ItemTypeTargetAccount];
    targetTargetAccountItem.email = self.displayedTargetAccountEmail;
    targetTargetAccountItem.avatar = self.displayedTargetAccountAvatar;
    [model addItem:targetTargetAccountItem
        toSectionWithIdentifier:SectionIdentifierContent];
  }

  self.saveCardButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeCardSave];
  self.saveCardButtonItem.textAlignment = NSTextAlignmentNatural;
  self.saveCardButtonItem.buttonText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  self.saveCardButtonItem.enabled = !self.currentCardSaved;
  self.saveCardButtonItem.disableButtonIntrinsicWidth = YES;
  [model addItem:self.saveCardButtonItem
      toSectionWithIdentifier:SectionIdentifierContent];

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
  self.cardNumber = prefs[kCardNumberPrefKey];
  self.expirationMonth = prefs[kExpirationMonthPrefKey];
  self.expirationYear = prefs[kExpirationYearPrefKey];
  self.legalMessages = prefs[kLegalMessagesPrefKey];
  self.currentCardSaved = [prefs[kCurrentCardSavedPrefKey] boolValue];
  self.supportsEditing = [prefs[kSupportsEditingPrefKey] boolValue];
  self.displayedTargetAccountEmail = prefs[kDisplayedTargetAccountEmailPrefKey];
  self.displayedTargetAccountAvatar =
      prefs[kDisplayedTargetAccountAvatarPrefKey] == [NSNull null]
          ? nil
          : prefs[kDisplayedTargetAccountAvatarPrefKey];
  [self.tableView reloadData];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  switch (itemType) {
    case ItemTypeCardLastDigits: {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeCardHolderName: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(nameEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(nameDidChange:)
                   forControlEvents:UIControlEventEditingChanged |
                                    UIControlEventEditingDidEnd];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeCardExpireMonth: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(monthEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(expireMonthDidChange:)
                   forControlEvents:UIControlEventEditingChanged |
                                    UIControlEventEditingDidEnd];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeCardExpireYear: {
      TableViewTextEditCell* editCell =
          base::mac::ObjCCast<TableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                             action:@selector(yearEditDidBegin)
                   forControlEvents:UIControlEventEditingDidBegin];
      [editCell.textField addTarget:self
                             action:@selector(expireYearDidChange:)
                   forControlEvents:UIControlEventEditingChanged |
                                    UIControlEventEditingDidEnd];
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeCardLegalMessage: {
      TableViewTextLinkCell* linkCell =
          base::mac::ObjCCast<TableViewTextLinkCell>(cell);
      linkCell.delegate = self;
      linkCell.separatorInset =
          UIEdgeInsetsMake(0, self.tableView.bounds.size.width, 0, 0);
      break;
    }
    case ItemTypeCardSave: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(saveCardButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeTargetAccount:
      cell.separatorInset =
          UIEdgeInsetsMake(0, self.tableView.bounds.size.width, 0, 0);
      break;
  }

  return cell;
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

#pragma mark - TableViewTextLinkCellDelegate

- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(CrURL*)URL {
  [self.saveCardModalDelegate dismissModalAndOpenURL:URL.gurl];
}

#pragma mark - Private Methods

// Updates `self.saveCardButtonItem` enabled state taking into account the
// current editable items.
- (void)updateSaveCardButtonState {
  BOOL newButtonState = [self isCurrentInputValid];
  if ([self.saveCardButtonItem isEnabled] != newButtonState) {
    self.saveCardButtonItem.enabled = newButtonState;
    [self reconfigureCellsForItems:@[ self.saveCardButtonItem ]];
  }
}

- (void)saveCardButtonWasPressed:(UIButton*)sender {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalAcceptedTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Accepted];
  [self.saveCardModalDelegate
      saveCardWithCardholderName:self.cardholderNameItem.textFieldValue
                 expirationMonth:self.expirationMonthItem.textFieldValue
                  expirationYear:self.expirationYearItem.textFieldValue];
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

- (void)nameDidChange:(UITextField*)textField {
  BOOL isNameValid = [self isCardholderNameValid:textField.text];

  self.cardholderNameItem.textFieldValue = textField.text;
  [self.cardholderNameItem setHasValidText:isNameValid];
  [self reconfigureCellsForItems:@[ self.cardholderNameItem ]];

  [self updateSaveCardButtonState];
}

- (void)expireMonthDidChange:(UITextField*)textField {
  BOOL isMonthValid =
      [self isExpirationMonthValid:textField.text
                           forYear:self.expirationYearItem.textFieldValue];

  self.expirationMonthItem.textFieldValue = textField.text;
  [self.expirationMonthItem setHasValidText:isMonthValid];
  [self reconfigureCellsForItems:@[ self.expirationMonthItem ]];

  [self updateSaveCardButtonState];
}

- (void)expireYearDidChange:(UITextField*)textField {
  BOOL isYearValid = [self isExpirationYearValid:textField.text];
  // Check if the card month is valid for the newly entered year.
  BOOL isMonthValid =
      [self isExpirationMonthValid:self.expirationMonthItem.textFieldValue
                           forYear:textField.text];

  self.expirationYearItem.textFieldValue = textField.text;
  [self.expirationYearItem setHasValidText:isYearValid];
  [self.expirationMonthItem setHasValidText:isMonthValid];
  [self reconfigureCellsForItems:@[
    self.expirationYearItem, self.expirationMonthItem
  ]];

  [self updateSaveCardButtonState];
}

- (void)dismissInfobarModal {
  base::RecordAction(
      base::UserMetricsAction("MobileMessagesModalCancelledTapped"));
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [self.saveCardModalDelegate dismissInfobarModal:self];
}

#pragma mark - Helpers

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

  return textEditItem;
}

// YES if the current values of the Card are valid.
// TODO(crbug.com/1029067):Ideally the InfobarDelegate should validate
// the correctness of the input.
- (BOOL)isCurrentInputValid {
  if (![self isCardholderNameValid:self.cardholderNameItem.textFieldValue])
    return NO;

  if (![self isExpirationMonthValid:self.expirationMonthItem.textFieldValue
                            forYear:self.expirationYearItem.textFieldValue])
    return NO;

  if (![self isExpirationYearValid:self.expirationYearItem.textFieldValue])
    return NO;

  return YES;
}

// YES if `cardholderName` is valid.
- (BOOL)isCardholderNameValid:(NSString*)cardholderName {
  // Check that the name is not empty or only whitespace.
  NSCharacterSet* set = [NSCharacterSet whitespaceCharacterSet];
  if (![[cardholderName stringByTrimmingCharactersInSet:set] length])
    return NO;

  return YES;
}

// YES if `expirationMonth` is valid for `expirationYear`.
- (BOOL)isExpirationMonthValid:(NSString*)expirationMonth
                       forYear:(NSString*)expirationYear {
  NSNumber* expirationMonthNumber = [self numberFromString:expirationMonth];
  if (!expirationMonthNumber)
    return NO;

  int expirationMonthInteger = [expirationMonthNumber intValue];
  if (expirationMonthInteger <= 0 ||
      expirationMonthInteger > kNumberOfMonthsInYear)
    return NO;

  if ([self currentYearIntValue] ==
      [[self numberFromString:expirationYear] intValue])
    return expirationMonthInteger >= [self currentMonthIntValue];

  return YES;
}

// YES if `expirationYear` is valid for the current date.
- (BOOL)isExpirationYearValid:(NSString*)expirationYear {
  NSNumber* expirationYearNumber = [self numberFromString:expirationYear];
  if (!expirationYearNumber)
    return NO;

  return [self currentYearIntValue] <= [expirationYearNumber intValue];
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

@end

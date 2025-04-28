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
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/save_card_infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_save_card_modal_delegate.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
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
  ItemTypeCardLegalMessage,
  ItemTypeCardSave,
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
// Card Network for accessibility label.
@property(nonatomic, copy) NSString* cardNetwork;
// Card Number to be displayed.
@property(nonatomic, copy) NSString* cardNumber;
// Card Expiration Month to be displayed
@property(nonatomic, copy) NSString* expirationMonth;
// Card Expiration Year to be displayed.
@property(nonatomic, copy) NSString* expirationYear;
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
  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD_CLOSE)
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(dismissInfobarModal)];
  closeButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.leftBarButtonItem = closeButton;
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

  self.saveCardButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeCardSave];
  self.saveCardButtonItem.textAlignment = NSTextAlignmentNatural;
  self.saveCardButtonItem.buttonText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  self.saveCardButtonItem.enabled = !self.currentCardSaveAccepted;
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
  self.cardNetwork = prefs[kCardNetworkPrefKey];
  self.cardNumber = prefs[kCardNumberPrefKey];
  self.expirationMonth = prefs[kExpirationMonthPrefKey];
  self.expirationYear = prefs[kExpirationYearPrefKey];
  self.legalMessages = prefs[kLegalMessagesPrefKey];
  self.currentCardSaveAccepted =
      [prefs[kCurrentCardSaveAcceptedPrefKey] boolValue];
  self.supportsEditing = [prefs[kSupportsEditingPrefKey] boolValue];
  self.displayedTargetAccountEmail = prefs[kDisplayedTargetAccountEmailPrefKey];
  self.logoIcon = prefs[kLogoIconPrefKey];
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
          base::apple::ObjCCast<TableViewTextEditCell>(cell);
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
          base::apple::ObjCCast<TableViewTextEditCell>(cell);
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
          base::apple::ObjCCast<TableViewTextEditCell>(cell);
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
          base::apple::ObjCCast<TableViewTextLinkCell>(cell);
      linkCell.delegate = self;
      linkCell.separatorInset =
          UIEdgeInsetsMake(0, self.tableView.bounds.size.width, 0, 0);
      break;
    }
    case ItemTypeCardSave: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(saveCardButtonWasPressed:)
          forControlEvents:UIControlEventTouchUpInside];
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
  self.saveCardButtonItem.buttonText = @"";
  self.saveCardButtonItem.enabled = NO;
  self.saveCardButtonItem.showsActivityIndicator = !uploadCompleted;
  self.saveCardButtonItem.showsCheckmark = uploadCompleted;
  if (uploadCompleted) {
    self.saveCardButtonItem.buttonBackgroundColor =
        [UIColor colorNamed:kBlue100Color];
    self.saveCardButtonItem.dimBackgroundWhenDisabled = NO;
    // VoiceOver would only announce button's accessibility label when its
    // state changes from enabled to disabled. For confirmation state, the
    // button's state is already disabled from previously showing loading state.
    // Thus posting accessibility announcement here.
    UIAccessibilityPostNotification(
        UIAccessibilityAnnouncementNotification,
        l10n_util::GetNSString(
            IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME));
  }
  // Set the accessibility label on the button that would be read by the
  // VoiceOver when the button is focused. Also, there's no need to specially
  // post accessibility announcement for loading, since VoiceOver will announce
  // the button's accessibility label on its state change from previously
  // showing enabled state when `Save Card` is offered to disabled state while
  // loading.
  self.saveCardButtonItem.buttonAccessibilityLabel =
      uploadCompleted
          ? l10n_util::GetNSString(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME)
          : l10n_util::GetNSString(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_LOADING_THROBBER_ACCESSIBLE_NAME);

  [self updateItemsInProgressState];

  [self reconfigureCellsForItems:@[
    self.cardLastDigitsItem, self.cardholderNameItem, self.expirationMonthItem,
    self.expirationYearItem, self.saveCardButtonItem
  ]];
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

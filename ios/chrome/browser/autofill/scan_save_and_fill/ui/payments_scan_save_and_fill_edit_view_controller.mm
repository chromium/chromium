// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_edit_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_ui_type.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mutator.h"
#import "ios/chrome/browser/autofill/ui_bundled/util/autofill_credit_card_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/util/autofill_settings_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Constant for section 0.
const NSInteger kSectionIdentifierEnumZero = 0;

// Point size for the lock symbol in confirmation state.
const CGFloat kLockSymbolPointSize = 18.0;

// Identifiers for sections in the table view.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCardDetails = kSectionIdentifierEnumZero,
  SectionIdentifierNickname,
};

// Identifiers for items (rows) in the table view.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardNumber = kSectionIdentifierEnumZero,
  ItemTypeCardExpireDate,
  ItemTypeCardHolderName,
  ItemTypeCardCVC,
  ItemTypeCardNickname,
};

// Spacing between elements in the footer stack view.
const CGFloat kFooterSpacing = 16.0;

// Margins for the footer view (top, left, bottom, right).
const UIEdgeInsets kFooterMargins = {8.0, 0.0, 16.0, 0.0};

// Estimated height of the footer view.
const CGFloat kEstimatedFooterHeight = 50.0;

// Height of the header view containing the logo.
const CGFloat kHeaderHeight = 56.0;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Height of the Google Wallet logo.
const CGFloat kGoogleWalletLogoHeight = 32.0;
#endif
}  // namespace

@interface PaymentsScanSaveAndFillEditViewController () <
    TableViewTextEditItemDelegate,
    UITextViewDelegate>
@end

@implementation PaymentsScanSaveAndFillEditViewController {
  // Stored card details.
  NSString* _cardNumber;
  NSString* _expirationDate;
  NSString* _cardholderName;
  NSString* _cardCVC;
  NSString* _nickname;

  // Tracked edit items for diffable data source.
  TableViewTextEditItem* _cardNumberItem;
  TableViewTextEditItem* _expirationDateItem;
  TableViewTextEditItem* _cardholderNameItem;
  TableViewTextEditItem* _cardCVCItem;
  TableViewTextEditItem* _nicknameItem;

  // Legal messages to show at the bottom.
  NSArray<SaveCardMessageWithLinks*>* _legalMessages;

  // Data source.
  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* _diffableDataSource;

  // The save and fill button.
  ChromeButton* _saveButton;

  // Currently focused item.
  TableViewTextEditItem* _focusedItem;

  // Track if the user action has been logged to avoid duplicate logging.
  BOOL _actionLogged;
}

#pragma mark - Initialization

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set title and cancel button.
  self.title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancel)];

  self.tableView.tableHeaderView = [self createHeaderView];
  self.tableView.sectionFooterHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedSectionFooterHeight = kEstimatedFooterHeight;

  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  tapGesture.cancelsTouchesInView = NO;
  [self.view addGestureRecognizer:tapGesture];

  _saveButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _saveButton.title =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_AND_AUTOFILL);
  [_saveButton addTarget:self
                  action:@selector(didTapSave)
        forControlEvents:UIControlEventTouchUpInside];

  RegisterTableViewCell<TableViewTextEditCell>(self.tableView);

  _diffableDataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          TableViewItem* item) {
             TableViewTextEditCell* cell =
                 DequeueTableViewCell<TableViewTextEditCell>(tableView);
             [item configureCell:cell
                      withStyler:[[ChromeTableViewStyler alloc] init]];
             cell.selectionStyle = UITableViewCellSelectionStyleNone;
             return cell;
           }];

  [self loadItemsAndApplySnapshot];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self logScanCardAction:ScanCardOfferToSaveAction::kIgnore];
  [self.delegate onViewDisappeared];
}

#pragma mark - Items Configuration

// Creates the card number input item.
- (TableViewTextEditItem*)cardNumberItem {
  return [self createEditItemWithType:ItemTypeCardNumber
                   fieldNameLabelText:l10n_util::GetNSString(
                                          IDS_IOS_AUTOFILL_CARD_NUMBER)
                       textFieldValue:_cardNumber
                         keyboardType:UIKeyboardTypeNumberPad
                 textFieldPlaceholder:
                     l10n_util::GetNSString(
                         IDS_IOS_AUTOFILL_SCAN_CARD_PLACEHOLDER_CARD_NUMBER)];
}

// Creates the expiration date input item.
- (TableViewTextEditItem*)expirationDateItem {
  return [self
      createEditItemWithType:ItemTypeCardExpireDate
          fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_EXP_DATE)
              textFieldValue:_expirationDate
                keyboardType:UIKeyboardTypeNumberPad
        textFieldPlaceholder:
            l10n_util::GetNSString(
                IDS_IOS_AUTOFILL_SCAN_CARD_PLACEHOLDER_EXPIRY_DATE)];
}

// Creates the cardholder name input item.
- (TableViewTextEditItem*)cardholderNameItem {
  return [self
      createEditItemWithType:ItemTypeCardHolderName
          fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_CARDHOLDER)
              textFieldValue:_cardholderName
                keyboardType:UIKeyboardTypeDefault
        textFieldPlaceholder:nil];
}

// Creates the CVC input item.
- (TableViewTextEditItem*)cardCVCItem {
  return [self createEditItemWithType:ItemTypeCardCVC
                   fieldNameLabelText:l10n_util::GetNSString(
                                          IDS_IOS_AUTOFILL_SCAN_CARD_CVC)
                       textFieldValue:_cardCVC
                         keyboardType:UIKeyboardTypeNumberPad
                 textFieldPlaceholder:
                     l10n_util::GetNSString(
                         IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CVC_OPTIONAL)];
}

// Creates the nickname input item.
- (TableViewTextEditItem*)nicknameItem {
  return [self
      createEditItemWithType:ItemTypeCardNickname
          fieldNameLabelText:l10n_util::GetNSString(IDS_IOS_AUTOFILL_NICKNAME)
              textFieldValue:_nickname
                keyboardType:UIKeyboardTypeDefault
        textFieldPlaceholder:l10n_util::GetNSString(
                                 IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_NICKNAME)];
}

#pragma mark - UI Setup

// Creates and returns the header view containing the title image (logo).
- (UIView*)createHeaderView {
  UIView* headerView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, kHeaderHeight)];

  UIImageView* logoView =
      [[UIImageView alloc] initWithImage:[self aboveTitleImage]];
  logoView.contentMode = UIViewContentModeScaleAspectFit;
  logoView.accessibilityLabel = [self aboveTitleImageAccessibilityLabel];
  logoView.isAccessibilityElement = YES;
  logoView.translatesAutoresizingMaskIntoConstraints = NO;

  [headerView addSubview:logoView];
  [NSLayoutConstraint activateConstraints:@[
    [logoView.centerXAnchor constraintEqualToAnchor:headerView.centerXAnchor],
    [logoView.centerYAnchor constraintEqualToAnchor:headerView.centerYAnchor],
  ]];

  return headerView;
}

// Creates and returns the footer view with legal texts and save button.
- (UIView*)createFooterView {
  UIStackView* footerView = [[UIStackView alloc] initWithFrame:CGRectZero];
  footerView.axis = UILayoutConstraintAxisVertical;
  footerView.spacing = kFooterSpacing;
  footerView.layoutMargins = kFooterMargins;
  footerView.layoutMarginsRelativeArrangement = YES;

  // Add legal messages.
  for (SaveCardMessageWithLinks* message in _legalMessages) {
    UITextView* legalTextView =
        [AutofillCreditCardUtil createTextViewForLegalMessage:message];
    legalTextView.delegate = self;
    [footerView addArrangedSubview:legalTextView];
  }

  // Add save button.
  [footerView addArrangedSubview:_saveButton];

  // Adjust footer frame to fit contents.
  CGSize size =
      [footerView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  footerView.frame =
      CGRectMake(/*x=*/0, /*y=*/0, /*width=*/self.view.frame.size.width,
                 /*height=*/size.height);

  return footerView;
}

#pragma mark - Actions

// Dismisses the keyboard when tapping outside.
- (void)dismissKeyboard {
  [self.view endEditing:YES];
}
// Helper method to log the user action in scan card edit view.
- (void)logScanCardAction:(ScanCardOfferToSaveAction)action {
  if (_actionLogged) {
    return;
  }
  base::UmaHistogramEnumeration("IOS.ScanCardOfferToSave", action);
  _actionLogged = YES;
}

// Triggered when the user taps the save button.
- (void)didTapSave {
  _saveButton.enabled = NO;
  [self logScanCardAction:ScanCardOfferToSaveAction::kAccept];
  [self.mutator didTapSave];
}

// Triggered when the user taps the cancel button.
- (void)didTapCancel {
  [self logScanCardAction:ScanCardOfferToSaveAction::kReject];
  [self.mutator didCancel];
}

#pragma mark - CreditCardScannerConsumer

// Sets the card details returned by the card scanner.
- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear {
  _cardNumber = cardNumber;
  if (expirationMonth.length > 0 && expirationYear.length > 0) {
    _expirationDate =
        [NSString stringWithFormat:@"%@/%@", expirationMonth, expirationYear];
  }

  if (_cardNumberItem && _expirationDateItem) {
    _cardNumberItem.textFieldValue = _cardNumber;
    _expirationDateItem.textFieldValue = _expirationDate;

    [self
        validateAndReconfigureItems:@[ _cardNumberItem, _expirationDateItem ]];
  }
}

#pragma mark - SaveCardBottomSheetConsumer

- (void)showConfirmationState {
  _saveButton.enabled = NO;
  _saveButton.title = nil;
  UIButtonConfiguration* config = _saveButton.configuration;
  config.image = DefaultSymbolWithPointSize(kLockSymbol, kLockSymbolPointSize);
  _saveButton.configuration = config;
  _saveButton.primaryButtonImage = PrimaryButtonImageCustom;
}

- (void)setField:(AutofillCreditCardUIType)type
         isValid:(BOOL)isValid
    errorMessage:(NSString*)errorMessage {
  TableViewTextEditItem* item = nil;
  switch (type) {
    case AutofillCreditCardUIType::kNumber:
      item = _cardNumberItem;
      break;
    case AutofillCreditCardUIType::kExpMonth:
      item = _expirationDateItem;
      break;
    case AutofillCreditCardUIType::kSecurityCode:
      item = _cardCVCItem;
      break;
    case AutofillCreditCardUIType::kNickname:
      item = _nicknameItem;
      break;
    default:
      break;
  }

  if (item) {
    if (item == _focusedItem || item.textFieldValue.length == 0) {
      // Do not show error indicators while editing or if the field is empty.
      item.hasValidText = YES;
    } else {
      // When not editing and not empty (e.g. populated by OCR, or after editing
      // ends), show actual validation status.
      item.hasValidText = isValid;
    }

    [AutofillSettingsUtil updateAccessibilityLabelForItem:item
                                             isInputValid:isValid
                                             errorMessage:errorMessage];
    [self reconfigureCellsForItems:@[ item ]];
  }
}

- (void)setSaveButtonEnabled:(BOOL)enabled {
  _saveButton.enabled = enabled;
}

- (void)setCardNameAndLastFourDigits:(NSString*)cardNameAndLastFourDigits
                  withCardExpiryDate:(NSString*)cardExpiryDate
                         andCardIcon:(UIImage*)issuerIcon
           andCardAccessibilityLabel:(NSString*)accessibilityLabel {
}

- (void)setAboveTitleImage:(UIImage*)logoImage {
}

- (void)setAboveTitleImageDescription:(NSString*)description {
}

- (void)setSubtitle:(NSString*)subtitle {
}

- (void)setAcceptActionText:(NSString*)acceptActionText {
}

- (void)setCancelActionText:(NSString*)cancelActionText {
}

- (void)showLoadingStateWithAccessibilityLabel:(NSString*)accessibilityLabel {
}

- (void)setLegalMessages:(NSArray<SaveCardMessageWithLinks*>*)legalMessages {
  _legalMessages = [legalMessages copy];
  if (self.isViewLoaded) {
    NSDiffableDataSourceSnapshot* snapshot = [_diffableDataSource snapshot];
    // Reload the Nickname section because its footer view contains both the
    // legal messages and the save button. Since this is the last section, it
    // serves as the global form footer.
    [snapshot reloadSectionsWithIdentifiers:@[ @(SectionIdentifierNickname) ]];
    [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  }
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  NSNumber* sectionIdentifier =
      [_diffableDataSource sectionIdentifierForIndex:section];
  if (sectionIdentifier.integerValue == SectionIdentifierNickname) {
    return [self createFooterView];
  }
  return [super tableView:tableView viewForFooterInSection:section];
}

#pragma mark - TableViewTextEditItemDelegate

- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)item {
  _focusedItem = item;
  // Suppress errors as soon as editing starts.
  item.hasValidText = YES;
  [self reconfigureCellsForItems:@[ item ]];
}

- (void)tableViewItemDidChange:(TableViewTextEditItem*)item {
  // Pushes changes for button validation, but visual suppression applies.
  [self pushValueUpdateForItem:item];
}

- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)item {
  _focusedItem = nil;
  // Triggers real validation state display upon exit.
  [self pushValueUpdateForItem:item];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  return NO;
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  __weak __typeof__(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate
        didTapLinkURL:[[CrURL alloc] initWithNSURL:textItem.link]];
  }];
}

#pragma mark - Private

// Helper to validate and reconfigure the cells and save button for multiple
// items.
- (void)validateAndReconfigureItems:(NSArray<TableViewItem*>*)items {
  for (TableViewItem* item in items) {
    TableViewTextEditItem* cardItem =
        base::apple::ObjCCast<TableViewTextEditItem>(item);
    if (cardItem) {
      [self pushValueUpdateForItem:cardItem];
    }
  }
  [self reconfigureCellsForItems:items];
}

// Helper to get the localized error message.
- (NSString*)errorMessageForItem:(TableViewTextEditItem*)item {
  AutofillCreditCardUIType uiType = AutofillCreditCardUIType::kUnknown;
  switch (item.type) {
    case ItemTypeCardNumber:
      uiType = AutofillCreditCardUIType::kNumber;
      break;
    case ItemTypeCardExpireDate:
      uiType = AutofillCreditCardUIType::kExpMonth;
      break;
    case ItemTypeCardHolderName:
      uiType = AutofillCreditCardUIType::kFullName;
      break;
    case ItemTypeCardCVC:
      uiType = AutofillCreditCardUIType::kSecurityCode;
      break;
    case ItemTypeCardNickname:
      uiType = AutofillCreditCardUIType::kNickname;
      break;
  }
  return [AutofillSettingsUtil errorMessageForUIType:uiType];
}

// Helper to validate item and update its state.
- (void)pushValueUpdateForItem:(TableViewTextEditItem*)item {
  AutofillCreditCardUIType uiType = AutofillCreditCardUIType::kUnknown;

  switch (item.type) {
    case ItemTypeCardNumber:
      uiType = AutofillCreditCardUIType::kNumber;
      break;
    case ItemTypeCardExpireDate:
      uiType = AutofillCreditCardUIType::kExpMonth;
      break;
    case ItemTypeCardCVC:
      uiType = AutofillCreditCardUIType::kSecurityCode;
      break;
    case ItemTypeCardNickname:
      uiType = AutofillCreditCardUIType::kNickname;
      break;
    case ItemTypeCardHolderName:
      uiType = AutofillCreditCardUIType::kFullName;
      break;
    default:
      break;
  }

  [self.mutator didUpdateValue:item.textFieldValue forField:uiType];
}

// Returns the image to be used above the title of the bottomsheet.
- (UIImage*)aboveTitleImage {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use the optimized high-resolution iOS symbol for branded builds.
  return MakeSymbolMulticolor(CustomSymbolWithPointSize(
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableWalletBranding)
          ? kGoogleWalletSymbol
          : kGooglePaySymbol,
      kGoogleWalletLogoHeight));
#else
  // Fallback to the generic asset for unbranded builds.
  return NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
#endif
}

// Returns the accessibility label to be used for the title image.
- (NSString*)aboveTitleImageAccessibilityLabel {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return [self.dataSource logoAccessibilityLabel];
#else
  return l10n_util::GetNSString(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME);
#endif
}

// Helper method to create a TableViewTextEditItem with the specified
// properties. Used to construct the input fields in the bottom sheet.
- (TableViewTextEditItem*)createEditItemWithType:(ItemType)itemType
                              fieldNameLabelText:(NSString*)fieldNameLabelText
                                  textFieldValue:(NSString*)textFieldValue
                                    keyboardType:(UIKeyboardType)keyboardType
                            textFieldPlaceholder:
                                (NSString*)textFieldPlaceholder {
  TableViewTextEditItem* item =
      [[TableViewTextEditItem alloc] initWithType:itemType];
  item.delegate = self;
  item.fieldNameLabelText = fieldNameLabelText;
  item.textFieldValue = textFieldValue ?: @"";
  item.textFieldEnabled = YES;
  item.keyboardType = keyboardType;
  if (textFieldPlaceholder) {
    item.textFieldPlaceholder = textFieldPlaceholder;
  }

  return item;
}

// Populates the items array and applies a new snapshot to the diffable data
// source to reflect the current state.
- (void)loadItemsAndApplySnapshot {
  _cardNumberItem = [self cardNumberItem];
  _expirationDateItem = [self expirationDateItem];
  _cardholderNameItem = [self cardholderNameItem];
  _cardCVCItem = [self cardCVCItem];
  _nicknameItem = [self nicknameItem];

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[
    @(SectionIdentifierCardDetails), @(SectionIdentifierNickname)
  ]];

  [snapshot appendItemsWithIdentifiers:@[
    _cardNumberItem, _expirationDateItem, _cardholderNameItem, _cardCVCItem
  ]
             intoSectionWithIdentifier:@(SectionIdentifierCardDetails)];

  [snapshot appendItemsWithIdentifiers:@[ _nicknameItem ]
             intoSectionWithIdentifier:@(SectionIdentifierNickname)];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];

  [self pushValueUpdateForItem:_cardNumberItem];
  [self pushValueUpdateForItem:_expirationDateItem];
  [self pushValueUpdateForItem:_cardCVCItem];
  [self pushValueUpdateForItem:_nicknameItem];
}

// Reconfigures the cells for the specified items to apply UI state changes
- (void)reconfigureCellsForItems:(NSArray<TableViewItem*>*)items {
  NSDiffableDataSourceSnapshot* snapshot = [_diffableDataSource snapshot];
  [snapshot reconfigureItemsWithIdentifiers:items];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

@end

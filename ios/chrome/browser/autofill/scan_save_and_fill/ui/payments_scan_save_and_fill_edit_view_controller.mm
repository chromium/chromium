// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_edit_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/timer/elapsed_timer.h"
#import "build/branding_buildflags.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/payments/payments_autofill_client.h"
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
// Identifier for the single card details section in the table view.
const NSInteger kSectionIdentifierCardDetails = 0;

// Identifiers for items (rows) in the table view.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCardNumber = 0,
  ItemTypeCardExpireDate,
  ItemTypeCardHolderName,
  ItemTypeCardCVC,
  ItemTypeCardNickname,
};

// Spacing between elements in the footer stack view.
const CGFloat kFooterSpacing = 16.0;

// Margins for the footer view (top, left, bottom, right).
const UIEdgeInsets kFooterMargins = {24.0, 16.0, 16.0, 16.0};

// Estimated height of the footer view.
const CGFloat kEstimatedFooterHeight = 50.0;

// Height of the header view containing the logo.
const CGFloat kHeaderHeight = 56.0;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Height of the Google Wallet logo.
const CGFloat kGoogleWalletLogoHeight = 32.0;
#endif

// Separator for expiration date components (Month/Year).
NSString* const kDateSeparator = @"/";

// Alpha value for the save button in loading and success states.
const CGFloat kSaveButtonActiveAlpha = 1.0;

// State of the save button during async transitions.
enum class SaveButtonState {
  kLoading,
  kConfirmation,
};

// Styles the button for the loading state.
void StyleButtonForLoading(UIButtonConfiguration* config) {
  config.showsActivityIndicator = YES;
  config.background.backgroundColor = [UIColor colorNamed:kGrey400Color];
  config.baseForegroundColor = [UIColor whiteColor];
  config.activityIndicatorColorTransformer = ^UIColor*(UIColor* _) {
    return [UIColor whiteColor];
  };
}

// Styles the button for the success/confirmation state.
void StyleButtonForConfirmation(UIButtonConfiguration* config) {
  config.showsActivityIndicator = NO;
  config.background.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
  UIColor* blueColor = [UIColor colorNamed:kBlueColor];
  config.baseForegroundColor = blueColor;
  config.image =
      [config.image imageWithTintColor:blueColor
                         renderingMode:UIImageRenderingModeAlwaysOriginal];
}
}  // namespace

@interface PaymentsScanSaveAndFillEditViewController () <
    TableViewTextEditItemDelegate,
    UITextViewDelegate>
@end

@implementation PaymentsScanSaveAndFillEditViewController {
  // The table view.
  UITableView* _tableView;

  // The bottom sticky container for the save button.
  UIStackView* _bottomContainerView;

  // Stored card details.
  NSString* _cardNumber;
  NSString* _expirationDate;
  NSString* _cardholderName;
  NSString* _cardCVC;
  NSString* _nickname;

  // Initial scanned details.
  NSString* _scannedCardNumber;
  NSString* _scannedExpirationMonth;
  NSString* _scannedExpirationYear;

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

  // Timer to track the end-to-end latency of the card scanning session.
  base::ElapsedTimer _sessionTimer;
}

#pragma mark - Initialization

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  return self;
}

#pragma mark - Properties

- (UITableView*)tableView {
  return _tableView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  // Set title and cancel button.
  self.title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapCancel)];

  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:ChromeTableViewStyle()];
  _tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _tableView.delegate = self;
  _tableView.tableHeaderView = [self createHeaderView];
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.separatorStyle = UITableViewCellSeparatorStyleSingleLine;
  _tableView.sectionFooterHeight = UITableViewAutomaticDimension;
  _tableView.estimatedSectionFooterHeight = kEstimatedFooterHeight;

  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  tapGesture.cancelsTouchesInView = NO;
  [self.view addGestureRecognizer:tapGesture];

  [self setupBottomContainerAndConstraints];

  RegisterTableViewCell<TableViewTextEditCell>(_tableView);

  _diffableDataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:_tableView
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
  [self updateBottomContainerView];
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

// Updates the bottom container view with the save button.
- (void)updateBottomContainerView {
  for (UIView* view in _bottomContainerView.arrangedSubviews) {
    [view removeFromSuperview];
  }
  // Add save button.
  [_bottomContainerView addArrangedSubview:_saveButton];
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

  if (action == ScanCardOfferToSaveAction::kAccept) {
    if (_scannedCardNumber.length > 0) {
      BOOL numberEdited =
          ![_cardNumberItem.textFieldValue isEqualToString:_scannedCardNumber];
      base::UmaHistogramBoolean("IOS.ScannedCard.NumberEdited", numberEdited);
    }

    NSArray<NSString*>* components = [_expirationDateItem.textFieldValue
        componentsSeparatedByString:kDateSeparator];
    NSString* currentMonth = components.count > 0 ? components[0] : @"";
    NSString* currentYear = components.count > 1 ? components[1] : @"";

    if (_scannedExpirationMonth.length > 0) {
      BOOL monthEdited =
          ![currentMonth isEqualToString:_scannedExpirationMonth];
      base::UmaHistogramBoolean("IOS.ScannedCard.ExpMonthEdited", monthEdited);
    }
    if (_scannedExpirationYear.length > 0) {
      BOOL yearEdited = ![currentYear isEqualToString:_scannedExpirationYear];
      base::UmaHistogramBoolean("IOS.ScannedCard.ExpYearEdited", yearEdited);
    }
  }
  _actionLogged = YES;
}

// Triggered when the user taps the save button.
- (void)didTapSave {
  [self showLoadingStateWithAccessibilityLabel:nil];
  [self logScanCardAction:ScanCardOfferToSaveAction::kAccept];

  // Defer the mutator call to the next run loop cycle to allow UIKit to
  // establish the spinner layout stably before the local save synchronous
  // completion triggers the progress dialog and dismisses the view controller.
  __weak __typeof__(_mutator) weakMutator = _mutator;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakMutator didTapSave];
      }));
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
  base::UmaHistogramTimes("IOS.ScanCard.EndToEndLatency",
                          _sessionTimer.Elapsed());

  _scannedCardNumber = cardNumber;
  _scannedExpirationMonth = expirationMonth;
  _scannedExpirationYear = expirationYear;

  _cardNumber = cardNumber;
  if (expirationMonth.length > 0 && expirationYear.length > 0) {
    _expirationDate =
        [NSString stringWithFormat:@"%@%@%@", expirationMonth, kDateSeparator,
                                   expirationYear];
  }

  std::string appLocale =
      GetApplicationContext()->GetApplicationLocaleStorage()->Get();

  base::UmaHistogramBoolean(
      "IOS.ScanCardOfferToSave.ValidNumber",
      [AutofillCreditCardUtil isValidCreditCardNumber:cardNumber
                                             appLocal:appLocale]);

  base::UmaHistogramBoolean(
      "IOS.ScanCardOfferToSave.ValidExpMonth",
      [AutofillCreditCardUtil
          isValidCreditCardExpirationMonth:expirationMonth]);

  base::UmaHistogramBoolean(
      "IOS.ScanCardOfferToSave.ValidExpYear",
      [AutofillCreditCardUtil isValidCreditCardExpirationYear:expirationYear
                                                     appLocal:appLocale]);

  if (_cardNumberItem && _expirationDateItem) {
    _cardNumberItem.textFieldValue = _cardNumber;
    _expirationDateItem.textFieldValue = _expirationDate;

    [self
        validateAndReconfigureItems:@[ _cardNumberItem, _expirationDateItem ]];
  }
}

#pragma mark - SaveCardBottomSheetConsumer

- (void)showConfirmationState {
  [self transitionSaveButtonToState:SaveButtonState::kConfirmation
                          withImage:PrimaryButtonImageCheckmark];
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
  if (accessibilityLabel.length > 0) {
    _saveButton.accessibilityLabel = accessibilityLabel;
  }

  [self transitionSaveButtonToState:SaveButtonState::kLoading
                          withImage:PrimaryButtonImageSpinner];
}

- (void)setLegalMessages:(NSArray<SaveCardMessageWithLinks*>*)legalMessages {
  _legalMessages = [legalMessages copy];
  if (self.isViewLoaded) {
    NSDiffableDataSourceSnapshot* snapshot = [_diffableDataSource snapshot];
    // Reload the CardDetails section because its footer view contains both the
    // legal messages and the save button. Since this is the last section, it
    // serves as the global form footer.
    [snapshot
        reloadSectionsWithIdentifiers:@[ @(kSectionIdentifierCardDetails) ]];
    [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
  }
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  NSNumber* sectionIdentifier =
      [_diffableDataSource sectionIdentifierForIndex:section];
  if (sectionIdentifier.integerValue == kSectionIdentifierCardDetails) {
    UIStackView* footerView = [[UIStackView alloc] initWithFrame:CGRectZero];
    footerView.axis = UILayoutConstraintAxisVertical;
    footerView.spacing = kFooterSpacing;
    footerView.layoutMargins = kFooterMargins;
    footerView.layoutMarginsRelativeArrangement = YES;

    for (SaveCardMessageWithLinks* message in _legalMessages) {
      UITextView* legalTextView =
          [AutofillCreditCardUtil createTextViewForLegalMessage:message];
      legalTextView.delegate = self;
      [footerView addArrangedSubview:legalTextView];
    }
    return footerView;
  }
  return nil;
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

// Transitions the save button to a textless state displaying the specified
// image and applies custom configuration styling based on `state`.
- (void)transitionSaveButtonToState:(SaveButtonState)state
                          withImage:(PrimaryButtonImage)primaryButtonImage {
  _saveButton.enabled = NO;
  _saveButton.title = nil;
  _saveButton.primaryButtonImage = primaryButtonImage;

  _saveButton.configurationUpdateHandler = ^(UIButton* button) {
    UIButtonConfiguration* config = button.configuration;
    config.title = nil;
    config.attributedTitle = nil;

    switch (state) {
      case SaveButtonState::kLoading:
        StyleButtonForLoading(config);
        break;
      case SaveButtonState::kConfirmation:
        StyleButtonForConfirmation(config);
        break;
    }

    button.alpha = kSaveButtonActiveAlpha;
    button.configuration = config;
  };
  [_saveButton setNeedsUpdateConfiguration];
}

// Sets up the bottom container view and subview constraints.
- (void)setupBottomContainerAndConstraints {
  _saveButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _saveButton.title =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_AND_AUTOFILL);
  [_saveButton addTarget:self
                  action:@selector(didTapSave)
        forControlEvents:UIControlEventTouchUpInside];

  _bottomContainerView = [[UIStackView alloc] initWithFrame:CGRectZero];
  _bottomContainerView.axis = UILayoutConstraintAxisVertical;
  _bottomContainerView.spacing = kFooterSpacing;
  _bottomContainerView.layoutMargins = kFooterMargins;
  _bottomContainerView.layoutMarginsRelativeArrangement = YES;
  _bottomContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _bottomContainerView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self.view addSubview:_tableView];
  [self.view addSubview:_bottomContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_tableView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_tableView.bottomAnchor
        constraintEqualToAnchor:_bottomContainerView.topAnchor],

    [_bottomContainerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_bottomContainerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_bottomContainerView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
  ]];
}

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
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleWalletSymbol, kGoogleWalletLogoHeight));
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

  [snapshot
      appendSectionsWithIdentifiers:@[ @(kSectionIdentifierCardDetails) ]];

  [snapshot appendItemsWithIdentifiers:@[
    _cardNumberItem, _expirationDateItem, _cardholderNameItem, _cardCVCItem,
    _nicknameItem
  ]
             intoSectionWithIdentifier:@(kSectionIdentifierCardDetails)];

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

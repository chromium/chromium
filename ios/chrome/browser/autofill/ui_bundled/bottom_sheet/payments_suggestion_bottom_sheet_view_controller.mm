// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/grit/components_scaled_resources.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/autofill/model/credit_card/credit_card_data.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/payments_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Credit Card icon corner radius.
CGFloat const kCreditCardIconCornerRadius = 5;

// Default spacing used for the views in the bottom sheet.
CGFloat const kSpacing = 10;

// Spacing use for the spacing before the logo title in the bottom sheet.
CGFloat const kSpacingBeforeImage = 16;

// Spacing use for the spacing after the logo title in the bottom sheet.
CGFloat const kSpacingAfterImage = 4;

// Height of the logo used as the title of the bottom sheet.
CGFloat const kTitleLogoHeight = 32;

}  // namespace

@interface PaymentsSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDataSource> {
  // List of credit cards and icon for the bottom sheet.
  NSArray<CreditCardData*>* _creditCardData;

  // URL of the current page the bottom sheet is being displayed on.
  GURL _URL;

  // YES if the primary button is active where actions are effective.
  BOOL _primaryButtonActive;
}

// The payments controller handler used to open the payments options.
@property(nonatomic, weak) id<PaymentsSuggestionBottomSheetHandler> handler;

// YES if the GPay logo should be shown to the user.
@property(nonatomic, assign) BOOL showGooglePayLogo;

// Whether the bottom sheet will be disabled on exit. Default is YES.
@property(nonatomic, assign) BOOL disableBottomSheetOnExit;

@end

@implementation PaymentsSuggestionBottomSheetViewController

- (instancetype)initWithHandler:
                    (id<PaymentsSuggestionBottomSheetHandler>)handler
                            URL:(const GURL&)URL {
  self = [super init];
  if (self) {
    self.handler = handler;
    _URL = URL;
    self.disableBottomSheetOnExit = YES;
    _primaryButtonActive = NO;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [self titleImage];
  self.imageViewAccessibilityLabel = [NSString
      stringWithFormat:@"%@. %@",
                       l10n_util::GetNSString(
                           self.showGooglePayLogo
                               ? IDS_IOS_AUTOFILL_WALLET_SERVER_NAME
                               : IDS_IOS_PRODUCT_NAME),
                       l10n_util::GetNSString(
                           IDS_IOS_PAYMENT_BOTTOM_SHEET_SELECT_PAYMENT_METHOD)];
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImage;
  self.customSpacingAfterImage = kSpacingAfterImage;
  self.subtitleTextStyle = UIFontTextStyleFootnote;
  std::u16string formattedURL =
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          _URL);
  self.subtitleString = l10n_util::GetNSStringF(
      IDS_IOS_PAYMENT_BOTTOM_SHEET_SUBTITLE, formattedURL);
  self.customSpacing = kSpacing;

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.actionHandler = self;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PAYMENT_BOTTOM_SHEET_CONTINUE);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PAYMENT_BOTTOM_SHEET_USE_KEYBOARD);
  self.secondaryActionImage =
      DefaultSymbolWithPointSize(kKeyboardSymbol, kSymbolActionPointSize);

  [super viewDidLoad];

  [self adjustTransactionsPrimaryActionButtonHorizontalConstraints];

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:TraitCollectionSetForTraits(
                                      @[ UITraitUserInterfaceStyle.self ])
                       withAction:@selector(resizeLogoOnTraitChange)];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.imageViewAccessibilityLabel);
  [self.delegate paymentsBottomSheetViewDidAppear];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    [self resizeLogoOnTraitChange];
  }
}
#endif

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (self.disableBottomSheetOnExit) {
    [self.delegate disableBottomSheetAndRefocus:YES];
  }
  [self.handler viewDidDisappear];
}

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setCreditCardData:(NSArray<CreditCardData*>*)creditCardData
        showGooglePayLogo:(BOOL)showGooglePayLogo {
  BOOL requiresUpdate = (_creditCardData != nil);
  _creditCardData = creditCardData;
  self.showGooglePayLogo = showGooglePayLogo;
  if (requiresUpdate) {
    [self reloadTableViewData];
  }
}

- (void)dismiss {
  [self dismissViewControllerAnimated:NO completion:NULL];
}

- (void)activatePrimaryButton {
  _primaryButtonActive = YES;
}

#pragma mark - UITableViewDelegate

// Long press open context menu.
- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  __weak __typeof(self) weakSelf = self;
  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    NSMutableArray<UIMenu*>* menuElements =
        [[NSMutableArray alloc] initWithArray:suggestedActions];

    PaymentsSuggestionBottomSheetViewController* strongSelf = weakSelf;
    if (strongSelf) {
      [menuElements
          addObject:[UIMenu menuWithTitle:@""
                                    image:nil
                               identifier:nil
                                  options:UIMenuOptionsDisplayInline
                                 children:@[
                                   [strongSelf openPaymentMethodsAction]
                                 ]]];
      [menuElements
          addObject:[UIMenu menuWithTitle:@""
                                    image:nil
                               identifier:nil
                                  options:UIMenuOptionsDisplayInline
                                 children:@[
                                   [strongSelf
                                       openPaymentDetailsForIndexPath:indexPath]
                                 ]]];
    }
    return [UIMenu menuWithTitle:@"" children:menuElements];
  };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self rowCount];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewDetailIconCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell"];
  return [self layoutCell:cell
        forTableViewWidth:tableView.frame.size.width
              atIndexPath:indexPath];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate didTapOnPrimaryButton];

  if (!_primaryButtonActive) {
    return;
  }

  self.disableBottomSheetOnExit = NO;

  base::RecordAction(
      base::UserMetricsAction("BottomSheet_CreditCard_SuggestionAccepted"));
  NSInteger index = [self selectedRow];
  base::UmaHistogramSparse(
      "Autofill.UserAcceptedSuggestionAtIndex.CreditCard.BottomSheet", index);
  [self.handler primaryButtonTappedForCard:_creditCardData[index]
                                   atIndex:index];

  if ([self rowCount] > 1) {
    base::UmaHistogramCounts100("Autofill.TouchToFill.CreditCard.SelectedIndex",
                                (int)index);
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.handler secondaryButtonTapped];
}

#pragma mark - TableViewBottomSheetViewController

- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  [tableView registerClass:TableViewDetailIconCell.class
      forCellReuseIdentifier:@"cell"];

  return tableView;
}

- (NSUInteger)rowCount {
  return _creditCardData.count;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  TableViewDetailIconCell* cell = [[TableViewDetailIconCell alloc] init];
  // Setup UI same as real cell.
  CGFloat tableWidth = [self tableViewWidth];
  cell = [self layoutCell:cell
        forTableViewWidth:tableWidth
              atIndexPath:[NSIndexPath indexPathForRow:index inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)].height;
}

#pragma mark - Private

// Returns the title logo image that is resized to the correct size for the
// bottom sheet. It should return the Google Pay badge
// image corresponding to the current UIUserInterfaceStyle (light/dark mode) if
// `showGooglePayLogo` value is YES otherwise the Chrome logo is shown.
- (UIImage*)titleImage {
  UIImage* image;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  image = MakeSymbolMulticolor(CustomSymbolWithPointSize(
      self.showGooglePayLogo ? kGooglePaySymbol : kMulticolorChromeballSymbol,
      kTitleLogoHeight));
#else
  image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                             kTitleLogoHeight);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

  return image;
}

// Returns the string to display at a given row in the table view.
- (NSString*)suggestionAtRow:(NSInteger)row {
  return [_creditCardData[row] cardNameAndLastFourDigits];
}

// Returns the display description at a given row in the table view.
- (NSString*)descriptionAtRow:(NSInteger)row {
  return [_creditCardData[row] cardDetails];
}

// Returns the credit card icon at a given row in the table view.
- (UIImage*)iconAtRow:(NSInteger)row {
  return [_creditCardData[row] icon];
}

// Returns an accessible card name at a given row in the table view.
- (NSString*)accessibleCardNameAtRow:(NSInteger)row {
  return l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
      base::SysNSStringToUTF16([_creditCardData[row] accessibleCardName]), u"");
}

// Returns the accessibility value for the card at a given row in the table
// view.
- (NSString*)accessibilityValueForCardAtRow:(NSInteger)row {
  return l10n_util::GetNSStringF(IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE,
                                 base::NumberToString16(row + 1),
                                 base::NumberToString16([self rowCount]));
}

// Creates the UI action used to open the payment methods view.
- (UIAction*)openPaymentMethodsAction {
  __weak __typeof(self) weakSelf = self;
  void (^paymentMethodsButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Payment Methods.
    weakSelf.disableBottomSheetOnExit = NO;
    [weakSelf.handler displayPaymentMethods];
  };
  UIImage* creditCardIcon =
      DefaultSymbolWithPointSize(kCreditCardSymbol, kSymbolActionPointSize);
  return [UIAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PAYMENT_BOTTOM_SHEET_MANAGE_PAYMENT_METHODS)
                image:creditCardIcon
           identifier:nil
              handler:paymentMethodsButtonTapHandler];
}

// Creates the UI action used to open the payment details for form suggestion at
// index path.
// Test.
- (UIAction*)openPaymentDetailsForIndexPath:(NSIndexPath*)indexPath {
  __weak __typeof(self) weakSelf = self;
  NSString* creditCardIdentifier =
      [_creditCardData[indexPath.row] backendIdentifier];

  void (^showDetailsButtonTapHandler)(UIAction*) = ^(UIAction* action) {
    // Open Payments Details.
    weakSelf.disableBottomSheetOnExit = NO;
    [weakSelf.handler
        displayPaymentDetailsForCreditCardIdentifier:creditCardIdentifier];
  };

  UIImage* infoIcon =
      DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolActionPointSize);
  return
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_PAYMENT_BOTTOM_SHEET_SHOW_DETAILS)
                          image:infoIcon
                     identifier:nil
                        handler:showDetailsButtonTapHandler];
}

// Layouts the cell for the table view with the payment info at the specific
// index path.
- (TableViewDetailIconCell*)layoutCell:(TableViewDetailIconCell*)cell
                     forTableViewWidth:(CGFloat)tableViewWidth
                           atIndexPath:(NSIndexPath*)indexPath {
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;
  cell.customAccessibilityLabel = [self accessibleCardNameAtRow:indexPath.row];
  cell.accessibilityValue = [self accessibilityValueForCardAtRow:indexPath.row];

  [cell setDetailText:[self descriptionAtRow:indexPath.row]];
  [cell setIconImage:[self iconAtRow:indexPath.row]
            tintColor:nil
      backgroundColor:cell.backgroundColor
         cornerRadius:kCreditCardIconCornerRadius];
  [cell updateIconBackgroundWidthToFitContent:YES];
  [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];

  cell.textLabel.text = [self suggestionAtRow:indexPath.row];
  cell.textLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.textLabel.numberOfLines = 1;

  // If we have the potential presence of a virtual card, the textLabel on its
  // own is no longer a unique identifier, so we include the description.
  cell.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@ %@", cell.textLabel.text,
                                 [self descriptionAtRow:indexPath.row]];

  cell.separatorInset = [self separatorInsetForTableViewWidth:tableViewWidth
                                                  atIndexPath:indexPath];
  cell.accessoryType = [self accessoryType:indexPath];
  return cell;
}

// Resizes the GPay logo to match the new trait collection.
- (void)resizeLogoOnTraitChange {
  self.image = [self titleImage];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.textContainerInset = UIEdgeInsetsZero;
}

@end

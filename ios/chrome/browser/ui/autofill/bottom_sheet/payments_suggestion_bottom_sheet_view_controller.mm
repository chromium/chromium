// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/grit/components_scaled_resources.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_data.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Credit Card icon corner radius.
CGFloat const kCreditCardIconCornerRadius = 5;

// Default spacing use for the views in the bottom sheet.
CGFloat const kSpacing = 10;

// Spacing use for the spacing before the logo title in the bottom sheet.
CGFloat const kSpacingBeforeImage = 16;

// Spacing use for the spacing after the logo title in the bottom sheet.
CGFloat const kSpacingAfterImage = 4;

// Height of the logo used as the title of the bottom sheet.
CGFloat const kTitleLogoHeight = 24;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Default custom detent identifier.
NSString* const kCustomDetentIdentifier = @"customDetent";

}  // namespace

@interface PaymentsSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UISheetPresentationControllerDelegate,
    UITableViewDataSource> {
  // If YES: the table view is currently showing 2.5 credit card suggestions.
  // If NO: the table view is currently showing all credit card suggestions.
  BOOL _tableViewIsMinimized;

  // Height constraint for the bottom sheet when showing 2.5 credit card
  // suggestions.
  NSLayoutConstraint* _minimizedHeightConstraint;

  // Height constraint for the bottom sheet when showing all suggestions.
  NSLayoutConstraint* _heightConstraint;

  // List of credit cards and icon for the bottom sheet.
  NSArray<id<PaymentsSuggestionBottomSheetData>>* _creditCardData;

  // URL of the current page the bottom sheet is being displayed on.
  GURL _URL;
}

// The payments controller handler used to open the payments options.
@property(nonatomic, weak) id<PaymentsSuggestionBottomSheetHandler> handler;

// YES if the GPay logo should be shown to the user.
@property(nonatomic, assign) BOOL showGooglePayLogo;

// Whether the bottom sheet will be disabled on exit. Default is YES.
@property(nonatomic, assign) BOOL disableBottomSheetOnExit;

// YES if the expanded bottom sheet size takes the whole screen.
@property(nonatomic, assign) BOOL expandSizeTooLarge;

// Keep track of the minimized state height.
@property(nonatomic, assign) absl::optional<CGFloat> minimizedStateHeight;

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
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  // If the user has more than 2 credit cards, we want to start with the
  // minimized bottom sheet.
  _tableViewIsMinimized = _creditCardData.count > 2;

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

  // Set selection to the first one.
  [self selectFirstRow];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    // Make sure the GPay logo matches the new trait collection.
    self.image = [self titleImage];
  }

  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    self.minimizedStateHeight = absl::nullopt;
    [self updateHeight];
  }
}

- (void)viewIsAppearing:(BOOL)animated {
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 170000
  [super viewIsAppearing:animated];
#endif

  [self updateHeight];
}

- (void)viewDidDisappear:(BOOL)animated {
  if (self.disableBottomSheetOnExit) {
    [self.delegate disableBottomSheet];
  }
  [self.handler viewDidDisappear:animated];
}

- (CGFloat)initialHeight {
  if (!self.minimizedStateHeight.has_value()) {
    self.minimizedStateHeight = [self preferredHeightForContent];
  }
  return self.minimizedStateHeight.value();
}

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setCreditCardData:
            (NSArray<id<PaymentsSuggestionBottomSheetData>>*)creditCardData
        showGooglePayLogo:(BOOL)showGooglePayLogo {
  BOOL requiresUpdate = (_creditCardData != nil);
  _creditCardData = creditCardData;
  self.showGooglePayLogo = showGooglePayLogo;
  if (requiresUpdate) {
    [self reloadTableViewData];
    [self updateHeight];
  }
}

- (void)dismiss {
  [self dismissViewControllerAnimated:NO completion:NULL];
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
  return _creditCardData.count;
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
  NSInteger index = [self selectedRow];
  [self.handler primaryButtonTapped:[_creditCardData[index] backendIdentifier]];

  if (_creditCardData.count > 1) {
    base::UmaHistogramCounts100("Autofill.TouchToFill.CreditCard.SelectedIndex",
                                (int)index);
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.handler secondaryButtonTapped];
}

#pragma mark - Private

// Returns the title logo image that is resized to the correct size for the
// bottom sheet. It should return the Google Pay badge
// image corresponding to the current UIUserInterfaceStyle (light/dark mode) if
// `showGooglePayLogo` value is YES otherwise the Chrome logo is shown.
- (UIImage*)titleImage {
  UIImage* image;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  image = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kChromeSymbol, kTitleLogoHeight));
#else
  image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                             kTitleLogoHeight);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

  if (self.showGooglePayLogo) {
    image = self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
                ? NativeImage(IDR_AUTOFILL_GOOGLE_PAY_DARK)
                : NativeImage(IDR_AUTOFILL_GOOGLE_PAY);

    // Using kTitleLogoHeight (24pt) returns a GPay logo too small, so we are
    // using 28pt to ressemble the mocks.
    CGFloat gPayLogoSize = 28;
    CGFloat ratio = gPayLogoSize / image.size.height;
    CGSize imageSize = CGSizeMake(image.size.width * ratio, gPayLogoSize);
    UIGraphicsImageRenderer* renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:imageSize];
    image =
        [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
          [image drawInRect:(CGRect){.origin = CGPointZero, .size = imageSize}];
        }];
  }

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
  return [_creditCardData[row] accessibleCardName];
}

// Creates the payments bottom sheet's table view.
- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  [tableView registerClass:TableViewDetailIconCell.class
      forCellReuseIdentifier:@"cell"];

  _minimizedHeightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:[self tableViewEstimatedRowHeight] *
                                [self initialNumberOfVisibleCells]];
  _minimizedHeightConstraint.priority = UILayoutPriorityDefaultLow;
  _heightConstraint = [tableView.heightAnchor
      constraintEqualToConstant:[self tableViewEstimatedRowHeight] *
                                _creditCardData.count];

  _minimizedHeightConstraint.active = _tableViewIsMinimized;
  _heightConstraint.active = !_tableViewIsMinimized;

  return tableView;
}

// Updates the bottom sheet's height based on the number of credit cards to
// show.
- (void)updateHeight {
  BOOL useMinimizedState = _tableViewIsMinimized;

  if (_creditCardData.count) {
    [self.view layoutIfNeeded];
    CGFloat fullHeight = [self computeTableViewHeightForAllCells];
    if (fullHeight > 0) {
      // Update height constraints for the table view.
      _heightConstraint.constant = fullHeight;

      if (_creditCardData.count > 2) {
        _minimizedHeightConstraint.constant =
            [self computeTableViewHeightForMinimizedState];
      } else {
        _minimizedHeightConstraint.constant = fullHeight;
      }

      // Do not use minized state if it is larger than the superview height.
      useMinimizedState &=
          [self initialHeight] < self.parentViewControllerHeight;
    }
  }

  // Update the custom detent with the correct initial height for the bottom
  // sheet. (Initial height is not calculated properly in -viewDidLoad, but we
  // need to setup the bottom sheet in that method so there is not a delay when
  // showing the table view and the action buttons).
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.delegate = self;
  // Setup the minimized height (if the user has more than 2 credit cards).
  NSMutableArray* currentDetents = [[NSMutableArray alloc] init];
  if (@available(iOS 16, *)) {
    if (useMinimizedState) {
      // Show gradient view when the user is in minimized state to show that the
      // view can be scrolled.
      [self displayGradientView:YES];

      CGFloat bottomSheetHeight = [self initialHeight];
      auto detentBlock = ^CGFloat(
          id<UISheetPresentationControllerDetentResolutionContext> context) {
        return bottomSheetHeight;
      };
      UISheetPresentationControllerDetent* customDetent =
          [UISheetPresentationControllerDetent
              customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                                resolver:detentBlock];
      [currentDetents addObject:customDetent];
    }
  }

  // Done calculating the height for the bottom sheet for 2.5 credit card
  // suggestions, disable minimized height constraint.
  _minimizedHeightConstraint.active = NO;
  _heightConstraint.active = YES;

  // Calculate the full height of the bottom sheet with the minimized height
  // constraint disabled.
  if (@available(iOS 16, *)) {
    CGFloat fullHeight = [self preferredHeightForContent];
    auto fullHeightBlock = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      self.expandSizeTooLarge = (fullHeight > context.maximumDetentValue);
      return self.expandSizeTooLarge ? context.maximumDetentValue : fullHeight;
    };
    UISheetPresentationControllerDetent* customDetentExpand =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomDetentIdentifier
                              resolver:fullHeightBlock];
    [currentDetents addObject:customDetentExpand];
    presentationController.detents = currentDetents;
    presentationController.selectedDetentIdentifier =
        useMinimizedState ? kCustomMinimizedDetentIdentifier
                          : kCustomDetentIdentifier;
  }
}

// Returns whether the provided index path points to the last row of the table
// view.
- (BOOL)isLastRow:(NSIndexPath*)indexPath {
  return NSUInteger(indexPath.row) == (_creditCardData.count - 1);
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

- (CGFloat)initialNumberOfVisibleCells {
  return 2.5;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  TableViewDetailIconCell* cell = [[TableViewDetailIconCell alloc] init];
  // Setup UI same as real cell.
  cell = [self layoutCell:cell
        forTableViewWidth:[self tableViewWidth]
              atIndexPath:[NSIndexPath indexPathForRow:index inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake([self tableViewWidth], 1)]
      .height;
}

// Mocks the all the cells to calculate the real table view height.
- (CGFloat)computeTableViewHeightForAllCells {
  CGFloat height = 0;
  for (NSUInteger i = 0; i < _creditCardData.count; i++) {
    CGFloat cellHeight = [self computeTableViewCellHeightAtIndex:i];
    height += cellHeight;
  }
  return height;
}

// Mocks the cells to calculate the real table view height in minized state.
- (CGFloat)computeTableViewHeightForMinimizedState {
  CHECK(_creditCardData.count > [self initialNumberOfVisibleCells]);
  CGFloat height = 0;
  NSInteger count =
      static_cast<NSInteger>(floor([self initialNumberOfVisibleCells]));
  for (NSInteger i = 0; i <= count; i++) {
    CGFloat cellHeight = [self computeTableViewCellHeightAtIndex:i];
    if (i == count) {
      CGFloat diff = abs([self initialNumberOfVisibleCells] - count);
      height += cellHeight * diff;
    } else {
      height += cellHeight;
    }
  }
  return height;
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

  cell.textLabel.text = [self suggestionAtRow:indexPath.row];
  [cell setDetailText:[self descriptionAtRow:indexPath.row]];
  [cell setIconImage:[self iconAtRow:indexPath.row]
            tintColor:nil
      backgroundColor:cell.backgroundColor
         cornerRadius:kCreditCardIconCornerRadius];
  [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];

  // Make separator invisible on last cell
  CGFloat separatorLeftMargin =
      [self isLastRow:indexPath] ? tableViewWidth : kTableViewHorizontalSpacing;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorLeftMargin, 0.f, 0.f);

  if (_creditCardData.count > 1 && [self selectedRow] == indexPath.row) {
    cell.accessoryType = UITableViewCellAccessoryCheckmark;
  } else {
    cell.accessoryType = UITableViewCellAccessoryNone;
  }
  cell.textLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  cell.textLabel.numberOfLines = 1;
  return cell;
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)sheetPresentationControllerDidChangeSelectedDetentIdentifier:
    (UISheetPresentationController*)sheetPresentationController
    API_AVAILABLE(ios(16)) {
  // Show the gradient view to let the user know that the view can be scrolled
  // when the bottom sheet is in minimized state or if the expanded state takes
  // more space than the screen.
  NSString* selectedDetentIdentifier =
      sheetPresentationController.selectedDetentIdentifier;
  [self displayGradientView:selectedDetentIdentifier ==
                                kCustomMinimizedDetentIdentifier ||
                            (selectedDetentIdentifier ==
                                 kCustomDetentIdentifier &&
                             self.expandSizeTooLarge)];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.textContainerInset = UIEdgeInsetsZero;
}

@end

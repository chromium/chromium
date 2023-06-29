// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_data.h"
#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Credit Card icon corner radius.
CGFloat const kCreditCardIconCornerRadius = 5;

}  // namespace

@interface PaymentsSuggestionBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDataSource> {
  // Height constraint for the bottom sheet when showing all suggestions.
  NSLayoutConstraint* _heightConstraint;

  // Table view for the list of suggestions.
  UITableView* _tableView;

  // List of credit cards and icon for the bottom sheet.
  NSArray<id<PaymentsSuggestionBottomSheetData>>* _creditCardData;

  // View which contains the GPay logo.
  UIImageView* _logoImageView;
}

@end

@implementation PaymentsSuggestionBottomSheetViewController

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.titleView = [self setUpTitleView];

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  // TODO(crbug.com/1450214): Use proper strings.
  self.actionHandler = self;

  self.primaryActionString = @"Continue - TEST";
  self.secondaryActionString = @"No thanks - TEST";

  [super viewDidLoad];

  [self expand:_creditCardData.count];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    // Make sure the GPay logo matches the new trait collection.
    _logoImageView.image = [self googlePayBadgeImage];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [self.delegate disableBottomSheet];
}

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setCreditCardData:
    (NSArray<id<PaymentsSuggestionBottomSheetData>>*)creditCardData {
  _creditCardData = creditCardData;
}

- (void)dismiss {
  [self dismissViewControllerAnimated:NO completion:NULL];
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
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;

  cell.textLabel.text = [self suggestionAtRow:indexPath.row];
  [cell setDetailText:[self descriptionAtRow:indexPath.row]];
  [cell setIconImage:[self iconAtRow:indexPath.row]
            tintColor:nil
      backgroundColor:cell.backgroundColor
         cornerRadius:kCreditCardIconCornerRadius];

  // Make separator invisible on last cell
  CGFloat separatorLeftMargin = [self isLastRow:indexPath]
                                    ? tableView.bounds.size.width
                                    : kTableViewHorizontalSpacing;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorLeftMargin, 0.f, 0.f);

  if ([self selectedRow] == indexPath.row) {
    cell.accessoryType = UITableViewCellAccessoryCheckmark;
  } else {
    cell.accessoryType = UITableViewCellAccessoryNone;
  }
  return cell;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Use payments button
  __weak __typeof(self) weakSelf = self;
  [self dismissViewControllerAnimated:NO
                           completion:^{
                             // Send a notification to fill the
                             // credit card related fields
                             [weakSelf didSelectCreditCard];
                           }];
}

- (void)confirmationAlertSecondaryAction {
  // "No thanks" button, which dismisses the bottom sheet.
  [self dismiss];
}

#pragma mark - Private

// Configures the title view of this ViewController.
- (UIView*)setUpTitleView {
  _logoImageView =
      [[UIImageView alloc] initWithImage:[self googlePayBadgeImage]];
  UILabel* titleLabel = [[UILabel alloc] init];
  // TODO(crbug.com/1450214): Use proper strings.
  titleLabel.text = @"Autofill Payment Info - TEST";
  UIStackView* titleView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _logoImageView, titleLabel ]];
  titleView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  titleView.axis = UILayoutConstraintAxisVertical;
  titleView.alignment = UIStackViewAlignmentCenter;
  titleView.translatesAutoresizingMaskIntoConstraints = NO;
  return titleView;
}

// Returns the google pay badge image corresponding to the current
// UIUserInterfaceStyle (light/dark mode).
- (UIImage*)googlePayBadgeImage {
  // IDR_AUTOFILL_GOOGLE_PAY_DARK only exists in official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
             ? NativeImage(IDR_AUTOFILL_GOOGLE_PAY_DARK)
             : NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
#else
  return NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
#endif
}

// Returns the string to display at a given row in the table view.
- (NSString*)suggestionAtRow:(NSInteger)row {
  const autofill::CreditCard* creditCard = [_creditCardData[row] creditCard];

  return base::SysUTF16ToNSString(creditCard->CardNameAndLastFourDigits());
}

// Returns the display description at a given row in the table view.
- (NSString*)descriptionAtRow:(NSInteger)row {
  const autofill::CreditCard* creditCard = [_creditCardData[row] creditCard];

  return base::SysUTF16ToNSString(creditCard->ExpirationDateForDisplay());
}

// Returns the credit card icon at a given row in the table view.
- (UIImage*)iconAtRow:(NSInteger)row {
  return [_creditCardData[row] icon];
}

// Creates the payments bottom sheet's table view, initially at minimized
// height.
- (UITableView*)createTableView {
  _tableView = [super createTableView];

  [_tableView registerClass:TableViewDetailIconCell.class
      forCellReuseIdentifier:@"cell"];

  _heightConstraint = [_tableView.heightAnchor
      constraintEqualToConstant:_tableView.rowHeight * _creditCardData.count];
  _heightConstraint.active = YES;

  return _tableView;
}

// Notifies the delegate that a credit card was selected by the user.
- (void)didSelectCreditCard {
  [self.delegate
      didSelectCreditCard:[_creditCardData[[self selectedRow]] creditCard]];
}

// Returns whether the provided index path points to the last row of the table
// view.
- (BOOL)isLastRow:(NSIndexPath*)indexPath {
  return NSUInteger(indexPath.row) == (_creditCardData.count - 1);
}

@end

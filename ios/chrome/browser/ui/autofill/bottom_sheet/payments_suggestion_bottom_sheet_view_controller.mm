// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#import "build/branding_buildflags.h"
#import "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PaymentsSuggestionBottomSheetViewController () {
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
  self.primaryActionString = @"Continue - TEST";
  self.secondaryActionString = @"No thanks - TEST";

  [super viewDidLoad];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    // Make sure the GPay logo matches the new trait collection.
    _logoImageView.image = [self googlePayBadgeImage];
  }
}

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setCreditCardData:
    (NSArray<id<PaymentsSuggestionBottomSheetData>>*)creditCardData {
  _creditCardData = creditCardData;
}

- (void)dismiss {
  [self dismissViewControllerAnimated:NO completion:NULL];
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

@end

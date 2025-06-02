// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_constants.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller_delegate.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Stack view insets and spacing.
const CGFloat kBWGConsentMainStackHorizontalInset = 20.0;
const CGFloat kBWGConsentMainStackTopInset = 24.0;
const CGFloat kBWGConsentMainStackSpacing = 5.0;

// Spacing the title labels.
const CGFloat kMainTitleLabelSpacing = 8.0;
const CGFloat kSubTitleLabelSpacing = 12.0;

}  // namespace

@interface BWGPromoViewController () <PromoStyleViewControllerDelegate>
@end

@implementation BWGPromoViewController {
  UIStackView* _mainStackView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.navigationItem.hidesBackButton = YES;
  [self configurePromoStyleProperties];
  [self configureMainStackView];
  [_mainStackView addArrangedSubview:[self createMainTitle]];
  [_mainStackView addArrangedSubview:[self createSubTitle]];
  [super viewDidLoad];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.BWGPromoDelegate promoViewControllerWasDismissed];
}

#pragma mark - Private

// Configure promo style properties to add buttons. Ignores header image type.
- (void)configurePromoStyleProperties {
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_PRIMARY_BUTTON);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_BWG_FIRST_RUN_SECONDARY_BUTTON);
}

// Configure the main stack view.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.distribution = UIStackViewDistributionFill;
  _mainStackView.alignment = UIStackViewAlignmentFill;
  _mainStackView.spacing = kBWGConsentMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_mainStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kBWGConsentMainStackHorizontalInset],
    [_mainStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kBWGConsentMainStackHorizontalInset],
    [_mainStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kBWGConsentMainStackTopInset],
    [_mainStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor]
  ]];
}

// Create a view containing the main title.
- (UIView*)createMainTitle {
  UILabel* mainTitleLabel = [[UILabel alloc] init];
  mainTitleLabel.text = kBWGPromoMainTitleText;
  mainTitleLabel.textAlignment = NSTextAlignmentCenter;
  mainTitleLabel.adjustsFontSizeToFitWidth = YES;
  mainTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  mainTitleLabel.font = PreferredFontForTextStyle(UIFontTextStyleTitle2,
                                                  UIFontWeightSemibold, 22);

  UIView* titleContainer = [[UIView alloc] init];
  titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [titleContainer addSubview:mainTitleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [mainTitleLabel.leadingAnchor
        constraintEqualToAnchor:titleContainer.leadingAnchor
                       constant:kMainTitleLabelSpacing],
    [mainTitleLabel.trailingAnchor
        constraintEqualToAnchor:titleContainer.trailingAnchor
                       constant:-kMainTitleLabelSpacing],
    [mainTitleLabel.topAnchor constraintEqualToAnchor:titleContainer.topAnchor],
    [mainTitleLabel.bottomAnchor
        constraintEqualToAnchor:titleContainer.bottomAnchor],
  ]];
  return titleContainer;
}

// Create a view containing the sub title label.
- (UIView*)createSubTitle {
  UILabel* subTitleLabel = [[UILabel alloc] init];
  subTitleLabel.text = kBWGPromoSubTitleText;
  subTitleLabel.numberOfLines = 2;
  subTitleLabel.textAlignment = NSTextAlignmentCenter;
  subTitleLabel.adjustsFontSizeToFitWidth = YES;
  subTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  subTitleLabel.font = PreferredFontForTextStyle(UIFontTextStyleTitle3,
                                                 UIFontWeightRegular, 15.0);

  UIView* subTitleContainer = [[UIView alloc] init];
  subTitleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [subTitleContainer addSubview:subTitleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [subTitleLabel.leadingAnchor
        constraintEqualToAnchor:subTitleContainer.leadingAnchor
                       constant:kSubTitleLabelSpacing],
    [subTitleLabel.trailingAnchor
        constraintEqualToAnchor:subTitleContainer.trailingAnchor
                       constant:-kSubTitleLabelSpacing]
  ]];
  return subTitleLabel;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.BWGPromoDelegate didAcceptPromo];
}

- (void)didTapSecondaryActionButton {
  [self.mutator didCloseBWGPromo];
}

@end

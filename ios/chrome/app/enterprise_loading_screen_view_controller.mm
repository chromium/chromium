// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/enterprise_loading_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/common/ui/util/dynamic_type_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// All the following values are from "ios/chrome/app/resources/LaunchScreen.xib"
// and should be in sync so that the transition between app launch screen and
// the enterprise launch screen is invisible for the users.
constexpr CGFloat kBottomMargin = 20;
constexpr CGFloat kLogoMultiplier = 0.381966;
constexpr CGFloat kBrandWidth = 107;

constexpr CGFloat kStatusWidth = 195;
constexpr CGFloat kSpacingHeight = 10;
constexpr CGFloat kPaddingHeight = 50;
}  // namespace

@interface EnterpriseLoadScreenViewController ()

// Text displayed during the loading.
@property(nonatomic, strong) UILabel* loadingLabel;

@end

@implementation EnterpriseLoadScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIImageView* logo = [self createLogoView];
  UIImageView* brand = [self createBrandView];
  UIStackView* status = [self createStatusView];

  UIStackView* mainStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ logo, status, brand ]];
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  mainStackView.distribution = UIStackViewDistributionEqualSpacing;
  mainStackView.alignment = UIStackViewAlignmentCenter;

  [self.view addSubview:mainStackView];

  [NSLayoutConstraint activateConstraints:@[
    [logo.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                   multiplier:kLogoMultiplier],
    [logo.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
    [brand.bottomAnchor
        constraintEqualToAnchor:self.view.layoutMarginsGuide.bottomAnchor
                       constant:-kBottomMargin],
    [brand.widthAnchor constraintEqualToConstant:kBrandWidth],
    [status.widthAnchor constraintEqualToConstant:kStatusWidth],
    [mainStackView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor],
    [mainStackView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Limit the size of text to avoid truncation.
  self.loadingLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);
}

#pragma mark - Private

// Creates and configures the logo image.
- (UIImageView*)createLogoView {
  UIImage* logo = [UIImage imageNamed:@"launchscreen_app_logo"];
  UIImageView* logoImageView = [[UIImageView alloc] initWithImage:logo];
  logoImageView.contentMode = UIViewContentModeScaleAspectFit;
  logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
  return logoImageView;
}

// Creates and configures the brand name image.
- (UIImageView*)createBrandView {
  UIImage* brandNameLogo = [UIImage imageNamed:@"launchscreen_brand_name"];
  UIImageView* brandImageView =
      [[UIImageView alloc] initWithImage:brandNameLogo];
  brandImageView.contentMode = UIViewContentModeScaleAspectFit;
  brandImageView.translatesAutoresizingMaskIntoConstraints = NO;
  return brandImageView;
}

// Creates and configures the status view which contains the loading spinner and
// loading text.
- (UIStackView*)createStatusView {
  self.loadingLabel = [[UILabel alloc] init];
  // Chrome's localization utilities aren't available at this stage, so this
  // method uses the native iOS API.
  self.loadingLabel.text =
      NSLocalizedString(@"IDS_IOS_FIRST_RUN_LAUNCH_SCREEN_ENTERPRISE", @"");

  // Limit the size of text to avoid truncation.
  self.loadingLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);

  self.loadingLabel.numberOfLines = 0;
  self.loadingLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.loadingLabel.textAlignment = NSTextAlignmentCenter;

  UIActivityIndicatorView* spinner = [[UIActivityIndicatorView alloc] init];
  [spinner startAnimating];

  UIView* spacing = [[UIView alloc] init];
  spacing.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* bottomPadding = [[UIView alloc] init];
  bottomPadding.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* statusStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        spinner, spacing, self.loadingLabel, bottomPadding
      ]];
  statusStackView.axis = UILayoutConstraintAxisVertical;
  statusStackView.translatesAutoresizingMaskIntoConstraints = NO;
  statusStackView.alignment = UIStackViewAlignmentCenter;
  statusStackView.spacing = UIStackViewSpacingUseSystem;

  [NSLayoutConstraint activateConstraints:@[
    [spacing.heightAnchor constraintEqualToConstant:kSpacingHeight],
    [bottomPadding.heightAnchor constraintEqualToConstant:kPaddingHeight]
  ]];
  return statusStackView;
}

@end

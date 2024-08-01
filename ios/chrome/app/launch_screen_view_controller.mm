// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/launch_screen_view_controller.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// All the following values are from "ios/chrome/app/resources/LaunchScreen.xib"
// and should be in sync so that the transition between app launch screen and
// the launch screen view is invisible for the users.
constexpr CGFloat kBottomMargin = 20;
constexpr CGFloat kLogoMultiplier = 0.381966;
constexpr CGFloat kBrandWidth = 107;
constexpr CGFloat kStatusWidth = 195;
}  // namespace

@interface LaunchScreenViewController ()

// Label displayed during the loading.
@property(nonatomic, strong) UILabel* loadingLabel;

@end

@implementation LaunchScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  UIView* view = self.view;
  view.accessibilityIdentifier =
      first_run::kLaunchScreenAccessibilityIdentifier;

  view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UIImageView* logo = [self createLogoView];
  UIImageView* brand = [self createBrandView];
  NSArray<UIView*>* arrangedSubviews = self.detailView == nil
                                           ? @[ logo, brand ]
                                           : @[ logo, self.detailView, brand ];
  UIStackView* mainStackView =
      [[UIStackView alloc] initWithArrangedSubviews:arrangedSubviews];
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  mainStackView.distribution = UIStackViewDistributionEqualSpacing;
  mainStackView.alignment = UIStackViewAlignmentCenter;

  [view addSubview:mainStackView];

  [NSLayoutConstraint activateConstraints:@[
    [logo.widthAnchor constraintEqualToAnchor:view.widthAnchor
                                   multiplier:kLogoMultiplier],
    [logo.centerYAnchor constraintEqualToAnchor:view.centerYAnchor],
    [brand.bottomAnchor
        constraintEqualToAnchor:view.layoutMarginsGuide.bottomAnchor
                       constant:-kBottomMargin],
    [brand.widthAnchor constraintEqualToConstant:kBrandWidth],
    [mainStackView.widthAnchor constraintEqualToAnchor:view.widthAnchor],
    [mainStackView.centerXAnchor constraintEqualToAnchor:view.centerXAnchor],
  ]];
  if (self.detailView) {
    [self.detailView.widthAnchor constraintEqualToConstant:kStatusWidth]
        .active = YES;
  }
}

#pragma mark - Private

// Creates and configures the logo image view.
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

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/ui/glic_navigation_controller.h"

#import "ios/chrome/browser/intelligence/gemini/ui/glic_consent_mutator.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_constants.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_promo_view_controller.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_promo_view_controller_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

namespace {

// Sheet detents.
const CGFloat kPartialDetentHeight = 500.0;
const CGFloat kFullDetentHeight = 700.0;

// Corner radius.
const CGFloat kPreferredCornerRadius = 16.0;

// Logos size, spacing.
const CGFloat kLogoPointSize = 44.0;
const CGFloat kPromoLogoSpacing = 8.0;
const CGFloat kPromoLogoTopGap = 16.0;
const CGFloat kPromoLogoBottomGap = -16.0;

// Logo names.
constexpr NSString* const kSwiftLogoName = @"swift";
constexpr NSString* const kAppleLogoName = @"applelogo";

}  // namespace

@interface GLICNavigationController () <GLICPromoViewControllerDelegate>

@end

@implementation GLICNavigationController {
  GLICPromoViewController* _promoViewController;
  GLICConsentViewController* _consentViewController;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [self configureNavigationController];
  [super viewDidLoad];
  _promoViewController = [[GLICPromoViewController alloc] init];
  _promoViewController.glicPromoDelegate = self;
  _promoViewController.mutator = self.mutator;
  [self pushViewController:_promoViewController animated:NO];
  [self createLogos];
}

#pragma mark - Private

// Create a custom sheet presentation controller detent with a fixed height.
- (UISheetPresentationControllerDetent*)
    customHeightDetentWithIdentifier:(NSString*)identifier
                              height:(CGFloat)height {
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return height;
  };

  return
      [UISheetPresentationControllerDetent customDetentWithIdentifier:identifier
                                                             resolver:resolver];
}

// Configure the navigation controller.
- (void)configureNavigationController {
  self.sheetPresentationController.detents = @[
    [self customHeightDetentWithIdentifier:
              kGLICPromoConsentPartialDetentIdentifier
                                    height:kPartialDetentHeight],
    [self customHeightDetentWithIdentifier:kGLICPromoConsentFullDetentIdentifier
                                    height:kFullDetentHeight]
  ];

  self.modalPresentationStyle = UIModalPresentationPageSheet;

  self.sheetPresentationController.preferredCornerRadius =
      kPreferredCornerRadius;
  self.sheetPresentationController.prefersScrollingExpandsWhenScrolledToEdge =
      NO;
}

// Create logos and add it to the navigation bar.
- (void)createLogos {
  UIStackView* logosStackView = [[UIStackView alloc] init];
  logosStackView.axis = UILayoutConstraintAxisHorizontal;
  logosStackView.distribution = UIStackViewDistributionFill;
  logosStackView.alignment = UIStackViewAlignmentCenter;
  logosStackView.spacing = kPromoLogoSpacing;
  logosStackView.translatesAutoresizingMaskIntoConstraints = NO;

  // TODO(crbug.com/414777888): Change logo.
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kLogoPointSize
                          weight:UIImageSymbolWeightRegular];

  UIImage* logoStar = DefaultSymbolWithConfiguration(kSwiftLogoName, config);
  UIImageView* logoStarImageView = [[UIImageView alloc] initWithImage:logoStar];
  logoStarImageView.contentMode = UIViewContentModeScaleAspectFit;
  [logosStackView addArrangedSubview:logoStarImageView];

  UIImage* logoBrand = DefaultSymbolWithConfiguration(kAppleLogoName, config);
  UIImageView* logoBrandImageView =
      [[UIImageView alloc] initWithImage:logoBrand];
  logoBrandImageView.contentMode = UIViewContentModeScaleAspectFit;
  [logosStackView addArrangedSubview:logoBrandImageView];

  [self.navigationBar addSubview:logosStackView];

  [NSLayoutConstraint activateConstraints:@[
    [logosStackView.centerXAnchor
        constraintEqualToAnchor:self.navigationBar.centerXAnchor],
    [logosStackView.topAnchor
        constraintEqualToAnchor:self.navigationBar.topAnchor
                       constant:kPromoLogoTopGap],
    [logosStackView.bottomAnchor
        constraintEqualToAnchor:self.navigationBar.bottomAnchor
                       constant:-kPromoLogoBottomGap],
  ]];
}

#pragma mark - GLICPromoViewControllerDelegate

- (void)didAcceptPromo {
  _consentViewController = [[GLICConsentViewController alloc] init];
  _consentViewController.mutator = self.mutator;
  [self pushViewController:_consentViewController animated:YES];
}

@end

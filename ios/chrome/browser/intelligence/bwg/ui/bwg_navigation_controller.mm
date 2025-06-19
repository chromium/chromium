// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_navigation_controller.h"

#import "base/notreached.h"
#import "build/branding_buildflags.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_constants.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

// Corner radius.
const CGFloat kPreferredCornerRadius = 16.0;

// Logos size, spacing.
const CGFloat kLogoPointSize = 58.0;
const CGFloat kLogoTopGap = 32.0;

// Lottie Animation width.
const CGFloat kLottieAnimationContainerWidth = 150.0;

// Logo stack view height.
const CGFloat kLogoStackViewHeight = 62.0;

}  // namespace

@interface BWGNavigationController () <BWGPromoViewControllerDelegate,
                                       UINavigationControllerDelegate>

@end

@implementation BWGNavigationController {
  BWGPromoViewController* _promoViewController;
  BWGConsentViewController* _consentViewController;

  id<LottieAnimation> _logoAnimation;
  // If YES, `_showPromo` will show the promo view. Otherwise, it will skip the
  // promo view.
  BOOL _showPromo;
  // Whether the account is managed.
  BOOL _isAccountManaged;
}

- (instancetype)initWithPromo:(BOOL)showPromo
             isAccountManaged:(BOOL)isAccountManaged {
  self = [super init];
  if (self) {
    _showPromo = showPromo;
    _isAccountManaged = isAccountManaged;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.delegate = self;
  if (!_showPromo) {
    [self createConsentView];
    [self pushViewController:_consentViewController animated:NO];
  } else {
    _promoViewController = [[BWGPromoViewController alloc] init];
    _promoViewController.BWGPromoDelegate = self;
    _promoViewController.mutator = self.mutator;
    _promoViewController.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeAlways;
    [self pushViewController:_promoViewController animated:NO];
  }
  [self createLogos];
  [self configureNavigationController];
}

#pragma mark - Private

// Height of the content of the presented UI.
- (CGFloat)contentHeight {
  [_consentViewController.view layoutIfNeeded];
  if (self.topViewController == _promoViewController) {
    return [_promoViewController contentHeight] +
           self.navigationBar.frame.size.height;
  }
  if (self.topViewController == _consentViewController) {
    return [_consentViewController contentHeight] +
           self.navigationBar.frame.size.height;
  }
  NOTREACHED();
}

// Configures the navigation controller.
- (void)configureNavigationController {
  self.navigationBar.prefersLargeTitles = YES;
  __weak BWGNavigationController* weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf contentHeight];
  };
  UISheetPresentationControllerDetent* detent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kBWGPromoConsentFullDetentIdentifier
                            resolver:resolver];
  self.sheetPresentationController.detents = @[ detent ];

  self.modalInPresentation = NO;
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  self.sheetPresentationController.selectedDetentIdentifier =
      kBWGPromoConsentFullDetentIdentifier;
  self.sheetPresentationController.preferredCornerRadius =
      kPreferredCornerRadius;
  self.sheetPresentationController.prefersScrollingExpandsWhenScrolledToEdge =
      NO;
}

// Creates a stack view to display animated logos in the navigation bar.
- (void)createLogos {
  UIStackView* logosStackView = [[UIStackView alloc] init];
  logosStackView.alignment = UIStackViewAlignmentCenter;
  logosStackView.translatesAutoresizingMaskIntoConstraints = NO;
  logosStackView.layoutMarginsRelativeArrangement = YES;

  UIView* logoBrandContainer =
      [self animatedLogoContainerWithLottie:kLottieAnimationFREBannerName];
  [logosStackView addArrangedSubview:logoBrandContainer];

  [self.navigationBar addSubview:logosStackView];

  [NSLayoutConstraint activateConstraints:@[
    [logosStackView.centerXAnchor
        constraintEqualToAnchor:self.navigationBar.centerXAnchor],
    [logosStackView.topAnchor
        constraintEqualToAnchor:self.navigationBar.topAnchor
                       constant:kLogoTopGap],
    [logosStackView.heightAnchor constraintEqualToConstant:kLogoStackViewHeight]
  ]];
}

// Creates the BWG Consent VC.
- (void)createConsentView {
  _consentViewController = [[BWGConsentViewController alloc]
      initWithIsAccountManaged:_isAccountManaged];
  _consentViewController.mutator = self.mutator;
  _consentViewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeAlways;
}

// Creates a container for a Lottie animation for the logos.
- (UIView*)animatedLogoContainerWithLottie:(NSString*)jsonName {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [container.widthAnchor
        constraintEqualToConstant:kLottieAnimationContainerWidth],
    [container.heightAnchor constraintEqualToConstant:kLogoPointSize],
  ]];

  LottieAnimationConfiguration* configuration =
      [[LottieAnimationConfiguration alloc] init];
  configuration.animationName = jsonName;
  configuration.loopAnimationCount = 1;

  id<LottieAnimation> wrapper =
      ios::provider::GenerateLottieAnimation(configuration);
  wrapper.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.animationView.contentMode = UIViewContentModeScaleAspectFill;
  [wrapper play];
  [container addSubview:wrapper.animationView];
  AddSameConstraints(wrapper.animationView, container);

  UIImageView* logoView = [[UIImageView alloc] init];
  logoView.contentMode = UIViewContentModeScaleAspectFit;
  logoView.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:logoView];

  if (_logoAnimation) {
    _logoAnimation = wrapper;
  }
  return container;
}

#pragma mark - BWGPromoViewControllerDelegate

- (void)didAcceptPromo {
  [self createConsentView];
  [self pushViewController:_consentViewController animated:YES];
  __weak BWGNavigationController* weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf.sheetPresentationController invalidateDetents];
  }];
}

- (void)promoViewControllerWasDismissed {
  [self.BWGNavigationDelegate promoWasDismissed:self];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  [self.sheetPresentationController invalidateDetents];
}

@end

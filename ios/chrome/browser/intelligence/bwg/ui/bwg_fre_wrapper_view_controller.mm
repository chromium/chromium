// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_wrapper_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_promo_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {
// Sheet Presentation corner radius.
const CGFloat kPreferredCornerRadius = 16.0;

// Logos size, spacing.
const CGFloat kLogoPointSize = 70.0;
const CGFloat kLottieAnimationContainerWidth = 150.0;
const CGFloat kLogoTopGap = 32.0;
const CGFloat kExtraSpacingTitleContent = 8.0;

// Transitions.
const CGFloat kAnimationDuration = 1.0;
const CGFloat kDamping = 0.85;

// Spacing for secondary button.
const CGFloat kSpacingAfterSecondaryButton = 32.0;

}  // namespace

@interface BWGFREWrapperViewController () <BWGPromoViewControllerDelegate>

// The main scroll view for the content.
@property(nonatomic, strong) UIScrollView* contentScrollView;

@end

@implementation BWGFREWrapperViewController {
  // The BWG Promo View Controller.
  BWGPromoViewController* _promoViewController;
  // The BWG Consent View Controller.
  BWGConsentViewController* _consentViewController;
  // If YES, `_showPromo` will show the promo view. Otherwise, it will skip the
  // promo view.
  BOOL _showPromo;
  // Whether the account is managed.
  BOOL _isAccountManaged;
  // The main stack view containing the logos.
  UIStackView* _mainStackView;
  // Scroll view that contains the horizontal stack view for transitions.
  // Horizontal stack view holding the promo and consent views.
  UIStackView* _contentHorizontalStackView;
  // Currently active child view controller.
  __weak UIViewController<BWGFREViewControllerProtocol>*
      _currentChildViewController;
  // Stack View containing the logos.
  UIStackView* _logosStackView;
  // The Lottie animation for the logo.
  id<LottieAnimation> _logoAnimation;
  // Content height constraint for the current view.
  NSLayoutConstraint* _contentHeightConstraint;
}

- (instancetype)initWithPromo:(BOOL)showPromo
             isAccountManaged:(BOOL)isAccountManaged {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _showPromo = showPromo;
    _isAccountManaged = isAccountManaged;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  [self setupChildViewControllers];
  [self setupSubviews];
  [self configureSheetPresentation];

  if (_showPromo) {
    _currentChildViewController = _promoViewController;
  } else {
    _currentChildViewController = _consentViewController;
  }

  [self updateAccessibilityVisibility];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak BWGFREWrapperViewController* weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateContentHeightConstraint];
        [weakSelf.view layoutIfNeeded];
        if ([weakSelf isShowingConsentViewAfterPromo]) {
          CGFloat newWidth = weakSelf.contentScrollView.frame.size.width;
          weakSelf.contentScrollView.contentOffset = CGPointMake(newWidth, 0);
        }
        [weakSelf.sheetPresentationController invalidateDetents];
      }
                      completion:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  _contentHeightConstraint = [self.contentScrollView.heightAnchor
      constraintEqualToConstant:[self childContentHeight]];
  _contentHeightConstraint.active = YES;
  [self.sheetPresentationController invalidateDetents];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // The related WebState can be hidden asynchronously while this animated view
  // is being shown. `BWGTabHelper::WasHidden()` causes the related coordinator
  // to shut down, causing the `mutator` to be nil, and leaves the view in a
  // broken state once shown. This check ensures that if the view is in a broken
  // state, automatically dismiss it.
  if (!self.mutator) {
    [self dismissViewControllerAnimated:YES completion:nil];
  }
}

#pragma mark - Private

// Returns YES if the consent view is currently displayed as the second step
// after the promo.
- (BOOL)isShowingConsentViewAfterPromo {
  return _showPromo && (_currentChildViewController == _consentViewController);
}

// Updates the content height constraint.
- (void)updateContentHeightConstraint {
  _contentHeightConstraint.constant = [self childContentHeight];
}

// Returns the child view controller's content height.
- (CGFloat)childContentHeight {
  if (@available(iOS 26, *)) {
    return [_currentChildViewController contentHeight] +
           kSpacingAfterSecondaryButton;
  }
  return [_currentChildViewController contentHeight];
}

// Creates and returns the stack view containing the animated logos.
- (UIStackView*)createLogosStackView {
  UIStackView* logosStackView = [[UIStackView alloc] init];
  logosStackView.alignment = UIStackViewAlignmentCenter;
  logosStackView.translatesAutoresizingMaskIntoConstraints = NO;
  logosStackView.layoutMarginsRelativeArrangement = YES;

  UIView* logoBrandContainer =
      [self animatedLogoContainerWithLottie:kLottieAnimationFREBannerName];
  [logosStackView addArrangedSubview:logoBrandContainer];

  [NSLayoutConstraint
      activateConstraints:@[ [logosStackView.heightAnchor
                              constraintEqualToConstant:kLogoPointSize] ]];

  return logosStackView;
}

// Creates a container view for a Lottie animation with the given JSON name.
- (UIView*)animatedLogoContainerWithLottie:(NSString*)JSONName {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [container.widthAnchor
        constraintEqualToConstant:kLottieAnimationContainerWidth],
    [container.heightAnchor constraintEqualToConstant:kLogoPointSize],
  ]];

  LottieAnimationConfiguration* configuration =
      [[LottieAnimationConfiguration alloc] init];
  configuration.animationName = JSONName;

  id<LottieAnimation> wrapper =
      ios::provider::GenerateLottieAnimation(configuration);
  wrapper.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.animationView.contentMode = UIViewContentModeScaleAspectFit;
  [wrapper play];
  [container addSubview:wrapper.animationView];
  AddSameConstraints(wrapper.animationView, container);

  UIImageView* logoView = [[UIImageView alloc] init];
  logoView.contentMode = UIViewContentModeScaleAspectFit;
  logoView.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:logoView];

  _logoAnimation = wrapper;

  return container;
}

// Constructs the main view hierarchy. The layout consists of a main vertical
// scroll view which holds a stack view `_mainStackView`. This stack view
// contains the top logos and a horizontal, non scrollable content area. The
// content area itself uses a horizontal stack view
// `_contentHorizontalStackView` to place the promo and consent views side by
// side, for a smooth horizontal sliding transition between them.
- (void)setupSubviews {
  UIScrollView* mainScrollView = [[UIScrollView alloc] init];
  mainScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:mainScrollView];
  AddSameConstraints(mainScrollView, self.view.safeAreaLayoutGuide);

  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _mainStackView.layoutMarginsRelativeArrangement = YES;
  _mainStackView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(kLogoTopGap, 0, 0, 0);
  [mainScrollView addSubview:_mainStackView];

  _logosStackView = [self createLogosStackView];
  [_mainStackView addArrangedSubview:_logosStackView];

  self.contentScrollView = [[UIScrollView alloc] init];
  self.contentScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.contentScrollView.showsHorizontalScrollIndicator = NO;
  self.contentScrollView.scrollEnabled = NO;

  _contentHorizontalStackView = [[UIStackView alloc] init];
  _contentHorizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _contentHorizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  _contentHorizontalStackView.distribution = UIStackViewDistributionFillEqually;

  if (_promoViewController) {
    [self addChildViewController:_promoViewController];
    [_contentHorizontalStackView addArrangedSubview:_promoViewController.view];
    [_promoViewController didMoveToParentViewController:self];
  }

  [self addChildViewController:_consentViewController];
  [_contentHorizontalStackView addArrangedSubview:_consentViewController.view];
  [_consentViewController didMoveToParentViewController:self];

  [self.contentScrollView addSubview:_contentHorizontalStackView];

  [_mainStackView setCustomSpacing:kExtraSpacingTitleContent
                         afterView:_logosStackView];
  [_mainStackView addArrangedSubview:self.contentScrollView];

  [NSLayoutConstraint activateConstraints:@[
    // Main vertical stack view constraints.
    [_mainStackView.topAnchor
        constraintEqualToAnchor:mainScrollView.contentLayoutGuide.topAnchor],
    [_mainStackView.leadingAnchor
        constraintEqualToAnchor:mainScrollView.contentLayoutGuide
                                    .leadingAnchor],
    [_mainStackView.trailingAnchor
        constraintEqualToAnchor:mainScrollView.contentLayoutGuide
                                    .trailingAnchor],
    [_mainStackView.widthAnchor
        constraintEqualToAnchor:mainScrollView.frameLayoutGuide.widthAnchor],
    [_mainStackView.bottomAnchor
        constraintEqualToAnchor:mainScrollView.contentLayoutGuide.bottomAnchor],

    // Center the logos stack view within the main stack view.
    [_logosStackView.centerXAnchor
        constraintEqualToAnchor:_mainStackView.centerXAnchor],
    [_contentHorizontalStackView.widthAnchor
        constraintEqualToAnchor:self.contentScrollView.frameLayoutGuide
                                    .widthAnchor
                     multiplier:[self contentStackViewWidthMultiplier]]
  ]];
}

// Returns the width multiplier for the content stack view based on the number
// of pages. The multiplier is 2.0 if showing both promo and consent,
// otherwise 1.0.
- (CGFloat)contentStackViewWidthMultiplier {
  return _showPromo ? 2.0 : 1.0;
}

// Instantiates and configures the child view controllers.
- (void)setupChildViewControllers {
  if (_showPromo) {
    _promoViewController = [[BWGPromoViewController alloc] init];
    _promoViewController.BWGPromoDelegate = self;
    _promoViewController.mutator = self.mutator;
  }

  _consentViewController = [[BWGConsentViewController alloc]
      initWithIsAccountManaged:_isAccountManaged];
  _consentViewController.mutator = self.mutator;
}

// Configures the view controller to be presented as a sheet. It uses a
// custom detent that resolves its height based on the currently visible
// child view controller's content, ensuring the sheet is always perfectly
// sized.
- (void)configureSheetPresentation {
  __weak __typeof(self) weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return [weakSelf contentHeight];
  };
  UISheetPresentationControllerDetent* detent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kBWGPromoConsentFullDetentIdentifier
                            resolver:resolver];
  self.sheetPresentationController.detents = @[ detent ];

  self.modalInPresentation = YES;
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  self.sheetPresentationController.selectedDetentIdentifier =
      kBWGPromoConsentFullDetentIdentifier;
  [self configureCornerRadius];
}

// Configures the correct preferred corner radius given the form factor.
- (void)configureCornerRadius {
  CGFloat preferredCornerRadius =
      IsSplitToolbarMode(self.presentingViewController)
          ? kPreferredCornerRadius
          : UISheetPresentationControllerAutomaticDimension;
  self.navigationController.sheetPresentationController.preferredCornerRadius =
      preferredCornerRadius;
}

// Calculates the total height of the content to be displayed in the sheet.
- (CGFloat)contentHeight {
  CGFloat childContentHeight = [self childContentHeight];
  return childContentHeight + kLogoPointSize + kLogoTopGap +
         kExtraSpacingTitleContent;
}

// Updates VoiceOver focus to the consent view after promo transition.
- (void)updateAccessibilityFocus {
  CHECK(_consentViewController);

  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _consentViewController.view);
}

// Manages which view is visible to VoiceOver.
- (void)updateAccessibilityVisibility {
  if (_promoViewController) {
    _promoViewController.view.accessibilityElementsHidden =
        (_currentChildViewController != _promoViewController);
  }

  _consentViewController.view.accessibilityElementsHidden =
      (_currentChildViewController != _consentViewController);
}

#pragma mark - BWGPromoViewControllerDelegate

// Handles the primary action from the promo screen. It transitions the view
// to the consent screen and animates the content scroll view horizontally.
- (void)didAcceptPromo {
  _currentChildViewController = _consentViewController;
  [self updateAccessibilityVisibility];
  [self updateContentHeightConstraint];

  __weak __typeof(self) weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf.sheetPresentationController invalidateDetents];
  }];

  CGFloat mainStackViewWidth = _mainStackView.frame.size.width;
  [UIView animateWithDuration:kAnimationDuration
      delay:0.0
      usingSpringWithDamping:kDamping
      initialSpringVelocity:0.0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        weakSelf.contentScrollView.contentOffset =
            CGPointMake(mainStackViewWidth, 0);
      }
      completion:^(BOOL finished) {
        if (finished && UIAccessibilityIsVoiceOverRunning()) {
          [weakSelf updateAccessibilityFocus];
        }
      }];
}

@end

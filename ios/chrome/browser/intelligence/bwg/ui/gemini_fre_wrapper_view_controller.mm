// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_fre_wrapper_view_controller.h"

#import <algorithm>

#import "base/check.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_promo_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Sheet Presentation corner radius.
const CGFloat kPreferredCornerRadius = 16.0;

// Logos size, spacing.
const CGFloat kLogoPointSize = 70.0;
const CGFloat kLottieAnimationContainerWidth = 150.0;
const CGFloat kLogoTopGap = 32.0;
const CGFloat kExtraSpacingTitleContent = 8.0;

// Slide in configuration
const CGFloat kSlideDuration = 1.0;
const CGFloat kSpringDamping = 0.85;

// Multipliers for the detent height.
const CGFloat kMaxDetentRatio = 1.0;
const CGFloat kMinDetentRatio = 0.25;

// Adjustment taking into account the inset between the content and buttons.
const CGFloat kInsetAdjustment = 20;

}  // namespace

@interface GeminiFREWrapperViewController () <
    ButtonStackActionDelegate,
    GeminiConsentViewControllerDelegate>

// Scroll view that contains the horizontal stack view for transitions.
@property(nonatomic, strong) UIScrollView* horizontalScrollView;

// Returns the button stack configuration for promo or consent.
+ (ButtonStackConfiguration*)buttonsConfigurationForPromo:(BOOL)promo;

@end

@implementation GeminiFREWrapperViewController {
  // The Gemini Promo View Controller.
  GeminiPromoViewController* _promoViewController;
  // The Gemini Consent View Controller.
  GeminiConsentViewController* _consentViewController;
  // If YES, the promo view is shown initially. Otherwise, we skip it.
  BOOL _showPromo;
  // Whether the account is managed.
  BOOL _isAccountManaged;
  // Type of Gemini FRE.
  GeminiFREType _FREType;
  // The country of the FRE.
  NSString* _country;
  // The main stack view containing the logos.
  UIStackView* _mainStackView;
  // Horizontal stack view holding the promo and consent views.
  UIStackView* _horizontalStackView;
  // Currently active child view controller.
  __weak UIViewController<GeminiFREViewControllerProtocol>*
      _currentChildViewController;
  // Stack View containing the logos.
  UIStackView* _logosStackView;
  // The Lottie animation for the logo.
  id<LottieAnimation> _logoAnimation;
  // Content height constraint for the current view.
  NSLayoutConstraint* _contentHeightConstraint;
  // Whether an accordion item has been expanded at least once.
  BOOL _hasExpandedAccordion;
  // Whether the UI must enforce strict legal consent requirements.
  BOOL _useStrictLegalConsent;
}

- (instancetype)initWithPromo:(BOOL)showPromo
             isAccountManaged:(BOOL)isAccountManaged
        useStrictLegalConsent:(BOOL)useStrictLegalConsent
                      FREType:(GeminiFREType)FREType
                      country:(NSString*)country {
  ButtonStackConfiguration* configuration =
      [GeminiFREWrapperViewController buttonsConfigurationForPromo:showPromo];

  self = [super initWithConfiguration:configuration];
  if (self) {
    _showPromo = showPromo;
    _isAccountManaged = isAccountManaged;
    _useStrictLegalConsent = useStrictLegalConsent;
    _FREType = FREType;
    _country = country;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.actionDelegate = self;

  [self setupChildViewControllers];
  [self setupSubviews];
  [self configureSheetPresentation];
  [self updateButtonConfiguration];
  [self updateAccessibilityVisibility];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateContentHeightConstraint];
      }
                      completion:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self updateContentHeightConstraint];
  [self.view layoutIfNeeded];
  [self.sheetPresentationController invalidateDetents];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (_currentChildViewController == _promoViewController) {
    [self.mutator didShowGeminiPromo];
  }
  // The related WebState can be hidden asynchronously while this animated view
  // is being shown. `GeminiTabHelper::WasHidden()` causes the related
  // coordinator to shut down, causing the `mutator` to be nil, and leaves the
  // view in a broken state once shown. This check ensures that if the view is
  // in a broken state, automatically dismiss it.
  if (!self.mutator) {
    [self dismissViewControllerAnimated:YES completion:nil];
  }
}

#pragma mark - Private

// Updates the content height constraint.
- (void)updateContentHeightConstraint {
  _contentHeightConstraint.constant =
      [_currentChildViewController contentHeight];
  [self.sheetPresentationController invalidateDetents];
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

// Constructs the main view hierarchy. The different steps are contained in a
// horizontal stack and transitioned using a horizontal scrollview.
- (void)setupSubviews {
  UIStackView* wrapperStackView = [[UIStackView alloc] init];
  wrapperStackView.axis = UILayoutConstraintAxisVertical;
  wrapperStackView.layoutMarginsRelativeArrangement = YES;
  wrapperStackView.translatesAutoresizingMaskIntoConstraints = NO;
  wrapperStackView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(kLogoTopGap, 0, 0, 0);
  [self.contentView addSubview:wrapperStackView];
  [self.contentView setContentHuggingPriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  AddSameConstraints(wrapperStackView, self.contentView);

  if (_FREType != GeminiFREType::kLive) {
    _logosStackView = [self createLogosStackView];
    [wrapperStackView addArrangedSubview:_logosStackView];
    [wrapperStackView setCustomSpacing:kExtraSpacingTitleContent
                             afterView:_logosStackView];
  }

  self.horizontalScrollView = [[UIScrollView alloc] init];
  self.horizontalScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.horizontalScrollView.showsHorizontalScrollIndicator = NO;
  self.horizontalScrollView.scrollEnabled = NO;
  [wrapperStackView addArrangedSubview:self.horizontalScrollView];

  _horizontalStackView = [[UIStackView alloc] init];
  _horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  _horizontalStackView.distribution = UIStackViewDistributionFill;
  _horizontalStackView.alignment = UIStackViewAlignmentTop;
  [self.horizontalScrollView addSubview:_horizontalStackView];

  if (_promoViewController) {
    [self addChildViewController:_promoViewController];
    [_horizontalStackView addArrangedSubview:_promoViewController.view];
    [_promoViewController didMoveToParentViewController:self];
    // Force the promo view to be exactly one page wide
    [_promoViewController.view.widthAnchor
        constraintEqualToAnchor:self.horizontalScrollView.frameLayoutGuide
                                    .widthAnchor]
        .active = YES;
    // Hide the other view to prevent invalid height computation.
    _consentViewController.view.hidden = YES;
  }

  [self addChildViewController:_consentViewController];
  [_horizontalStackView addArrangedSubview:_consentViewController.view];
  [_consentViewController didMoveToParentViewController:self];
  // Force the consent view to be exactly one page wide
  [_consentViewController.view.widthAnchor
      constraintEqualToAnchor:self.horizontalScrollView.frameLayoutGuide
                                  .widthAnchor]
      .active = YES;

  _contentHeightConstraint = [self.horizontalScrollView.heightAnchor
      constraintEqualToConstant:[_currentChildViewController contentHeight]];
  _contentHeightConstraint.active = YES;
  AddSameConstraints(_horizontalStackView,
                     self.horizontalScrollView.contentLayoutGuide);
}

// Instantiates and configures the child view controllers.
- (void)setupChildViewControllers {
  if (_showPromo) {
    _promoViewController = [[GeminiPromoViewController alloc] init];
    _promoViewController.mutator = self.mutator;
  }

  _consentViewController = [[GeminiConsentViewController alloc]
      initWithIsAccountManaged:_isAccountManaged
         useStrictLegalConsent:_useStrictLegalConsent
                       FREType:_FREType
                       country:_country];
  _consentViewController.mutator = self.mutator;
  _consentViewController.delegate = self;

  _currentChildViewController =
      _showPromo ? _promoViewController : _consentViewController;
}

// Configures modal presentation settings. We use a custom detent with a height
// based on the visible content while taking into account the scrollview inset.
- (void)configureSheetPresentation {
  __weak __typeof(self) weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return 0;
    }

    CGFloat maxDetentValue = kMaxDetentRatio * context.maximumDetentValue;

    if (strongSelf->_hasExpandedAccordion) {
      return maxDetentValue;
    }

    CGFloat height = [strongSelf preferredHeightForContent];
    // Apply adjustment when needed. super.addsContentViewBottomInset adds
    // insets which lead to an overly generous spacing between the content and
    // the buttons stack for non-scrollable iPhone layouts.
    if (context.containerTraitCollection.userInterfaceIdiom !=
        UIUserInterfaceIdiomPad) {
      height -= kInsetAdjustment;
    }
    CGFloat minDetentValue = kMinDetentRatio * context.maximumDetentValue;
    return std::clamp(height, minDetentValue, maxDetentValue);
  };

  UISheetPresentationControllerDetent* detent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kGeminiPromoConsentFullDetentIdentifier
                            resolver:resolver];
  self.sheetPresentationController.detents = @[ detent ];
  self.modalInPresentation = YES;
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  self.sheetPresentationController.selectedDetentIdentifier = detent.identifier;
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

#pragma mark - ButtonStackActionDelegate

// Handles the primary action from the promo screen. It transitions the view
// to the next step and animates the scroll view horizontally.
- (void)didAcceptPromo {
  _currentChildViewController = _consentViewController;
  _consentViewController.view.hidden = NO;
  [self updateAccessibilityVisibility];
  [self updateButtonConfiguration];

  // Fixed offset on both LTR and RTL languages after setting `hidden = NO`.
  [self.view layoutIfNeeded];
  CGFloat start = _promoViewController.view.frame.origin.x;
  self.horizontalScrollView.contentOffset = CGPointMake(start, 0);

  __weak __typeof(self) weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf updateContentHeightConstraint];
  }];

  GeminiPromoViewController* promoViewController = _promoViewController;
  CGFloat target = _consentViewController.view.frame.origin.x;
  [UIView animateWithDuration:kSlideDuration
      delay:0.0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0.0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        weakSelf.horizontalScrollView.contentOffset = CGPointMake(target, 0);
      }
      completion:^(BOOL finished) {
        promoViewController.view.hidden = YES;
        if (finished && UIAccessibilityIsVoiceOverRunning()) {
          [weakSelf updateAccessibilityFocus];
        }
      }];
}

- (void)didTapPrimaryActionButton {
  if (_currentChildViewController == _promoViewController) {
    RecordFREPromoAction(IOSGeminiFREAction::kAccept);
    [self didAcceptPromo];
  } else if (_currentChildViewController == _consentViewController) {
    RecordFREConsentAction(IOSGeminiFREAction::kAccept);
    if (_FREType == GeminiFREType::kLive) {
      [self.mutator didConsentToLiveGemini];
    } else {
      [self.mutator didConsentGemini];
    }
  }
}

- (void)didTapSecondaryActionButton {
  if (_currentChildViewController == _promoViewController) {
    RecordFREPromoAction(IOSGeminiFREAction::kDismiss);
    [self.mutator didCloseGeminiPromo];
  } else if (_currentChildViewController == _consentViewController) {
    RecordFREConsentAction(IOSGeminiFREAction::kDismiss);
    [self.mutator didRefuseGeminiConsent];
  }
}

- (void)didTapTertiaryActionButton {
  // Not used.
}

// Generates the configuration required by `ButtonStackViewController` for the
// primary & secondary actions. Buttons customization should all happen here.
+ (ButtonStackConfiguration*)buttonsConfigurationForPromo:(BOOL)promo {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  if (promo) {
    configuration.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_BWG_PROMO_PRIMARY_BUTTON);
    configuration.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_BWG_PROMO_SECONDARY_BUTTON);
  } else {
    configuration.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON);
    configuration.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON);
  }
  return configuration;
}

- (void)updateButtonConfiguration {
  BOOL onPromo = _currentChildViewController == _promoViewController;
  ButtonStackConfiguration* configuration =
      [GeminiFREWrapperViewController buttonsConfigurationForPromo:onPromo];
  [self updateConfiguration:configuration];
}

#pragma mark - GeminiConsentViewControllerDelegate

- (void)consentViewControllerDidExpandAccordionItem:
    (GeminiConsentViewController*)viewController {
  _hasExpandedAccordion = YES;
  __weak __typeof(self) weakSelf = self;
  [self.sheetPresentationController animateChanges:^{
    [weakSelf updateContentHeightConstraint];
  }];
}

@end

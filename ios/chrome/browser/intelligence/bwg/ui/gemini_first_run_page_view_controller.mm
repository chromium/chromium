// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_page_view_controller.h"

#import <algorithm>

#import "base/check_op.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
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

// Slide in configuration.
const CGFloat kSlideDuration = 1.0;
const CGFloat kSpringDamping = 0.85;

// Multipliers for the detent height.
const CGFloat kMaxDetentRatio = 1.0;
const CGFloat kMinDetentRatio = 0.25;

// Adjustment taking into account the inset between the content and buttons.
const CGFloat kInsetAdjustment = 20;
}  // namespace

@interface GeminiFirstRunPageViewController () <ButtonStackActionDelegate,
                                                GeminiFirstRunStepDelegate>

@end

@implementation GeminiFirstRunPageViewController {
  // The ordered steps.
  NSArray<UIViewController<GeminiFirstRunStep>*>* _steps;
  // Whether to show the top branding logo.
  BOOL _showBrandingHeader;
  // Scroll view that contains the horizontal stack view for transitions.
  UIScrollView* _horizontalScrollView;
  // Horizontal stack view holding the steps.
  UIStackView* _horizontalStackView;
  // View containing the centered logo.
  UIView* _logoContainerView;
  // The Lottie animation for the logo.
  id<LottieAnimation> _logoAnimation;
  // Content height constraint for the current view.
  NSLayoutConstraint* _contentHeightConstraint;
}

- (instancetype)initWithSteps:
                    (NSArray<UIViewController<GeminiFirstRunStep>*>*)steps
           showBrandingHeader:(BOOL)showBrandingHeader {
  // The container must be initialized with at least one step view controller.
  CHECK_GT(steps.count, 0u);
  // Initialize with an empty configuration; we will update it dynamically per
  // step.
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  self = [super initWithConfiguration:configuration];
  if (self) {
    _steps = [steps copy];
    _showBrandingHeader = showBrandingHeader;
    _currentStep = _steps.firstObject;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.actionDelegate = self;

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

// Re-calculates and updates the sheet's content height constraint after the
// actual screen layout (width) is resolved, ensuring text wrapping is accounted
// for.
- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateContentHeightConstraint];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_logoAnimation play];
  [_currentStep stepDidBecomeActive];
}

#pragma mark - Public

- (void)transitionToNextStepAnimated:(BOOL)animated {
  NSUInteger currentIndex = [_steps indexOfObject:_currentStep];
  if (currentIndex == NSNotFound || currentIndex + 1 >= _steps.count) {
    return;
  }
  UIViewController<GeminiFirstRunStep>* nextStep = _steps[currentIndex + 1];
  [self transitionToStep:nextStep animated:animated];
}

- (void)transitionToStep:(UIViewController<GeminiFirstRunStep>*)step
                animated:(BOOL)animated {
  CHECK([_steps containsObject:step]);
  if (step == _currentStep) {
    return;
  }

  UIViewController<GeminiFirstRunStep>* previousStep = _currentStep;
  _currentStep = step;

  [previousStep stepWillResignActive];
  [_currentStep.view setHidden:NO];
  [self updateAccessibilityVisibility];
  [self updateButtonConfiguration];

  // Set the layout before computing offsets.
  // Fixed offset on both LTR and RTL languages after setting `hidden = NO`.
  [self.view layoutIfNeeded];
  CGFloat start = previousStep.view.frame.origin.x;
  _horizontalScrollView.contentOffset = CGPointMake(start, 0);
  CGFloat target = _currentStep.view.frame.origin.x;

  [self.sheetPresentationController animateChanges:^{
    [self updateContentHeightConstraint];
  }];

  void (^animationsBlock)(void) = ^{
    self->_horizontalScrollView.contentOffset = CGPointMake(target, 0);
  };

  __weak __typeof(self) weakSelf = self;
  void (^completionBlock)(BOOL) = ^(BOOL finished) {
    previousStep.view.hidden = YES;
    [weakSelf.currentStep stepDidBecomeActive];
    if (finished && UIAccessibilityIsVoiceOverRunning()) {
      [weakSelf updateAccessibilityFocus];
    }
  };

  if (animated) {
    [UIView animateWithDuration:kSlideDuration
                          delay:0.0
         usingSpringWithDamping:kSpringDamping
          initialSpringVelocity:0.0
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:animationsBlock
                     completion:completionBlock];
  } else {
    animationsBlock();
    completionBlock(YES);
  }
}

#pragma mark - ButtonStackActionDelegate

// Each step is responsible for handling its own primary action logic (metrics,
// mutators, etc.). The container's only responsibility is to delegate the
// action and transition to the next step when not on the last step.
- (void)didTapPrimaryActionButton {
  BOOL isLastStep = (_currentStep == _steps.lastObject);

  [_currentStep didTapPrimaryButton];

  if (!isLastStep) {
    [self transitionToNextStepAnimated:YES];
  }
}

// Each step is responsible for handling its own secondary action tap, which
// will potentially lead to a dismiss of the first run experience.
- (void)didTapSecondaryActionButton {
  [_currentStep didTapSecondaryButton];
}

- (void)didTapTertiaryActionButton {
  // Not used.
}

- (void)didDismissButtonStackViewController {
  // Not used.
}

- (BOOL)shouldUseFullscreenForStep:(UIViewController<GeminiFirstRunStep>*)step {
  // A step will determine whether it wants to opt into using fullscreen
  // presentation, allowing us to preserve sheet detents unless the step
  // decides otherwise.
  return [step respondsToSelector:@selector(shouldUseFullscreenPresentation)] &&
         [step shouldUseFullscreenPresentation];
}

#pragma mark - GeminiFirstRunStepDelegate

- (void)stepContentHeightDidChange:(UIViewController<GeminiFirstRunStep>*)step {
  if (step == _currentStep) {
    if ([self shouldUseFullscreenForStep:step]) {
      // Sheet is fixed at largeDetent, only update internal scrollable content
      // height constraint.
      [self updateContentHeightConstraint];
    } else {
      [self.sheetPresentationController animateChanges:^{
        [self updateContentHeightConstraint];
      }];
    }
  }
}

#pragma mark - Private

// Updates the content height constraint and invalidates the sheet presentation
// detents if the height has changed.
- (void)updateContentHeightConstraint {
  CGFloat newHeight = [_currentStep contentHeight];
  if (newHeight != _contentHeightConstraint.constant) {
    _contentHeightConstraint.constant = newHeight;
    if (![self shouldUseFullscreenForStep:_currentStep]) {
      // Only invalidate detents when the sheet is not fixed at largeDetent.
      [self.sheetPresentationController invalidateDetents];
    }
  }
}

// Creates and returns the container view holding the centered animated logo.
- (UIView*)createLogoContainerView {
  UIView* logoContainerView = [[UIView alloc] init];
  logoContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* logoBrandContainer =
      [self animatedLogoContainerWithLottie:kLottieAnimationFirstRunBannerName];
  [logoContainerView addSubview:logoBrandContainer];

  [NSLayoutConstraint activateConstraints:@[
    [logoContainerView.heightAnchor constraintEqualToConstant:kLogoPointSize],
    [logoBrandContainer.centerXAnchor
        constraintEqualToAnchor:logoContainerView.centerXAnchor],
    [logoBrandContainer.centerYAnchor
        constraintEqualToAnchor:logoContainerView.centerYAnchor],
  ]];

  return logoContainerView;
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
  [container addSubview:wrapper.animationView];
  AddSameConstraints(wrapper.animationView, container);

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
  CGFloat topMargin = _showBrandingHeader ? kLogoTopGap : 0;
  wrapperStackView.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(topMargin, 0, 0, 0);
  wrapperStackView.accessibilityIdentifier =
      kGeminiFirstRunWrapperStackAccessibilityIdentifier;
  [self.contentView addSubview:wrapperStackView];
  [self.contentView setContentHuggingPriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  AddSameConstraints(wrapperStackView, self.contentView);

  if (_showBrandingHeader) {
    _logoContainerView = [self createLogoContainerView];
    [wrapperStackView addArrangedSubview:_logoContainerView];
    [wrapperStackView setCustomSpacing:kExtraSpacingTitleContent
                             afterView:_logoContainerView];
  }

  _horizontalScrollView = [[UIScrollView alloc] init];
  _horizontalScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _horizontalScrollView.showsHorizontalScrollIndicator = NO;
  _horizontalScrollView.scrollEnabled = NO;
  _horizontalScrollView.clipsToBounds = NO;
  [wrapperStackView addArrangedSubview:_horizontalScrollView];

  _horizontalStackView = [[UIStackView alloc] init];
  _horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _horizontalStackView.axis = UILayoutConstraintAxisHorizontal;
  _horizontalStackView.distribution = UIStackViewDistributionFill;
  _horizontalStackView.alignment = UIStackViewAlignmentTop;
  [_horizontalScrollView addSubview:_horizontalStackView];

  // Add each step into the horizontal scrolling container. Pin each step's
  // width to the scroll view's frame and hide non-active steps so only the
  // current screen contributes to layout height calculation and VoiceOver
  // accessibility.
  for (UIViewController<GeminiFirstRunStep>* step in _steps) {
    [self addChildViewController:step];
    [_horizontalStackView addArrangedSubview:step.view];
    [step didMoveToParentViewController:self];

    // Set step delegates so they can communicate height changes.
    if ([step respondsToSelector:@selector(setStepDelegate:)]) {
      step.stepDelegate = self;
    }

    [step.view.widthAnchor
        constraintEqualToAnchor:_horizontalScrollView.frameLayoutGuide
                                    .widthAnchor]
        .active = YES;

    step.view.hidden = step != _currentStep;
  }

  _contentHeightConstraint = [_horizontalScrollView.heightAnchor
      constraintEqualToConstant:[_currentStep contentHeight]];
  _contentHeightConstraint.active = YES;
  AddSameConstraints(_horizontalStackView,
                     _horizontalScrollView.contentLayoutGuide);
}

// Configures modal presentation settings. We use a custom detent with a height
// based on the visible content while taking into account the scrollview inset.
- (void)configureSheetPresentation {
  self.modalInPresentation = YES;
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  [self configureCornerRadius];

  // Steps opting into fullscreen presentation use a fixed large sheet detent
  // instead of dynamic content-fitting detents.
  if ([self shouldUseFullscreenForStep:_currentStep]) {
    self.sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent largeDetent] ];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return 0;
    }

    CGFloat maxDetentValue = kMaxDetentRatio * context.maximumDetentValue;
    // Use preferred height which calculates total height including logos,
    // content and button stack.
    CGFloat height = [strongSelf preferredHeightForContent];
    // Apply adjustment when needed. `super.addsContentViewBottomInset` adds
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
  self.sheetPresentationController.selectedDetentIdentifier = detent.identifier;
}

// Configures the correct preferred corner radius given the form factor.
- (void)configureCornerRadius {
  CGFloat preferredCornerRadius =
      IsSplitToolbarMode(self.presentingViewController)
          ? kPreferredCornerRadius
          : UISheetPresentationControllerAutomaticDimension;
  self.sheetPresentationController.preferredCornerRadius =
      preferredCornerRadius;
}

// Updates VoiceOver focus to the current view.
- (void)updateAccessibilityFocus {
  if (_currentStep) {
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    _currentStep.view);
  }
}

// Manages which view is visible to VoiceOver.
- (void)updateAccessibilityVisibility {
  for (UIViewController<GeminiFirstRunStep>* step in _steps) {
    step.view.accessibilityElementsHidden = (step != _currentStep);
  }
}

- (void)updateButtonConfiguration {
  if (_currentStep) {
    ButtonStackConfiguration* config = [_currentStep buttonStackConfiguration];
    [self updateConfiguration:config];
  }
}

@end

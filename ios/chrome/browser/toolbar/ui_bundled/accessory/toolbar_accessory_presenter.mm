// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/accessory/toolbar_accessory_presenter.h"

#import "base/i18n/rtl.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/presenters/ui_bundled/contained_presenter_delegate.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/accessory/toolbar_accessory_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kToolbarAccessoryWidthRegularRegular = 375;
const CGFloat kToolbarAccessoryCornerRadiusRegularRegular = 13;
const CGFloat kRegularRegularHorizontalMargin = 5;

// Margin between the beginning of the shadow image and the content being
// shadowed.
const CGFloat kShadowMargin = 196;

// Toolbar Accessory animation drop down duration.
const CGFloat kAnimationDuration = 0.15;
}  // namespace

@interface ToolbarAccessoryPresenter ()

// The `presenting` public property redefined as readwrite.
@property(nonatomic, assign, readwrite, getter=isPresenting) BOOL presenting;

// The view that acts as the background for `presentedView`, redefined as
// readwrite. This is especially important on iPhone, as this view that holds
// everything around the safe area.
@property(nonatomic, strong, readwrite) UIView* backgroundView;

// Layout guide to center the presented view below the safe area layout guide on
// iPhone.
@property(nonatomic, strong) UILayoutGuide* centeringGuide;

// A constraint that constrains any views to their pre-animation positions.
// It should be deactiviated during the presentation animation and replaced with
// a constraint that sets the views to their final position.
@property(nonatomic, strong) NSLayoutConstraint* animationConstraint;

@property(nonatomic, assign) BOOL isIncognito;

@end

@implementation ToolbarAccessoryPresenter {
  /// Whether the accessory is presented above the bottom toolbar.
  BOOL _isPresentedAboveBottomToolbar;

  /// Browser agent to get the omnibox position.
  raw_ptr<OmniboxPositionBrowserAgent> _omniboxPositionBrowserAgent;
}

@synthesize baseViewController = _baseViewController;
@synthesize presentedViewController = _presentedViewController;
@synthesize delegate = _delegate;

#pragma mark - Public

- (instancetype)initWithIsIncognito:(BOOL)isIncognito
        omniboxPositionBrowserAgent:
            (OmniboxPositionBrowserAgent*)omniboxPositionBrowserAgent {
  if ((self = [super init])) {
    _isIncognito = isIncognito;
    _omniboxPositionBrowserAgent = omniboxPositionBrowserAgent;
  }
  return self;
}

- (void)disconnect {
  _omniboxPositionBrowserAgent = nullptr;
}

- (BOOL)isPresentingViewController:(UIViewController*)viewController {
  return self.presenting && self.presentedViewController &&
         self.presentedViewController == viewController;
}

- (void)prepareForPresentation {
  self.presenting = YES;
  self.backgroundView = [self createBackgroundView];
  [self.baseViewController addChildViewController:self.presentedViewController];
  [self.baseViewController.view addSubview:self.backgroundView];

  _isPresentedAboveBottomToolbar = NO;
  if (ShouldShowCompactToolbar(self.baseViewController)) {
    [self prepareForPresentationOnIPhone];
  } else {
    [self prepareForPresentationOnIPad];
  }
}

- (void)presentAnimated:(BOOL)animated {
  if (animated) {
    // Force initial layout before the animation.
    [self.baseViewController.view layoutIfNeeded];
  }

  if (ShouldShowCompactToolbar(self.baseViewController)) {
    [self setupFinalConstraintsOnIPhone];
  } else {
    [self setupFinalConstraintsOnIPad];
  }

  __weak __typeof(self) weakSelf = self;
  auto completion = ^void(BOOL) {
    [weakSelf.presentedViewController
        didMoveToParentViewController:weakSelf.baseViewController];
    if ([weakSelf.delegate
            respondsToSelector:@selector(containedPresenterDidPresent:)]) {
      [weakSelf.delegate containedPresenterDidPresent:weakSelf];
    }
  };

  if (animated) {
    [UIView animateWithDuration:kAnimationDuration
                     animations:^() {
                       [self.baseViewController.view layoutIfNeeded];
                     }
                     completion:completion];
  } else {
    completion(YES);
  }
}

- (void)dismissAnimated:(BOOL)animated {
  [self.presentedViewController willMoveToParentViewController:nil];

  __weak __typeof(self) weakSelf = self;
  auto completion = ^void(BOOL) {
    [weakSelf.presentedViewController.view removeFromSuperview];
    [weakSelf.presentedViewController removeFromParentViewController];
    [weakSelf.backgroundView removeFromSuperview];
    weakSelf.backgroundView = nil;
    weakSelf.presenting = NO;
    if ([weakSelf.delegate
            respondsToSelector:@selector(containedPresenterDidDismiss:)]) {
      [weakSelf.delegate containedPresenterDidDismiss:weakSelf];
    }
  };
  if (animated) {
    void (^animation)();
    // Dismiss iPhone presentation.
    if (ShouldShowCompactToolbar(self.baseViewController)) {
      CGRect oldFrame = self.backgroundView.frame;
      self.backgroundView.layer.anchorPoint =
          CGPointMake(0.5, _isPresentedAboveBottomToolbar ? 1 : 0);
      self.backgroundView.frame = oldFrame;
      CGFloat fadeDirectionModifier = _isPresentedAboveBottomToolbar ? -1 : 1;
      animation = ^{
        self.backgroundView.transform =
            CGAffineTransformMakeScale(1, fadeDirectionModifier * 0.05);
        self.backgroundView.alpha = 0;
      };
    } else {  // Dismiss iPad presentation.
      CGFloat rtlModifier = base::i18n::IsRTL() ? -1 : 1;
      animation = ^{
        self.backgroundView.transform = CGAffineTransformMakeTranslation(
            rtlModifier * self.backgroundView.bounds.size.width, 0);
      };
    }
    [UIView animateWithDuration:kAnimationDuration
                     animations:animation
                     completion:completion];
  } else {
    completion(YES);
  }
}

#pragma mark - Private Helpers

// Positions the view into its initial, pre-animation position on iPhone.
- (void)prepareForPresentationOnIPhone {
  if (_omniboxPositionBrowserAgent) {
    _isPresentedAboveBottomToolbar =
        _omniboxPositionBrowserAgent->IsCurrentLayoutBottomOmnibox();
  }

  if (_isPresentedAboveBottomToolbar) {
    [self prepareForPresentationOnIPhoneBottomToolbar];
  } else {
    [self prepareForPresentationOnIPhoneTopToolbar];
  }
}

// Prepare the view to be presented over the top toolbar on iPhone. The
// pre-animation position the view offscreen above the `baseViewController`.
- (void)prepareForPresentationOnIPhoneTopToolbar {
  self.animationConstraint = [self.backgroundView.bottomAnchor
      constraintEqualToAnchor:self.baseViewController.view.topAnchor];

  // Use this constraint to force the greater than or equal constraint below to
  // be as small as possible.
  NSLayoutConstraint* centeringGuideTopConstraint =
      [self.centeringGuide.topAnchor
          constraintEqualToAnchor:self.backgroundView.topAnchor];
  centeringGuideTopConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    [self.backgroundView.leadingAnchor
        constraintEqualToAnchor:self.baseViewController.view.leadingAnchor],
    [self.backgroundView.trailingAnchor
        constraintEqualToAnchor:self.baseViewController.view.trailingAnchor],
    [self.centeringGuide.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.backgroundView
                                                 .safeAreaLayoutGuide
                                                 .topAnchor],
    [self.centeringGuide.bottomAnchor
        constraintEqualToAnchor:self.backgroundView.bottomAnchor],
    centeringGuideTopConstraint,
    self.animationConstraint,
  ]];
}

// Prepare the view to be presented above the bottom toolbar on iPhone. The
// pre-animation position the view offscreen below the `baseViewController`.
- (void)prepareForPresentationOnIPhoneBottomToolbar {
  self.animationConstraint = [self.backgroundView.topAnchor
      constraintEqualToAnchor:self.baseViewController.view.bottomAnchor];

  // Use this constraint to force the greater than or equal constraint below to
  // be as small as possible.
  NSLayoutConstraint* centeringGuideBottomConstraint =
      [self.centeringGuide.bottomAnchor
          constraintEqualToAnchor:self.backgroundView.bottomAnchor];
  centeringGuideBottomConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    [self.backgroundView.leadingAnchor
        constraintEqualToAnchor:self.baseViewController.view.leadingAnchor],
    [self.backgroundView.trailingAnchor
        constraintEqualToAnchor:self.baseViewController.view.trailingAnchor],
    [self.backgroundView.topAnchor
        constraintEqualToAnchor:self.centeringGuide.topAnchor],
    // Position the presented view above the safe area.
    [self.backgroundView.safeAreaLayoutGuide.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:self.centeringGuide.bottomAnchor],
    centeringGuideBottomConstraint,
    self.animationConstraint,
  ]];
}

// Positions the view into its initial, pre-animation position on iPad.
// Specifically, the final position will be a small accessory just below the
// toolbar on the upper right. The pre-animation position is that, but slid
// offscreen to the right.
- (void)prepareForPresentationOnIPad {
  self.backgroundView.layer.cornerRadius =
      kToolbarAccessoryCornerRadiusRegularRegular;

  UIImageView* shadow =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  shadow.translatesAutoresizingMaskIntoConstraints = NO;
  [self.backgroundView addSubview:shadow];

  // The width should be this value, unless that is too wide for the screen.
  // Using a less than required priority means that the view will attempt to be
  // this width, but use the less than or equal constraint below if the view
  // is too narrow.
  NSLayoutConstraint* widthConstraint = [self.backgroundView.widthAnchor
      constraintEqualToConstant:kToolbarAccessoryWidthRegularRegular];
  widthConstraint.priority = UILayoutPriorityRequired - 1;

  self.animationConstraint = [self.backgroundView.leadingAnchor
      constraintEqualToAnchor:self.baseViewController.view.trailingAnchor];

  [self.baseViewController.view addLayoutGuide:_topToolbarLayoutGuide];
  [NSLayoutConstraint activateConstraints:@[
    // Anchors accessory below the the toolbar.
    [self.backgroundView.topAnchor
        constraintEqualToAnchor:_topToolbarLayoutGuide.bottomAnchor],
    self.animationConstraint,
    widthConstraint,
    [self.backgroundView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.baseViewController.view
                                              .widthAnchor
                                 constant:-2 * kRegularRegularHorizontalMargin],
    [self.backgroundView.heightAnchor
        constraintEqualToConstant:kPrimaryToolbarWithOmniboxHeight],
    [self.centeringGuide.bottomAnchor
        constraintEqualToAnchor:self.backgroundView.bottomAnchor],
  ]];
  // Layouts `shadow` around `self.backgroundView`.
  AddSameConstraintsToSidesWithInsets(
      shadow, self.backgroundView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kBottom |
          LayoutSides::kTrailing,
      {-kShadowMargin, -kShadowMargin, -kShadowMargin, -kShadowMargin});
}

// Sets up the constraints on iPhone such that the view is ready to be animated
// to its final position.
- (void)setupFinalConstraintsOnIPhone {
  if (_isPresentedAboveBottomToolbar) {
    [self setupFinalConstraintsOnIPhoneBottomToolbar];
  } else {
    [self setupFinalConstraintsOnIPhoneTopToolbar];
  }
}

// Sets up the final constraints on iPhone over the top toolbar.
- (void)setupFinalConstraintsOnIPhoneTopToolbar {
  self.animationConstraint.active = NO;
  [self.backgroundView.topAnchor
      constraintEqualToAnchor:self.baseViewController.view.topAnchor]
      .active = YES;

  // Make sure the background doesn't shrink when the toolbar goes to fullscreen
  // mode.
  [self.baseViewController.view addLayoutGuide:_topToolbarLayoutGuide];
  [self.backgroundView.bottomAnchor
      constraintGreaterThanOrEqualToAnchor:_topToolbarLayoutGuide.bottomAnchor]
      .active = YES;
}

// Sets up the final constraints on iPhone above the bottom toolbar.
- (void)setupFinalConstraintsOnIPhoneBottomToolbar {
  self.animationConstraint.active = NO;

  [self.baseViewController.view addLayoutGuide:_bottomToolbarLayoutGuide];
  [NSLayoutConstraint activateConstraints:@[
    // Position the presented view above the bottom toolbar.
    [self.backgroundView.bottomAnchor
        constraintEqualToAnchor:_bottomToolbarLayoutGuide.topAnchor],

    // Position the presented view above the keyboard safe area. This uses the
    // `baseViewController` keyboard layout guide as it's updated sooner than
    // the `backgroundView` rendering a smoother animation.
    [self.baseViewController.view.keyboardLayoutGuide.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.centeringGuide.bottomAnchor],
  ]];
}

// Sets up the constraints on iPhone such that the view is ready to be animated
// to its final position.
- (void)setupFinalConstraintsOnIPad {
  self.animationConstraint.active = NO;
  [self.backgroundView.trailingAnchor
      constraintEqualToAnchor:self.baseViewController.view.trailingAnchor
                     constant:-kRegularRegularHorizontalMargin]
      .active = YES;
}

// Creates the background view and adds `self.presentedViewController.view` to
// it
- (UIView*)createBackgroundView {
  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.accessibilityIdentifier = kToolbarAccessoryContainerViewID;

  backgroundView.overrideUserInterfaceStyle =
      self.isIncognito ? UIUserInterfaceStyleDark
                       : UIUserInterfaceStyleUnspecified;
  backgroundView.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  [backgroundView addSubview:self.presentedViewController.view];

  self.centeringGuide = [[UILayoutGuide alloc] init];
  [backgroundView addLayoutGuide:self.centeringGuide];

  [NSLayoutConstraint activateConstraints:@[
    [self.centeringGuide.trailingAnchor
        constraintEqualToAnchor:backgroundView.trailingAnchor],
    [self.centeringGuide.leadingAnchor
        constraintEqualToAnchor:backgroundView.leadingAnchor],
    [self.centeringGuide.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.presentedViewController.view
                                                 .heightAnchor],
    [self.presentedViewController.view.heightAnchor
        constraintEqualToConstant:kPrimaryToolbarWithOmniboxHeight],
    [self.presentedViewController.view.leadingAnchor
        constraintEqualToAnchor:self.centeringGuide.leadingAnchor],
    [self.presentedViewController.view.trailingAnchor
        constraintEqualToAnchor:self.centeringGuide.trailingAnchor],
    [self.presentedViewController.view.centerYAnchor
        constraintEqualToAnchor:self.centeringGuide.centerYAnchor],
  ]];

  return backgroundView;
}

@end

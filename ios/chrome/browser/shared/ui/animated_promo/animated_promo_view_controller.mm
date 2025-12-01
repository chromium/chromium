// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

// Spacing applied at the top of the alert screen if there is no navigation bar.
constexpr CGFloat kCustomSpacingAtTopIfNoNavigationBar = 32;

// Spacing applied after the image when no animation is shown.
constexpr CGFloat kCustomSpacingAfterImageWithoutAnimation = 0;

// Default spacing applied between elements in the alert screen layout.
constexpr CGFloat kCustomSpacing = 8;

// Offset to raise the alertScreen's top anchor for devices with a regular
// size class.
constexpr CGFloat kCustomTopOffsetForRegularSizeClass = -24;

}  // namespace

@interface AnimatedPromoViewController ()

@end

@implementation AnimatedPromoViewController {
  // Custom animation view used in the full-screen promo.
  id<LottieAnimation> _animationViewWrapper;

  // Custom animation view used in the full-screen promo in dark mode.
  id<LottieAnimation> _animationViewWrapperDarkMode;

  // Child view controller used to display the alert screen for the promo.
  ConfirmationAlertViewController* _alertScreen;

  // TopAnchor constraint for `alertScreen`.
  NSLayoutConstraint* _alertScreenTopAnchorConstraint;
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _useLegacyDarkMode = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];

  alertScreen.titleString = _titleString;
  alertScreen.subtitleString = _subtitleString;
  alertScreen.configuration.primaryActionString = _primaryActionString;
  alertScreen.configuration.secondaryActionString = _secondaryActionString;
  [alertScreen reloadConfiguration];
  alertScreen.actionHandler = _actionHandler;
  alertScreen.shouldFillInformationStack = YES;
  alertScreen.underTitleView = _underTitleView;

  _alertScreen = alertScreen;

  _animationViewWrapper = [self createAnimation:_animationName];

  // Set the text localization.
  [_animationViewWrapper setDictionaryTextProvider:_animationTextProvider];

  if (self.useLegacyDarkMode) {
    _animationViewWrapperDarkMode =
        [self createAnimation:_animationNameDarkMode];
    [_animationViewWrapperDarkMode
        setDictionaryTextProvider:_animationTextProvider];
  }

  if (_animationBackgroundColor) {
    _animationViewWrapper.animationView.backgroundColor =
        _animationBackgroundColor;
    _animationViewWrapperDarkMode.animationView.backgroundColor =
        _animationBackgroundColor;
  }

  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  if (_animationViewWrapper) {
    [self configureAndLayoutAnimationView];
  }
  [self configureAlertScreen];
  [self layoutAlertScreen];

  // Set up UI with current trait first.
  [self updateUIForSizeClass];
  [self updateForDarkMode];

  [self registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                     withAction:@selector(updateUIForSizeClass)];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(updateForDarkMode)];
}

#pragma mark - Private

// Helper to determine if any navigation bar is currently visible.
- (BOOL)isAnyNavigationBarVisible {
  if (self.navigationController.navigationBar &&
      !self.navigationController.navigationBarHidden) {
    return YES;
  }

  return NO;
}

// The offset from center Y to place the divider between the animation and the
// confirmation alert screen.
- (CGFloat)centerYOffset {
  return CanShowTabStrip(_alertScreen) ? kCustomTopOffsetForRegularSizeClass
                                       : 0;
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  return ios::provider::GenerateLottieAnimation(config);
}

// Configures the alertScreen view.
- (void)configureAlertScreen {
  DCHECK(_alertScreen);
  _alertScreen.imageHasFixedSize = YES;
  _alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  _alertScreen.topAlignedLayout = YES;
  _alertScreen.customSpacing = kCustomSpacing;

  CGFloat spacingAfterImage = kCustomSpacing;

  if (![self shouldShowAnimation]) {
    spacingAfterImage = kCustomSpacingAfterImageWithoutAnimation;
  }

  _alertScreen.customSpacingBeforeImageIfNoNavigationBar =
      kCustomSpacingAtTopIfNoNavigationBar;
  _alertScreen.customSpacingAfterImage = spacingAfterImage;

  [self addChildViewController:_alertScreen];
  [self.view addSubview:_alertScreen.view];

  [_alertScreen didMoveToParentViewController:self];
}

// Sets the layout of the alertScreen view.
- (void)layoutAlertScreen {
  if (_animationViewWrapper.animationView) {
    [self layoutAlertScreenForPromoWithAnimation];
  }
}

// Sets the layout of the alertScreen view when the promo will be
// shown with the animation view (full-screen promo).
- (void)layoutAlertScreenForPromoWithAnimation {
  _alertScreen.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_alertScreen.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_alertScreen.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_alertScreen.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
  ]];
  [self updateAlertScreenTopAnchorConstraint];
}

// Updates the top anchor of the alertScreen.
// Called when the screen rotates, or in the initial layout.
- (void)updateAlertScreenTopAnchorConstraint {
  _alertScreenTopAnchorConstraint.active = NO;
  if ([self shouldShowAnimation]) {
    _alertScreenTopAnchorConstraint = [_alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
                       constant:[self centerYOffset]];
  } else {
    _alertScreenTopAnchorConstraint = [_alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor];
  }

  _alertScreenTopAnchorConstraint.active = YES;
}

// Configures the animation view and its constraints.
- (void)configureAndLayoutAnimationView {
  [self configureAndLayoutAnimationViewForWrapper:_animationViewWrapper];
  if (self.useLegacyDarkMode) {
    [self configureAndLayoutAnimationViewForWrapper:
              _animationViewWrapperDarkMode];

    BOOL darkModeEnabled =
        (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

    _animationViewWrapper.animationView.hidden = darkModeEnabled;
    _animationViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
    [self updateAnimationsPlaying];
  } else {
    [_animationViewWrapper play];
  }
}

// Helper method to configure the animation view and its constraints for the
// given LottieAnimation view.
- (void)configureAndLayoutAnimationViewForWrapper:(id<LottieAnimation>)wrapper {
  [self.view addSubview:wrapper.animationView];

  wrapper.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.animationView.contentMode = UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint activateConstraints:@[
    [wrapper.animationView.leftAnchor
        constraintEqualToAnchor:self.view.leftAnchor],
    [wrapper.animationView.rightAnchor
        constraintEqualToAnchor:self.view.rightAnchor],
    [wrapper.animationView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [wrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
                       constant:[self centerYOffset]],
  ]];

  [wrapper play];
}

// Returns YES if the view should display the animation view.
// The animation view should be displayed if `animationViewWrapper` is not null
// and the device is in portrait orientation.
- (BOOL)shouldShowAnimation {
  return _animationViewWrapper.animationView &&
         self.traitCollection.verticalSizeClass !=
             UIUserInterfaceSizeClassCompact;
}

// Checks if the animations are hidden or unhidden and plays (or stops) them
// accordingly.
- (void)updateAnimationsPlaying {
  _animationViewWrapper.animationView.hidden ? [_animationViewWrapper stop]
                                             : [_animationViewWrapper play];
  _animationViewWrapperDarkMode.animationView.hidden
      ? [_animationViewWrapperDarkMode stop]
      : [_animationViewWrapperDarkMode play];
}

// Called when the device is rotated or dark mode is enabled/disabled. (Un)Hide
// the animations accordingly.
- (void)updateUIForSizeClass {
  BOOL hidden = ![self shouldShowAnimation];

  if (self.useLegacyDarkMode) {
    BOOL darkModeEnabled =
        (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

    _animationViewWrapper.animationView.hidden = hidden || darkModeEnabled;
    _animationViewWrapperDarkMode.animationView.hidden =
        hidden || !darkModeEnabled;
  } else {
    _animationViewWrapper.animationView.hidden = hidden;
  }

  [self updateAnimationsPlaying];
  [self updateAlertScreenTopAnchorConstraint];
}

// Updates the animations for the styl used (light/dark mode).
- (void)updateForDarkMode {
  if (self.useLegacyDarkMode) {
    [self updateUIForSizeClass];
    return;
  }
  if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
    [self updateAnimationWithColorProvider:self.darkModeColorProvider];
  } else {
    [self updateAnimationWithColorProvider:self.lightModeColorProvider];
  }
}

// Updates the _animationViewWrapper with colors from `colorProvider`.
- (void)updateAnimationWithColorProvider:
    (NSDictionary<NSString*, UIColor*>*)colorProvider {
  for (NSString* keypath in colorProvider.allKeys) {
    [_animationViewWrapper setColorValue:colorProvider[keypath]
                              forKeypath:keypath];
  }
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/ui/animated_promo_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {
constexpr CGFloat kCustomSpacingAtTopIfNoNavigationBar = 32;
constexpr CGFloat kCustomSpacingAfterImageWithoutAnimation = 0;
constexpr CGFloat kCustomSpacing = 8;
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

#pragma mark - UIViewController

- (void)viewDidLoad {
  ConfirmationAlertViewController* alertScreen =
      [[ConfirmationAlertViewController alloc] init];

  alertScreen.titleString = _titleString;
  alertScreen.subtitleString = _subtitleString;
  alertScreen.primaryActionString = _primaryActionString;
  alertScreen.secondaryActionString = _secondaryActionString;
  alertScreen.actionHandler = _actionHandler;
  alertScreen.shouldFillInformationStack = YES;
  alertScreen.underTitleView = _underTitleView;

  _alertScreen = alertScreen;

  _animationViewWrapper = [self createAnimation:_animationName];
  _animationViewWrapperDarkMode = [self createAnimation:_animationNameDarkMode];

  // Set the text localization.
  [_animationViewWrapper setDictionaryTextProvider:_animationTextProvider];
  [_animationViewWrapperDarkMode
      setDictionaryTextProvider:_animationTextProvider];

  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  if (_animationViewWrapper) {
    [self configureAndLayoutAnimationView];
  }
  [self configureAlertScreen];
  [self layoutAlertScreen];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitUserInterfaceStyle.class, UITraitVerticalSizeClass.class ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateUIOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
// Called when the device is rotated or dark mode is enabled/disabled. (Un)Hide
// the animations accordingly.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - Private

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = 1000;
  return ios::provider::GenerateLottieAnimation(config);
}

// Configures the alertScreen view.
- (void)configureAlertScreen {
  DCHECK(_alertScreen);
  _alertScreen.imageHasFixedSize = YES;
  _alertScreen.showDismissBarButton = NO;
  _alertScreen.titleTextStyle = UIFontTextStyleTitle2;
  _alertScreen.topAlignedLayout = YES;
  _alertScreen.customSpacing = kCustomSpacing;

  if ([self shouldShowAnimation]) {
    _alertScreen.customSpacingBeforeImageIfNoNavigationBar =
        kCustomSpacingAtTopIfNoNavigationBar;
  } else {
    _alertScreen.customSpacingAfterImage =
        kCustomSpacingAfterImageWithoutAnimation;
  }

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
        constraintEqualToAnchor:self.view.centerYAnchor];
  } else {
    _alertScreenTopAnchorConstraint = [_alertScreen.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor];
  }
  _alertScreenTopAnchorConstraint.active = YES;
}

// Configures the animation view and its constraints.
- (void)configureAndLayoutAnimationView {
  [self configureAndLayoutAnimationViewForWrapper:_animationViewWrapper];
  [self
      configureAndLayoutAnimationViewForWrapper:_animationViewWrapperDarkMode];

  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

  _animationViewWrapper.animationView.hidden = darkModeEnabled;
  _animationViewWrapperDarkMode.animationView.hidden = !darkModeEnabled;
  [self updateAnimationsPlaying];
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
        constraintEqualToAnchor:self.view.centerYAnchor],
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
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
  BOOL hidden = ![self shouldShowAnimation];

  _animationViewWrapper.animationView.hidden = hidden || darkModeEnabled;
  _animationViewWrapperDarkMode.animationView.hidden =
      hidden || !darkModeEnabled;

  if (_animationBackgroundColor) {
    _animationViewWrapper.animationView.backgroundColor =
        _animationBackgroundColor;
    _animationViewWrapperDarkMode.animationView.backgroundColor =
        _animationBackgroundColor;
  }

  [self updateAnimationsPlaying];
  [self updateAlertScreenTopAnchorConstraint];
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/animated_lens/ui/animated_lens_promo_view_controller.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Video asset names.
NSString* const kLensTutorialAnimation = @"lens_tutorial_promo";
NSString* const kLensTutorialAnimationDarkomode =
    @"lens_tutorial_promo_darkmode";

// Accessibility IDs.
NSString* const kLensTutorialAnimationViewId = @"LensTutorialAnimationViewId";
NSString* const kLensTutorialAnimationDarkViewId =
    @"LensTutorialAnimationDarkViewId";
}  // namespace

@implementation AnimatedLensPromoViewController {
  // Animation view.
  id<LottieAnimation> _animationViewWrapper;
  // Animation view in dark mode.
  id<LottieAnimation> _animationViewWrapperDarkMode;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kAnimatedLensPromoAccessibilityIdentifier;

  self.preferToCompressContent = YES;
  self.titleTopMarginWhenNoHeaderImage = 30;
  self.subtitleBottomMargin = 40;

  self.titleText = l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_SUBTITLE);
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_START_BROWSING_BUTTON);

  NSArray<UITrait>* traits =
      TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
  [self registerForTraitChanges:traits
                     withAction:@selector(selectAnimationForCurrentStyle)];

  [self createAnimationViews];

  [super viewDidLoad];
}

#pragma mark - Private

// Creates the animation views. Sets `_animationViewWrapper` and
// `_animationViewWrapperDarkMode`.
- (void)createAnimationViews {
  NSString* animationAssetName = kLensTutorialAnimation;
  NSString* animationAssetNameDarkMode = kLensTutorialAnimationDarkomode;

  _animationViewWrapper = [self createAnimation:animationAssetName];
  _animationViewWrapperDarkMode =
      [self createAnimation:animationAssetNameDarkMode];

  _animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _animationViewWrapperDarkMode.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;

  [self selectAnimationForCurrentStyle];
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  return ios::provider::GenerateLottieAnimation(config);
}

// Selects regular or dark mode animation based on the given style.
- (void)selectAnimationForStyle:(UIUserInterfaceStyle)style {
  if (style == UIUserInterfaceStyleDark) {
    _animationViewWrapper.animationView.hidden = YES;
    [_animationViewWrapper stop];
    _animationViewWrapperDarkMode.animationView.hidden = NO;
    [_animationViewWrapperDarkMode play];
    [self.specificContentView
        addSubview:_animationViewWrapperDarkMode.animationView];

    [NSLayoutConstraint activateConstraints:@[
      [_animationViewWrapperDarkMode.animationView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor
                         constant:-12],
      [_animationViewWrapperDarkMode.animationView.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [_animationViewWrapperDarkMode.animationView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor
                         constant:50],
    ]];
  } else {
    _animationViewWrapperDarkMode.animationView.hidden = YES;
    [_animationViewWrapperDarkMode stop];
    _animationViewWrapper.animationView.hidden = NO;
    [_animationViewWrapper play];
    [self.specificContentView addSubview:_animationViewWrapper.animationView];

    [NSLayoutConstraint activateConstraints:@[
      [_animationViewWrapper.animationView.centerXAnchor
          constraintEqualToAnchor:self.specificContentView.centerXAnchor
                         constant:-12],
      [_animationViewWrapper.animationView.topAnchor
          constraintEqualToAnchor:self.specificContentView.topAnchor],
      [_animationViewWrapper.animationView.bottomAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor
                         constant:50],
    ]];

    // Set low compression resistance priority for the animation views to make
    // their height dynamic.
    [_animationViewWrapper.animationView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
    [_animationViewWrapperDarkMode.animationView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
  }
}

// Selects the animation based on current dark mode settings.
- (void)selectAnimationForCurrentStyle {
  [self selectAnimationForStyle:self.traitCollection.userInterfaceStyle];
}
@end

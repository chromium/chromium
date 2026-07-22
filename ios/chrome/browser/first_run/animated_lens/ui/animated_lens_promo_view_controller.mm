// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/animated_lens/ui/animated_lens_promo_view_controller.h"

#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Video asset names.
NSString* const kLensTutorialAnimation = @"search_with_lens_promo";

// Accessibility IDs.
NSString* const kLensTutorialAnimationViewId = @"LensTutorialAnimationViewId";
}  // namespace

@implementation AnimatedLensPromoViewController {
  // Animation view.
  id<LottieAnimation> _animationViewWrapper;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      first_run::kAnimatedLensPromoAccessibilityIdentifier;

  self.preferToCompressContent = NO;
  self.titleTopMarginWhenNoHeaderImage = 30;
  self.subtitleBottomMargin = 60;

  self.titleText = l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_LENS_SUBTITLE);
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_START_BROWSING_BUTTON);

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(selectAnimationForCurrentStyle)];

  [self createAnimationViews];

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(configureAnimationColors)];
  [self configureAnimationColors];

  [super viewDidLoad];
}

#pragma mark - Private

// Creates the animation views. Sets `_animationViewWrapper`
- (void)createAnimationViews {
  NSString* animationAssetName = kLensTutorialAnimation;

  _animationViewWrapper = [self createAnimation:animationAssetName];

  _animationViewWrapper.animationView
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
}

// Selects the animation based on current dark mode settings.
- (void)selectAnimationForCurrentStyle {
  [self selectAnimationForStyle:self.traitCollection.userInterfaceStyle];
}

// Configures the animation with semantic and custom colors.
- (void)configureAnimationColors {
  NSDictionary<NSString*, UIColor*>* lightModeColorProvider =
      SearchWithLensColorProvider(
          /*ntp_background_color=*/0xEDF4FE,
          /*card_background_color=*/0xFFFFFF,
          /*omnibox_background_color=*/0x022771,
          /*lens_icon_background_color=*/0xFFFFFF,
          /*magic_stack_content_color=*/0xE8F0FE,
          /*results_inner_card_color=*/0xEFF4FE);
  NSDictionary<NSString*, UIColor*>* darkModeColorProvider =
      SearchWithLensColorProvider(
          /*ntp_background_color=*/0x35363A,
          /*card_background_color=*/0x3E4042,
          /*omnibox_background_color=*/0x80868B,
          /*lens_icon_background_color=*/0x3E4042,
          /*magic_stack_content_color=*/0x4A4D50,
          /*results_inner_card_color=*/0x35363A);
  for (NSString* key in lightModeColorProvider.allKeys) {
    UIColor* lightColor = lightModeColorProvider[key];
    UIColor* darkColor = darkModeColorProvider[key];

    ConfigureAnimationCustomColor(_animationViewWrapper, key, lightColor,
                                  darkColor);
  }
}

// Returns the color provider for the Search With Lens animation.
NSDictionary<NSString*, UIColor*>* SearchWithLensColorProvider(
    int ntp_background_color,
    int card_background_color,
    int omnibox_background_color,
    int lens_icon_background_color,
    int magic_stack_content_color,
    int results_inner_card_color) {
  return @{
    @"ntp_background_color" : UIColorFromRGB(ntp_background_color),
    @"card_background_color" : UIColorFromRGB(card_background_color),
    @"omnibox_background_color" : UIColorFromRGB(omnibox_background_color),
    @"lens_icon_background_color" : UIColorFromRGB(lens_icon_background_color),
    @"shadow_background_color" : [UIColor colorNamed:kTertiaryBackgroundColor],
    @"omnibox_text_color" : [UIColor colorNamed:kTextSecondaryColor],
    @"magic_stack_content_color" : UIColorFromRGB(magic_stack_content_color),
    @"results_inner_card_color" : UIColorFromRGB(results_inner_card_color),
  };
}

@end

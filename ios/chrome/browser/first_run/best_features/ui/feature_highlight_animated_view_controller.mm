// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/ui/feature_highlight_animated_view_controller.h"

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

@implementation FeatureHighlightAnimatedViewController {
  // The item that the promo is configured to present.
  BestFeaturesItem* _bestFeaturesItem;
  // Animation view.
  id<LottieAnimation> _animationViewWrapper;
}

- (instancetype)initWithFeatureHighlightItem:
    (BestFeaturesItem*)bestFeaturesItem {
  self = [super init];
  if (self) {
    _bestFeaturesItem = bestFeaturesItem;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.shouldHideBanner = YES;
  self.preferToCompressContent = NO;
  self.titleTopMarginWhenNoHeaderImage = 100;
  self.subtitleBottomMargin = 20;

  self.titleText = _bestFeaturesItem.title;
  self.subtitleText = _bestFeaturesItem.subtitle;
  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_BEST_FEATURES_START_BROWSING_BUTTON);

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(updateAnimation)];

  [super viewDidLoad];
  [self createAnimationViews];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_animationViewWrapper play];
}

#pragma mark - Private

- (void)createAnimationViews {
  _animationViewWrapper =
      [self createAnimation:_bestFeaturesItem.animationName];
  [_animationViewWrapper
      setDictionaryTextProvider:_bestFeaturesItem.textProvider];
  _animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;

  [self.specificContentView addSubview:_animationViewWrapper.animationView];

  [NSLayoutConstraint activateConstraints:@[
    [_animationViewWrapper.animationView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [_animationViewWrapper.animationView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [_animationViewWrapper.animationView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [_animationViewWrapper.animationView.heightAnchor
        constraintEqualToAnchor:_animationViewWrapper.animationView.widthAnchor
                     multiplier:1.33],
  ]];

  [_animationViewWrapper.animationView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  [self updateAnimation];
}

- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  return ios::provider::GenerateLottieAnimation(config);
}

// Updates the animation for light/dark mode.
- (void)updateAnimation {
  if (!_bestFeaturesItem.lightModeColorProvider) {
    return;
  }

  for (NSString* key in _bestFeaturesItem.lightModeColorProvider.allKeys) {
    UIColor* lightColor = _bestFeaturesItem.lightModeColorProvider[key];
    UIColor* darkColor = _bestFeaturesItem.darkModeColorProvider[key];
    ConfigureAnimationCustomColor(_animationViewWrapper, key, lightColor,
                                  darkColor);
  }
}

@end

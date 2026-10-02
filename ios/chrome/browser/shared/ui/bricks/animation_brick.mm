// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bricks/animation_brick.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/brick_configurations/animation_brick_configuration.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

// Creates and returns the LottieAnimation wrapper for the `animationAssetName`.
id<LottieAnimation> CreateAnimation(NSString* animationAssetName) {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.shouldLoop = YES;
  return ios::provider::GenerateLottieAnimation(config);
}

}  // namespace

@implementation AnimationBrick {
  // This view's configuration object.
  AnimationBrickConfiguration* _config;
  // Wrapper containing the light mode animation.
  id<LottieAnimation> _wrapper;
  // Wrapper containing the dark mode animation.
  id<LottieAnimation> _darkWrapper;
}

- (instancetype)initWithConfiguration:(AnimationBrickConfiguration*)config {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _config = config;

    _wrapper = CreateAnimation(config.animationName);
    [_wrapper setDictionaryTextProvider:config.animationTextProvider];
    [self configureAndLayoutAnimationViewForWrapper:_wrapper];

    if (config.useLegacyDarkMode) {
      CHECK(config.darkAnimationName);
      _darkWrapper = CreateAnimation(config.darkAnimationName);
      [_darkWrapper setDictionaryTextProvider:config.animationTextProvider];
      [self configureAndLayoutAnimationViewForWrapper:_darkWrapper];
    }

    if (config.animationBackgroundColor) {
      _wrapper.animationView.backgroundColor = config.animationBackgroundColor;
      _darkWrapper.animationView.backgroundColor =
          config.animationBackgroundColor;
    }

    // Set up UI with current trait first.
    [self updateUIForSizeClass];
    [self updateForDarkMode];

    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                       withAction:@selector(updateUIForSizeClass)];

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(updateForDarkMode)];
  }
  return self;
}

#pragma mark - Private

// Returns YES if the view should display the animation view.
- (BOOL)shouldShowAnimation {
  return _wrapper.animationView && self.traitCollection.verticalSizeClass !=
                                       UIUserInterfaceSizeClassCompact;
}

// Checks if the animations are hidden or unhidden and plays (or stops) them
// accordingly.
- (void)updateAnimationsPlaying {
  if (_wrapper.animationView.hidden) {
    [_wrapper stop];
  } else {
    [_wrapper play];
  }

  if (_darkWrapper.animationView.hidden) {
    [_darkWrapper stop];
  } else {
    [_darkWrapper play];
  }
}

// Called when the device is rotated or dark mode is enabled/disabled. (Un)Hide
// the animations accordingly.
- (void)updateUIForSizeClass {
  BOOL hidden = ![self shouldShowAnimation];
  self.hidden = hidden;

  if (_config.useLegacyDarkMode) {
    BOOL darkModeEnabled =
        (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);

    _wrapper.animationView.hidden = hidden || darkModeEnabled;
    _darkWrapper.animationView.hidden = hidden || !darkModeEnabled;
  } else {
    _wrapper.animationView.hidden = hidden;
  }

  [self updateAnimationsPlaying];
}

// Updates the animations for the style used (light/dark mode).
- (void)updateForDarkMode {
  if (_config.useLegacyDarkMode) {
    [self updateUIForSizeClass];
    return;
  }

  for (NSString* key in _config.lightModeColorProvider.allKeys) {
    UIColor* lightColor = _config.lightModeColorProvider[key];
    UIColor* darkColor = _config.darkModeColorProvider[key];
    ConfigureAnimationCustomColor(_wrapper, key, lightColor, darkColor);
  }
}

// Helper method to configure the animation view and its constraints for the
// given LottieAnimation view.
- (void)configureAndLayoutAnimationViewForWrapper:(id<LottieAnimation>)wrapper {
  [self addSubview:wrapper.animationView];

  wrapper.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  wrapper.animationView.contentMode = UIViewContentModeScaleAspectFit;

  AddSameConstraints(wrapper.animationView, self);

  [wrapper play];
}
@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Lottie/Lottie.h>

#import "base/check.h"
#import "ios/chrome/browser/providers/lottie/chromium_lottie_animation.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromiumLottieAnimation {
  LOTAnimationView* _lottieAnimation;
}

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config {
  self = [super init];
  if (self) {
    DCHECK(config);
    DCHECK(config.path);

    _lottieAnimation = [LOTAnimationView
        animationNamed:config.path
              inBundle:config.bundle == nil ? [NSBundle mainBundle]
                                            : config.bundle];
  }
  return self;
}

- (void)play {
  [_lottieAnimation play];
}

- (void)stop {
  [_lottieAnimation stop];
}

- (void)setColorValue:(UIColor*)color forKeypath:(NSString*)keypath {
  LOTKeypath* lot_keypath = [LOTKeypath keypathWithKeys:keypath, nil];
  LOTColorValueCallback* color_callback =
      [LOTColorValueCallback withCGColor:color.CGColor];
  [_lottieAnimation setValueDelegate:color_callback forKeypath:lot_keypath];
}

- (void)setDictionaryTextProvider:
    (NSDictionary<NSString*, NSString*>*)dictionaryTextProvider {
  // Not available for the objc version of Lottie in Chromium.
}

- (BOOL)isAnimationPlaying {
  return _lottieAnimation.isAnimationPlaying;
}

- (UIView*)animationView {
  return _lottieAnimation;
}

@end

namespace ios {
namespace provider {

// Generate an instance of LottieAnimation.
id<LottieAnimation> GenerateLottieAnimation(
    LottieAnimationConfiguration* config) {
  return [[ChromiumLottieAnimation alloc] initWithConfig:config];
}

}  // namespace provider
}  // namespace ios
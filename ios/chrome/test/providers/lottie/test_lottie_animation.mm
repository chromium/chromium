// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

@interface TestLottieAnimationImpl : NSObject <LottieAnimation>

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config;

@end

@implementation TestLottieAnimationImpl {
  LottieAnimationConfiguration* _config;
  BOOL _playing;
  UIView* _animationView;
}

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config {
  self = [super init];
  if (self) {
    _config = config;
    _playing = NO;
    _animationView = [[UIView alloc] init];
  }
  return self;
}

- (void)play {
  _playing = YES;
}

- (void)stop {
  _playing = NO;
}

- (void)pause {
  _playing = NO;
}

// Called to set a color for a key path.
- (void)setColorValue:(UIColor*)color forKeypath:(NSString*)keypath {
}

// Called to set a dictionary text provider.
- (void)setDictionaryTextProvider:
    (NSDictionary<NSString*, NSString*>*)dictionaryTextProvider {
}

// Called to check if the animation is playing.
- (BOOL)isAnimationPlaying {
  return _playing;
}

// Returns the lottie animation view.
- (UIView*)animationView {
  return _animationView;
}

@end

namespace ios {
namespace provider {

// Generate an instance of LottieAnimation.
id<LottieAnimation> GenerateLottieAnimation(
    LottieAnimationConfiguration* config) {
  return [[TestLottieAnimationImpl alloc] initWithConfig:config];
}

}  // namespace provider
}  // namespace ios

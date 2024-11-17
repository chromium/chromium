// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/bundle_locations.h"
#import "build/build_config.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

#if !BUILDFLAG(IS_IOS_MACCATALYST)
#import <Lottie/Lottie.h>
#import "base/check.h"
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)

@interface ChromiumLottieAnimation : NSObject <LottieAnimation>

// Instantiates a LottieAnimationImpl with the given configuration.
//
// @param config The LottieAnimation configuration parameters to use.
// @return An instance of ChromiumLottieAnimation.
- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation ChromiumLottieAnimation {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  LOTAnimationView* _lottieAnimation;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config {
  self = [super init];
  if (self) {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
    DCHECK(config);
    DCHECK(config.animationName);

    _lottieAnimation = [LOTAnimationView
        animationNamed:config.animationName
              inBundle:config.bundle == nil ? base::apple::FrameworkBundle()
                                            : config.bundle];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  }
  return self;
}

- (void)play {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation play];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)stop {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation stop];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)pause {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_lottieAnimation pause];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setColorValue:(UIColor*)color forKeypath:(NSString*)keypath {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  LOTKeypath* lot_keypath = [LOTKeypath keypathWithKeys:keypath, nil];
  LOTColorValueCallback* color_callback =
      [LOTColorValueCallback withCGColor:color.CGColor];
  [_lottieAnimation setValueDelegate:color_callback forKeypath:lot_keypath];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setDictionaryTextProvider:
    (NSDictionary<NSString*, NSString*>*)dictionaryTextProvider {
  // Not available for the objc version of Lottie in Chromium.
}

- (BOOL)isAnimationPlaying {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  return _lottieAnimation.isAnimationPlaying;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  return NO;
}

- (UIView*)animationView {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  return _lottieAnimation;
#else
  return [[UIView alloc] initWithFrame:CGRectZero];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
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

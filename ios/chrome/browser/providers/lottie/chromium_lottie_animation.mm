// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "build/build_config.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

#if !BUILDFLAG(IS_IOS_MACCATALYST)
#import "ios/chrome/browser/providers/lottie/ChromiumLottieAnimation.h"
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)

// Wrapper around ChromiumLottieAnimation that implements LottieAnimation.
@interface ChromiumLottieAnimationWrapper : NSObject <LottieAnimation>

// Instantiates a ChromiumLottieAnimation with the given configuration.
//
// @param config The LottieAnimation configuration parameters to use.
// @return An instance of ChromiumLottieAnimation.
- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

@implementation ChromiumLottieAnimationWrapper {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  ChromiumLottieAnimation* _wrapped;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config {
  if ((self = [super init])) {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
    DCHECK(config);
    DCHECK(config.animationName);

    _wrapped = [[ChromiumLottieAnimation alloc]
        initWithAnimationName:config.animationName
                       bundle:config.bundle
           forceUseMainThread:config.forceUseMainThread];
    _wrapped.shouldLoop = config.shouldLoop;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  }
  return self;
}

- (void)play {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_wrapped play];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)stop {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_wrapped stop];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)pause {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_wrapped pause];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setColorValue:(UIColor*)color forKeypath:(NSString*)keypath {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_wrapped setColorValue:color forKeypath:keypath];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setGradientValue:(NSArray<UIColor*>*)colors
              forKeypath:(NSString*)keypath {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_wrapped setGradientValue:colors forKeypath:keypath];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (void)setDictionaryTextProvider:
    (NSDictionary<NSString*, NSString*>*)dictionary {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  [_wrapped setDictionaryTextProvider:dictionary];
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (BOOL)isAnimationPlaying {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  return _wrapped.isPlaying;
#else
  return NO;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
}

- (UIView*)animationView {
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  return _wrapped.animationView;
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
  return [[ChromiumLottieAnimationWrapper alloc] initWithConfig:config];
}

}  // namespace provider
}  // namespace ios

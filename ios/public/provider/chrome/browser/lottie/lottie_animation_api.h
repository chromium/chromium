// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_API_H_

#import <UIKit/UIKit.h>

@class LottieAnimationConfiguration;

// A wrapper that can facilitate communication with downstream.
@protocol LottieAnimation <NSObject>

// Called to plays the lottie animation.
- (void)play;

// Called to stop the lottie animation.
- (void)stop;

// Called to pause the lottie animation.
- (void)pause;

// Called to set a color for a key path.
- (void)setColorValue:(UIColor*)color forKeypath:(NSString*)keypath;

// Called to set a dictionary text provider.
- (void)setDictionaryTextProvider:
    (NSDictionary<NSString*, NSString*>*)dictionaryTextProvider;

// Called to check if the animation is playing.
- (BOOL)isAnimationPlaying;

// Returns the lottie animation view.
- (UIView*)animationView;

@end

namespace ios {
namespace provider {

// Generate an instance of LottieAnimation.
id<LottieAnimation> GenerateLottieAnimation(
    LottieAnimationConfiguration* config);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_API_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_LOTTIE_CHROMIUM_LOTTIE_ANIMATION_H_
#define IOS_CHROME_BROWSER_PROVIDERS_LOTTIE_CHROMIUM_LOTTIE_ANIMATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"

@class LottieAnimationConfiguration;

@interface ChromiumLottieAnimation : NSObject <LottieAnimation>

// Instantiates a LottieAnimationImpl with the given configuration.
//
// @param config The LottieAnimation configuration parameters to use.
// @return An instance of ChromiumLottieAnimation.
- (instancetype)initWithConfig:(LottieAnimationConfiguration*)config;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PROVIDERS_LOTTIE_CHROMIUM_LOTTIE_ANIMATION_H_
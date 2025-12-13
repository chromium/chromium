// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration parameters for LottieAnimation.
@interface LottieAnimationConfiguration : NSObject

// Name of the animation file.
@property(nonatomic, copy) NSString* animationName;
// The bundle in which the animation is located.
@property(nonatomic, strong) NSBundle* bundle;
// Whether the animation should loop or not. Default is NO.
@property(nonatomic, assign) BOOL shouldLoop;
// Whether the main thread should be forced-used to render the animation. This
// has negatif performance impact on the animation but is required to use some
// programmatic features (like the gradient). Default is NO.
@property(nonatomic, assign) BOOL forceUseMainThread;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_CONFIGURATION_H_

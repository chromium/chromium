// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration parameters for LottieAnimation.
@interface LottieAnimationConfiguration : NSObject

// Path to the json animation file.
@property(nonatomic, copy) NSString* path;
// Name of the animation file.
@property(nonatomic, copy) NSString* animationName;
// Subdirectory to the json animation file.
@property(nonatomic, copy) NSString* subdirectory;
// The bundle in which the animation is located.
@property(nonatomic, strong) NSBundle* bundle;
// The loop behavior of the animation.
@property(nonatomic, assign) CGFloat loopAnimationCount;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LOTTIE_LOTTIE_ANIMATION_CONFIGURATION_H_

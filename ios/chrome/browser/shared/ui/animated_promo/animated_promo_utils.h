// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_UTILS_H_

#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"

// Configures the animation with the given semantic color.
// Sets the color value for the keypath `**key.**.Color`.
void ConfigureAnimationSemanticColor(id<LottieAnimation> animation,
                                     NSString* key,
                                     NSString* color_name);

// Configures the animation with a custom color for a specific key.
// Set the color value for the keypath `**KEY.**.Color` based on the
// `UIUserInterfaceStyle`.
void ConfigureAnimationCustomColor(id<LottieAnimation> animation,
                                   NSString* key,
                                   UIColor* light_color,
                                   UIColor* dark_color);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_UTILS_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_GRADIENT_LAYER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_GRADIENT_LAYER_H_

#import <UIKit/UIKit.h>

enum class GradientLayerType { kLinear, kEaseInOut, kEaseInThenLinear };

// A version of CAGradient that can have different "curves" of gradient. Do not
// use -setColors: when using this gradient, use -setStartColor:endColor:
// instead.
@interface GradientLayer : CAGradientLayer

// Decide of the type of gradient. Default is kLinear. Need to be set before
// calling -setStartColor:endColor:.
@property(nonatomic, assign) GradientLayerType gradientType;

// Sets the start and end colors for this gradient. To be used instead of
// -setColor:.
- (void)setStartColor:(UIColor*)startColor endColor:(UIColor*)endColor;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_GRADIENT_LAYER_H_

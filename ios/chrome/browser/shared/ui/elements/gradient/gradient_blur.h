// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_GRADIENT_BLUR_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_GRADIENT_BLUR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_layer.h"

// WARNING: This might not behave as you expect it to. The blur effect is not
// using the gradient. The gradient is mostly used for the scrim effect. The
// blur will start when the gradient ends. It will not be as a hard cut as a
// non-gradient blur, but it will not be a nice blur. This is a limitation of
// UIKit.
@interface GradientBlur : UIVisualEffectView

// The gradient is mostly used for the scrim effect. The blur will start when
// the gradient ends. The blur amount can be modulated with the
// `effectPercentage`.
- (instancetype)initWithEffect:(UIVisualEffect*)effect
              effectPercentage:(CGFloat)effectPercentage
                    startPoint:(CGPoint)startPoint
                      endPoint:(CGPoint)endPoint
                  gradientType:(GradientLayerType)gradientType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithEffect:(UIVisualEffect*)effect NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_GRADIENT_BLUR_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_BLUR_VIEW_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_BLUR_VIEW_H_

#import <UIKit/UIKit.h>

// A UIVisualEffectView subclass that provides continuous, animatable control
// over blur intensity for App Bar buttons during fullscreen transitions using
// a fraction of the underlying UIBlurEffect.
@interface AppBarBlurView : UIVisualEffectView

// The normalized blur amount between 0.0 (no blur) and 1.0 (maximum blur
// defined by `maxBlurFraction`). When set to 0.0, or upon completing an
// animation reaching 1.0, the effect is cleared and the underlying animator is
// torn down. If modified inside a `[UIView animate...]` block, the blur
// transition will animate smoothly using a CADisplayLink.
@property(nonatomic, assign) CGFloat blurAmount;

// Designated initializer. Creates a blur view with the specified `style` and
// `maxBlurFraction`.
- (instancetype)initWithEffectStyle:(UIBlurEffectStyle)style
                    maxBlurFraction:(CGFloat)maxBlurFraction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithEffect:(UIVisualEffect*)effect NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_BLUR_VIEW_H_

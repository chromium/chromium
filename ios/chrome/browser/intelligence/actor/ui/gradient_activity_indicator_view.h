// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_GRADIENT_ACTIVITY_INDICATOR_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_GRADIENT_ACTIVITY_INDICATOR_VIEW_H_

#import <UIKit/UIKit.h>

// A circular activity indicator that animates with a variable-speed rotation
// and a multi-color conic gradient.
@interface GradientActivityIndicatorView : UIView

// The ordered colors rendered along the circular conic gradient.
// Defaults to a palette of Gemini-themed colors.
@property(nonatomic, copy) NSArray<UIColor*>* gradientColors;

// Starts the circular gradient animation.
- (void)startAnimating;

// Stops the animation and hides the indicator.
- (void)stopAnimating;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_GRADIENT_ACTIVITY_INDICATOR_VIEW_H_

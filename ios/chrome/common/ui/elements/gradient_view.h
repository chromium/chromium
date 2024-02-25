// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_GRADIENT_VIEW_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_GRADIENT_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// A UIView showing an axial gradient with two colors.
@interface GradientView : UIView

// Initializes the view with the a gradient starting at `startPoint` and ending
// at 'endPoint'. `startPoint` and `endPoint` are defined in the unit coordinate
// space ([0, 1]) and then mapped to the view's bounds rectangle when drawn.
- (instancetype)initWithStartColor:(UIColor*)startColor
                          endColor:(UIColor*)endColor
                        startPoint:(CGPoint)startPoint
                          endPoint:(CGPoint)endPoint NS_DESIGNATED_INITIALIZER;

// Initializes the view with a vertical gradient.
- (instancetype)initWithTopColor:(UIColor*)topColor
                     bottomColor:(UIColor*)bottomColor;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Updates the colors used in the gradient.
- (void)setStartColor:(UIColor*)startColor endColor:(UIColor*)endColor;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_GRADIENT_VIEW_H_

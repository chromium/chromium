// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_MULTI_COLOR_GRADIENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_MULTI_COLOR_GRADIENT_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// A multi color gradient supporting two or more colors with custom stop
// locations.
@interface MultiColorGradientView : UIView

// Initializes the view with the a gradient starting at `startPoint` and ending
// at 'endPoint'. `startPoint` and `endPoint` are defined in the unit coordinate
// space ([0, 1]) and then mapped to the view's bounds rectangle when drawn.
// `colors` defines the array of colors for each gradient stop, and `locations`
// defines the relative position (0 to 1) of each stop.
- (instancetype)initWithColors:(NSArray<UIColor*>*)colors
                     locations:(NSArray<NSNumber*>*)locations
                    startPoint:(CGPoint)startPoint
                      endPoint:(CGPoint)endPoint;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Updates the gradient with the provided colors.
- (void)updateColors:(NSArray<UIColor*>*)colors;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_GRADIENT_MULTI_COLOR_GRADIENT_VIEW_H_

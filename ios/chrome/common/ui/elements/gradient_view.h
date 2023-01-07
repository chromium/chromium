// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_GRADIENT_VIEW_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_GRADIENT_VIEW_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// A UIView showing a vertical gradient from transparent to opaque using the
// system background color.
@interface GradientView : UIView

- (instancetype)initWithTopColor:(UIColor*)topColor
                     bottomColor:(UIColor*)bottomColor
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_GRADIENT_VIEW_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_MATERIAL_TIMING_H_
#define IOS_CHROME_COMMON_MATERIAL_TIMING_H_

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

extern const CGFloat kMaterialDuration0;
extern const CGFloat kMaterialDuration1;
extern const CGFloat kMaterialDuration2;
extern const CGFloat kMaterialDuration3;
extern const CGFloat kMaterialDuration4;
extern const CGFloat kMaterialDuration5;
extern const CGFloat kMaterialDuration6;
extern const CGFloat kMaterialDuration7;
extern const CGFloat kMaterialDuration8;

// Type of material timing curve.
typedef NS_ENUM(NSUInteger, MaterialCurve) {
  MaterialCurveEaseInOut,
  MaterialCurveEaseOut,
  MaterialCurveEaseIn,
  MaterialCurveLinear,
};

// Per material spec, a motion curve with "follow through".
CAMediaTimingFunction* MaterialTransformCurve2();

// Returns a timing function related to the given `curve`.
CAMediaTimingFunction* MaterialTimingFunction(MaterialCurve curve);

@interface UIView (CrMaterialAnimations)

// Performs a standard UIView animation using a material timing `curve`.
// Note: any curve option specified in `options` will be ignored in favor of the
// specified curve value.
// See also: +[UIView animateWithDuration:delay:animations:completion].
+ (void)cr_animateWithDuration:(NSTimeInterval)duration
                         delay:(NSTimeInterval)delay
                 materialCurve:(MaterialCurve)materialCurve
                       options:(UIViewAnimationOptions)options
                    animations:(void (^)(void))animations
                    completion:(void (^)(BOOL finished))completion;

@end

#endif  // IOS_CHROME_COMMON_MATERIAL_TIMING_H_

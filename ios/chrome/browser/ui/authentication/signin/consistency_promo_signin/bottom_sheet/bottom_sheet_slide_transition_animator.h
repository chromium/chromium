// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_BOTTOM_SHEET_BOTTOM_SHEET_SLIDE_TRANSITION_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_BOTTOM_SHEET_BOTTOM_SHEET_SLIDE_TRANSITION_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class BottomSheetNavigationController;

// Enum to choose the animation in BottomSheetSlideTransitionAnimator.
typedef NS_ENUM(NSUInteger, BottomSheetSlideAnimation) {
  // Animation to pop a view controller.
  BottomSheetSlideAnimationPopping,
  // Animation to push a view controller.
  BottomSheetSlideAnimationPushing,
};

// Animator for BottomSheetNavigationController. The animation slides the pushed
// or popped views, next to each other.
@interface BottomSheetSlideTransitionAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// BottomSheetNavigationController view controller.
@property(nonatomic, weak)
    BottomSheetNavigationController* navigationController;

// Initialiser.
- (instancetype)initWithAnimation:(BottomSheetSlideAnimation)animation
             navigationController:
                 (BottomSheetNavigationController*)navigationController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_BOTTOM_SHEET_BOTTOM_SHEET_SLIDE_TRANSITION_ANIMATOR_H_

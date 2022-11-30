// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_SLIDE_TRANSITION_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_SLIDE_TRANSITION_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class ConsistencySheetNavigationController;

// Enum to choose the animation in ConsistencySheetSlideTransitionAnimator.
typedef NS_ENUM(NSUInteger, ConsistencySheetSlideAnimation) {
  // Animation to pop a view controller.
  ConsistencySheetSlideAnimationPopping,
  // Animation to push a view controller.
  ConsistencySheetSlideAnimationPushing,
};

// Animator for ConsistencySheetNavigationController. The animation slides the
// pushed or popped views, next to each other.
@interface ConsistencySheetSlideTransitionAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// ConsistencySheetNavigationController view controller.
@property(nonatomic, weak)
    ConsistencySheetNavigationController* navigationController;

// Initialiser.
- (instancetype)initWithAnimation:(ConsistencySheetSlideAnimation)animation
             navigationController:
                 (ConsistencySheetNavigationController*)navigationController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SHEET_CONSISTENCY_SHEET_SLIDE_TRANSITION_ANIMATOR_H_

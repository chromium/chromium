// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_SLIDE_TRANSITION_ANIMATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_SLIDE_TRANSITION_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class AccountPickerScreenNavigationController;

// Enum to choose the animation in AccountPickerScreenSlideTransitionAnimator.
enum AccountPickerScreenSlideAnimation {
  // Animation to pop a view controller.
  kAccountPickerScreenSlideAnimationPopping,
  // Animation to push a view controller.
  kAccountPickerScreenSlideAnimationPushing,
};

// Animator for AccountPickerScreenNavigationController. The animation slides
// the pushed or popped views, next to each other.
@interface AccountPickerScreenSlideTransitionAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// AccountPickerScreenNavigationController view controller.
@property(nonatomic, weak)
    AccountPickerScreenNavigationController* navigationController;

// Initialiser.
- (instancetype)initWithAnimation:(AccountPickerScreenSlideAnimation)animation
             navigationController:
                 (AccountPickerScreenNavigationController*)navigationController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SCREEN_ACCOUNT_PICKER_SCREEN_SLIDE_TRANSITION_ANIMATOR_H_

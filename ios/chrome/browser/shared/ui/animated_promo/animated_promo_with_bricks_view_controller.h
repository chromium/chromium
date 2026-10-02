// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_WITH_BRICKS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_WITH_BRICKS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

@class AnimatedPromoConfiguration;

// A container view controller for a full-screen promo featuring a Lottie
// animation. In a regular vertical size class, the top section plays the
// animation, followed below by a title, a subtitle, and a customizable view,
// all hosted within a `ButtonStackViewController`. The animation is hidden on
// compact vertical layout.
@interface AnimatedPromoWithBricksViewController : ButtonStackViewController

- (instancetype)initWithConfiguration:(AnimatedPromoConfiguration*)config
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ANIMATED_PROMO_ANIMATED_PROMO_WITH_BRICKS_VIEW_CONTROLLER_H_

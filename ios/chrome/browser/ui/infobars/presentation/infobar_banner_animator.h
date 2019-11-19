// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator used to present an InfobarBanner dropping from the top of the
// screen.
@interface InfobarBannerAnimator
    : NSObject <UIViewControllerAnimatedTransitioning,
                UIViewControllerInteractiveTransitioning>

// YES if this animator is presenting a view controller, NO if it is dismissing
// one.
@property(nonatomic, assign) BOOL presenting;

// The UIViewPropertyAnimator animating the presentation/dismissal.
@property(nonatomic, strong) UIViewPropertyAnimator* propertyAnimator;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_ANIMATOR_H_

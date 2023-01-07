// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_EXPAND_BANNER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_EXPAND_BANNER_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator used to animate the expansion of an InfobarBanner into an
// InfobarModal.
@interface InfobarExpandBannerAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// YES if this animator is presenting a view controller, NO if it is dismissing
// one.
@property(nonatomic, assign) BOOL presenting;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_EXPAND_BANNER_ANIMATOR_H_

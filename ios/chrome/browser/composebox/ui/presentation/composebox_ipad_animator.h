// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_ANIMATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class LayoutGuideCenter;

// Animator for the composebox presentation on iPad.
@interface ComposeboxiPadAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// The layout guide center to use for the animation.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Whether the animator is presenting or dismissing.
@property(nonatomic, assign) BOOL presenting;

// YES if the animator should position its container according to a larger
// layout.
@property(nonatomic, assign) BOOL shouldUseLargeLayout;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_ANIMATOR_H_

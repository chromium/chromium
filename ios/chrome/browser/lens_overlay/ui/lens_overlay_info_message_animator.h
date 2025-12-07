// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_INFO_MESSAGE_ANIMATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_INFO_MESSAGE_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator responsible for handling the hiding and showing fading transition of
// the informational message views.
@interface LensOverlayInfoMessageAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

- (instancetype)initWithOperation:(UINavigationControllerOperation)operation;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_INFO_MESSAGE_ANIMATOR_H_

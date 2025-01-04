// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_VIEW_FINDER_TRANSITION_MANAGER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_VIEW_FINDER_TRANSITION_MANAGER_H_

#import <UIKit/UIKit.h>

// Supported transition for LVF presentation.
typedef NS_ENUM(NSInteger, LensViewFinderTransition) {
  LensViewFinderTransitionSlideFromLeft = 0,
  LensViewFinderTransitionSlideFromRight,
};

@interface LensViewFinderTransitionManager
    : NSObject <UIViewControllerTransitioningDelegate,
                UIViewControllerAnimatedTransitioning>

- (instancetype)initWithLVFTransitionType:
    (LensViewFinderTransition)transitionType;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_VIEW_FINDER_TRANSITION_MANAGER_H_

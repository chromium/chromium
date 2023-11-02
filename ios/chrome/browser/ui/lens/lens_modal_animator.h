// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_LENS_MODAL_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_LENS_LENS_MODAL_ANIMATOR_H_

#import <UIKit/UIKit.h>

@interface LensModalAnimator : NSObject <UIViewControllerTransitioningDelegate,
                                         UIViewControllerAnimatedTransitioning>

@end

#endif  // IOS_CHROME_BROWSER_UI_LENS_LENS_MODAL_ANIMATOR_H_

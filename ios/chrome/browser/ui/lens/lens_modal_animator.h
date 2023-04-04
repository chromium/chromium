// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_LENS_MODAL_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_LENS_LENS_MODAL_ANIMATOR_H_

#import <UIKit/UIKit.h>
#import "base/ios/block_types.h"

enum class LensInputSelectionPresentationStyle;

@interface LensModalAnimator : NSObject <UIViewControllerTransitioningDelegate,
                                         UIViewControllerAnimatedTransitioning>

// The presentation style.
@property(nonatomic, assign)
    LensInputSelectionPresentationStyle presentationStyle;

// The completion block.
@property(nonatomic, strong) ProceduralBlock presentationCompletion;

@end

#endif  // IOS_CHROME_BROWSER_UI_LENS_LENS_MODAL_ANIMATOR_H_

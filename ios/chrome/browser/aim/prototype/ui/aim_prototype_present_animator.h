// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_PRESENT_ANIMATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_PRESENT_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"

// Animator for the custom presentation of the AIM prototype.
@interface AIMPrototypePresentAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

/// Whether AIM is toggled on during the presentation.
@property(nonatomic, assign) BOOL toggleOnAIM;

- (instancetype)initWithContextProvider:
    (id<AIMPrototypeAnimationContextProvider>)contextProvider;
@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_PRESENT_ANIMATOR_H_

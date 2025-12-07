// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_DISMISS_ANIMATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_DISMISS_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_animation_base.h"
#import "ios/chrome/browser/composebox/ui/composebox_animation_context.h"

// Animator for the custom dismissal of the composebox.
@interface ComposeboxDismissAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

- (instancetype)
    initWithContextProvider:(id<ComposeboxAnimationContext>)contextProvider
              animationBase:(id<ComposeboxAnimationBase>)animationBase;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_DISMISS_ANIMATOR_H_

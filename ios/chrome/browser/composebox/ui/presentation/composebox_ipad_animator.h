// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_ANIMATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_ANIMATOR_H_

#import <UIKit/UIKit.h>

@class LayoutGuideCenter;
enum class ComposeboxMode;

// Delegate for animation changes to the composebox.
@protocol ComposeboxiPadAnimatorDelegate

// Indicates to the delegate to update the Composebox `mode`.
- (void)setComposeboxMode:(ComposeboxMode)mode;

@end

// Animator for the composebox presentation on iPad.
@interface ComposeboxiPadAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

// The layout guide center to use for the animation.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Whether the animator is presenting or dismissing.
@property(nonatomic, assign) BOOL presenting;


// YES if AI mode should be immediately turned on during the presentation
// animation.
@property(nonatomic, assign) BOOL showAIMode;

// Delegate for this animator.
@property(nonatomic, weak) id<ComposeboxiPadAnimatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_ANIMATOR_H_

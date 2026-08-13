// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_VIEWPORT_INSETS_ANIMATOR_H_
#define IOS_WEB_WEB_STATE_UI_CRW_VIEWPORT_INSETS_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

@protocol CRWViewportController;

// Ephemeral helper object that drives frame-by-frame interpolation of
// `obscuredContentInsets` and scroll view `contentInset` using a CADisplayLink.
// Instances of this class are intended to exist only for the duration of an
// active animation.
@interface CRWViewportInsetsAnimator : NSObject

// Initializes the animator with the target web view, scroll view, start and
// target insets, duration, and completion callback.
- (instancetype)initWithWebView:(UIView<CRWViewportController>*)webView
                     scrollView:(UIScrollView*)scrollView
                    startInsets:(UIEdgeInsets)startInsets
                   targetInsets:(UIEdgeInsets)targetInsets
                       duration:(NSTimeInterval)duration
                     completion:(ProceduralBlock)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The target insets of the animation.
@property(nonatomic, readonly) UIEdgeInsets targetInsets;

// The current insets calculated during the animation.
@property(nonatomic, readonly) UIEdgeInsets currentInsets;

// Starts the CADisplayLink animation on the main run loop.
- (void)start;

// Stops and invalidates the display link animation without invoking completion.
- (void)stop;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_VIEWPORT_INSETS_ANIMATOR_H_

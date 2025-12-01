// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_ANIMATION_CONTEXT_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_ANIMATION_CONTEXT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"

// Defines the set of views and functional hooks required when presenting and
// dismissing the composebox.
@protocol ComposeboxAnimationContext

// The input plate view to be animated.
@property(nonatomic, readonly) UIView* inputPlateViewForAnimation;

// The close button to be animated.
@property(nonatomic, readonly) UIView* closeButtonForAnimation;

// The suggestions popup to be animated.
@property(nonatomic, readonly) UIView* popupViewForAnimation;

// Informs the composebox to update its visual mode to the given `mode`.
- (void)setComposeboxMode:(ComposeboxMode)mode;

// Requests the input plate to expand beyond to full width when dismissing.
- (void)expandInputPlateForDismissal;

// Whether the composebox is compact.
- (BOOL)inputPlateIsCompact;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_ANIMATION_CONTEXT_H_

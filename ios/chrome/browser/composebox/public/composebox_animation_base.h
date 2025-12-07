// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ANIMATION_BASE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ANIMATION_BASE_H_

#include <UIKit/UIKit.h>

// Describes the possible interactions with the composebox.
@protocol ComposeboxAnimationBase

// Whether to temporarily hide the entrypoint. This is used during the
// animation to avoid morphing the real view.
- (void)setEntrypointViewHidden:(BOOL)hidden;

// Returns a visual copy of the entrypoint view, to avoid manipulating the real
// view hierarchy for animations.
- (UIView*)entrypointViewVisualCopy;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ANIMATION_BASE_H_

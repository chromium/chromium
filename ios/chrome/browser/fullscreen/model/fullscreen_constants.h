// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>

#import "base/time/time.h"

// Progress threshold for automatically animating into fullscreen (collapsed).
inline constexpr CGFloat kEnterFullscreenProgressThreshold = 0.75;

// Progress threshold for automatically animating out of fullscreen (expanded).
inline constexpr CGFloat kExitFullscreenProgressThreshold = 0.25;

// The nominal scroll distance (in points) that maps to a full (0.0 to 1.0)
// transition. In practice, scrolling past X% of this distance triggers the
// animated transition to complete the rest.
inline constexpr CGFloat kEasedTransitionScrollDistance = 250.0;

// The scroll distance threshold for snapping to a fullscreen state.
inline constexpr CGFloat kFullscreenSnapThreshold = 10.0;

// Minimum and maximum duration for eased scroll transitions.
inline constexpr base::TimeDelta kEasedTransitionMinDuration =
    base::Milliseconds(250);
inline constexpr base::TimeDelta kEasedTransitionMaxDuration =
    base::Milliseconds(750);

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_CONSTANTS_H_

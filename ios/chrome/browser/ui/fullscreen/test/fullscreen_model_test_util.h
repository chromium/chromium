// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_FULLSCREEN_MODEL_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_FULLSCREEN_MODEL_TEST_UTIL_H_

#import <CoreGraphics/CoreGraphics.h>

class FullscreenModel;

// Sets the model's toolbar height and simulates a page load with a {0, 0}
// content offset.
void SetUpFullscreenModelForTesting(FullscreenModel* model,
                                    CGFloat toolbar_height);

// Simulates a user scroll event in `model` for a scroll of `offset_delta`
// points.
void SimulateFullscreenUserScrollWithDelta(FullscreenModel* model,
                                           CGFloat offset_delta);

// Simulates a user scroll event in `model` that will result in a progress value
// of `progress`.
void SimulateFullscreenUserScrollForProgress(FullscreenModel* model,
                                             CGFloat progress);

// Returns the delta from `model`'s current Y offset that would result in
// `progress`.
CGFloat GetFullscreenOffsetDeltaForProgress(FullscreenModel* model,
                                            CGFloat progress);

// Returns the base offset against which `model` would calculate `progress`,
// given its toolbar height and content offset.
CGFloat GetFullscreenBaseOffsetForProgress(FullscreenModel* model,
                                           CGFloat progress);

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_FULLSCREEN_MODEL_TEST_UTIL_H_

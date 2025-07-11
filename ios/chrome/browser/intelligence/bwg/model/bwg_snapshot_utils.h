// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SNAPSHOT_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SNAPSHOT_UTILS_H_

#import <UIKit/UIKit.h>

namespace bwg_snapshot_utils {

// Gets a snapshot of the entire window cropped as follows:
//
// - The left and right sides are cropped at the window's safe area.
// - The bottom is cropped at the window's safe area.
// - The top is cropped at the same place as the Content Area's top (web
// view).
//
// The result is a snapshot that spans the full width horizontally, and
// starts below the OS status bar/safe area (+ below the omnibox if located at
// the top) until the safe area at the bottom vertically.
//
// `view` is used to fetch a UIWindow and to find the ContentArea layout guide
// (needs to be in the same view hierarchy).
UIImage* GetCroppedFullscreenSnapshot(UIView* view);

}  // namespace bwg_snapshot_utils

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SNAPSHOT_UTILS_H_

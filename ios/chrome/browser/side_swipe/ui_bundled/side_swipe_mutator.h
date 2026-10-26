// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MUTATOR_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "base/feature_list.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_constants.h"

// Killswitch for recording user actions for back and forward edge swipes.
BASE_DECLARE_FEATURE(kRecordEdgeSwipeMetrics);

@class SideSwipeSnapshotNavigationView;

// Manages side-swipe navigation and tab switching.
@protocol SideSwipeMutator

// Performs navigation in the specified direction.
- (void)navigateInDirection:(NavigationDirection)direction;

// Notifies the mutator that a native WKWebView edge swipe started in the
// specified `direction`.
- (void)webViewEdgeSwipeDidStartInDirection:(NavigationDirection)direction;

// Notifies the mutator that a native WKWebView edge swipe was cancelled or
// failed.
- (void)webViewEdgeSwipeDidCancel;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MUTATOR_H_

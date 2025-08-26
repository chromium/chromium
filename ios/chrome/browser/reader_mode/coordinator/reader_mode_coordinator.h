// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol OverscrollActionsControllerDelegate;

// Coordinator for the Reader mode UI.
@interface ReaderModeCoordinator : ChromeCoordinator

// Required to support Overscroll Actions UI, which is displayed when Reader
// mode is pulled down.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

// Starts/stops the coordinator.
// If `animated` is true then the UI is presented/dismissed with an animation.
- (void)startAnimated:(BOOL)animated;
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_COORDINATOR_H_

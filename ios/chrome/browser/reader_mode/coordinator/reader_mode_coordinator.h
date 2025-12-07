// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_COORDINATOR_H_

#import "ios/chrome/browser/reader_mode/ui/reader_mode_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ReaderModeCoordinator;
@protocol OverscrollActionsControllerDelegate;

// Delegate for ReaderModeCoordinator.
@protocol ReaderModeCoordinatorDelegate <NSObject>
// Called when the reader mode animation completes.
- (void)readerModeCoordinatorAnimationDidComplete:
    (ReaderModeCoordinator*)coordinator;
@end

// Coordinator for the Reader mode UI.
@interface ReaderModeCoordinator
    : ChromeCoordinator <ReaderModeViewControllerDelegate>

// Required to support Overscroll Actions UI, which is displayed when Reader
// mode is pulled down.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

@property(nonatomic, weak) id<ReaderModeCoordinatorDelegate> delegate;

// Starts/stops the coordinator.
// If `animated` is true then the UI is presented/dismissed with an animation.
- (void)startAnimated:(BOOL)animated;
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_COORDINATOR_H_

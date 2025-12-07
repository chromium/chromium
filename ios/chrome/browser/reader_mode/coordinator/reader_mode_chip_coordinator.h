// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_CHIP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_CHIP_COORDINATOR_H_

#import "ios/chrome/browser/reader_mode/ui/reader_mode_chip_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ReaderModeChipVisibilityDelegate;

// Coordinator for the Reader Mode Chip.
@interface ReaderModeChipCoordinator : ChromeCoordinator

// The viewController visibility delegate.
@property(nonatomic, weak) id<ReaderModeChipVisibilityDelegate>
    visibilityDelegate;

// The view controller for this coordinator.
@property(nonatomic, strong) ReaderModeChipViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_COORDINATOR_READER_MODE_CHIP_COORDINATOR_H_

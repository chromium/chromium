// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutoDeletionCoordinatorDelegate;

// Coordinator for the Auto-deletion promo & action-sheet.
@interface AutoDeletionCoordinator : ChromeCoordinator

// Delegate notified when the coordinator should be stopped.
@property(nonatomic, weak) id<AutoDeletionCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_H_

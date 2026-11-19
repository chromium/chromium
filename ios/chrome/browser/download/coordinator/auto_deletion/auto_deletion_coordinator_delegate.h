// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AutoDeletionCoordinator;

// Delegate for `AutoDeletionCoordinator`.
@protocol AutoDeletionCoordinatorDelegate <NSObject>

// Called when the coordinator wants to be stopped.
- (void)autoDeletionCoordinatorWantsToBeStopped:
    (AutoDeletionCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_COORDINATOR_DELEGATE_H_

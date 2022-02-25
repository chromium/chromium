// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class UMACoordinator;

// Delegate for UMACoordinator.
@protocol UMACoordinatorDelegate <NSObject>

// Called when the coordinator has been removed from the screen.
- (void)UMACoordinatorDidRemove:(UMACoordinator*)coordinator;

@end

// Coordinator to present UMA dialog in the FRE.
@interface UMACoordinator : ChromeCoordinator

@property(nonatomic, weak) id<UMACoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_COORDINATOR_H_

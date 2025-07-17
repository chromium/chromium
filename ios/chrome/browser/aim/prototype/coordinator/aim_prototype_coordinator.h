// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COORDINATOR_H_

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AIMPrototypeCoordinator;

// Delegate for the AIM prototype coordinator.
@protocol AIMPrototypeCoordinatorDelegate
- (void)aimPrototypeCoordinatorDidFinish:(AIMPrototypeCoordinator*)coordinator;
@end

// LensOverlayCoordinator presents the public interface for the Lens Overlay.
@interface AIMPrototypeCoordinator
    : ChromeCoordinator <AIMPrototypeViewControllerDelegate>

@property(nonatomic, weak) id<AIMPrototypeCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COORDINATOR_H_

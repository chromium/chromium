// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_CONTAINER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class AIMPrototypeEntrypoint;

// Coordinator that contains the AIM prototype, presenting it modally.
@interface AIMPrototypeContainerCoordinator : ChromeCoordinator

/// Initializes the coordinator with the `baseViewController`, `browser`,
/// `entrypoint` and an optional `query` to pre-fill the omnibox.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                entrypoint:(AIMPrototypeEntrypoint)entrypoint
                                     query:(NSString*)query
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_CONTAINER_COORDINATOR_H_

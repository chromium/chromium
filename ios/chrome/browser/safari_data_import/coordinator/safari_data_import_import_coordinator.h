// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SafariDataImportChildCoordinatorDelegate;
enum class SafariDataImportStage;

/// Coordinator for the safari data import screen.
@interface SafariDataImportImportCoordinator : ChromeCoordinator

/// Delegate object that handles Safari import events.
@property(nonatomic, weak) id<SafariDataImportChildCoordinatorDelegate>
    delegate;

/// The current stage of import.
@property(nonatomic, readonly) SafariDataImportStage importStage;

/// Coordinator should be initialized with a base navigation view controller.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_IMPORT_COORDINATOR_H_

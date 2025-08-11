// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_MAIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_MAIN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SafariDataImportMainCoordinator;
@protocol SafariDataImportUIHandler;
enum class SafariDataImportEntryPoint;

/// Delegate object for the Safari data import flow.
@protocol SafariDataImportMainCoordinatorDelegate

/// Notifies that the safari import workflow has ended.
- (void)safariImportWorkflowDidEndForCoordinator:
    (SafariDataImportMainCoordinator*)coordinator;

@end

/// The coordinator starting the workflow that imports Safari data to Chrome.
/// Also the first in a chain of coordinators that respectively handles each
/// stage of the workflow.
@interface SafariDataImportMainCoordinator : ChromeCoordinator

/// Delegate object that handles Safari import events.
@property(nonatomic, weak) id<SafariDataImportMainCoordinatorDelegate> delegate;

/// Handler for Safari import workflow UI events. Optional.
@property(nonatomic, weak) id<SafariDataImportUIHandler> UIHandler;

/// Initializer.
- (instancetype)initFromEntryPoint:(SafariDataImportEntryPoint)entryPoint
            withBaseViewController:(UIViewController*)viewController
                           browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_MAIN_COORDINATOR_H_

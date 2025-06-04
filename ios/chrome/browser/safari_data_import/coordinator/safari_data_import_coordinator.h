// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SafariDataImportCoordinator;
@protocol SafariDataImportUIHandler;

/// Delegate object for the Safari data import flow.
@protocol SafariDataImportCoordinatorDelegate

/// Notifies that the safari import workflow has ended.
- (void)safariImportWorkflowDidEndForCoordinator:
    (SafariDataImportCoordinator*)coordinator;

@end

/// The coordinator for the flow that imports Safari data to Chrome.
@interface SafariDataImportCoordinator : ChromeCoordinator

/// Delegate object that handles Safari import events.
@property(nonatomic, weak) id<SafariDataImportCoordinatorDelegate> delegate;

/// Handler for Safari import workflow UI events. Optional.
@property(nonatomic, weak) id<SafariDataImportUIHandler> UIHandler;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_COORDINATOR_H_

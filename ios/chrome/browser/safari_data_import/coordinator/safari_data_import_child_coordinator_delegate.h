// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_CHILD_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_CHILD_COORDINATOR_DELEGATE_H_

@class ChromeCoordinator;

/// Delegate for coordinaters presented by the main coordinator.
@protocol SafariDataImportChildCoordinatorDelegate

/// User asks to dismiss the Safari data import  by tapping "done" or "close"
/// button.
- (void)safariDataImportCoordinatorWillDismissWorkflow:
    (ChromeCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_CHILD_COORDINATOR_DELEGATE_H_

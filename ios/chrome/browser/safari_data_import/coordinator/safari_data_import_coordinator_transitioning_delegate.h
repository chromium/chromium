// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_COORDINATOR_TRANSITIONING_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_COORDINATOR_TRANSITIONING_DELEGATE_H_

@class ChromeCoordinator;

/// Delegate declaration handling screen transitions and dismissal of the Safari
/// data import workflow.
@protocol SafariDataImportCoordinatorTransitioningDelegate

/// User asks to dismiss the Safari data import  by tapping "done" or "close"
/// button.
- (void)safariDataImportCoordinatorWillDismissWorkflow:
    (ChromeCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_COORDINATOR_SAFARI_DATA_IMPORT_COORDINATOR_TRANSITIONING_DELEGATE_H_

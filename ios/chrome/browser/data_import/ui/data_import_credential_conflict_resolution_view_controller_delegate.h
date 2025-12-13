// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_RESOLUTION_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_RESOLUTION_VIEW_CONTROLLER_DELEGATE_H_

// Handles dismissal of credential conflict resolution view.
@protocol DataImportCredentialConflictResolutionViewControllerDelegate

// Called when the user dismisses the conflict resolution screen, handles
// dismissal of the view.
- (void)cancelledConflictResolution;

// Called when the user clicks "Continue" in the conflict resolution screen,
// handles dismissal of the view.
- (void)resolvedCredentialConflicts;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_CREDENTIAL_CONFLICT_RESOLUTION_VIEW_CONTROLLER_DELEGATE_H_

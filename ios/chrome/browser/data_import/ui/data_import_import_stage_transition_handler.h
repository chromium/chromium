// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_IMPORT_STAGE_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_IMPORT_STAGE_TRANSITION_HANDLER_H_

/// Consumer that updates the UI to reflect import stage transition.
@protocol DataImportImportStageTransitionHandler

// Reasons for resetting the import flow to the initial stage.
enum class DataImportResetReason {
  // The flow was reset because the user manually cancelled it.
  kUserInitiated,
  // The flow was reset because no importable data was found in the selected
  // file.
  kNoImportableData,
  // The flow was reset because all detected data types are blocked by
  // enterprise policy.
  kAllDataBlockedByPolicy,
};

/// Transition to the next import stage.
- (void)transitionToNextImportStage;

/// Resets the import flow to the initial stage for the given `reason`.
- (void)resetToInitialImportStage:(DataImportResetReason)reason;

@end

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_UI_DATA_IMPORT_IMPORT_STAGE_TRANSITION_HANDLER_H_

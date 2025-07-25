// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_STAGE_TRANSITION_HANDLER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_STAGE_TRANSITION_HANDLER_H_

enum class SafariDataImportStage;

/// Consumer that updates the UI to reflect import stage transition.
@protocol SafariDataImportImportStageTransitionHandler

/// Transition to the next import stage.
- (void)transitionToNextImportStage;

/// Reset the import stage to `kNotReady`. This should be invoked when file
/// selection or processing is halted. If the user cancels the file selection,
/// `userInitiated` should be YES.
- (void)resetToInitialImportStage:(BOOL)userInitiated;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_STAGE_TRANSITION_HANDLER_H_

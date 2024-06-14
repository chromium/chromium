// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_

// Commands related to contextual sheet
@protocol ContextualSheetCommands

// Opens the contextual sheet, activating it for the current tab.
- (void)openContextualSheet;

// Closes the contextual sheet, deactivating it for the current tab.
- (void)closeContextualSheet;

// Shows the contextual sheet UI if it is active for the current tab.
- (void)showContextualSheetUIIfActive;

// Hides the contextual sheet but does not deactivate it for the current tab.
// This could be used, for example, when switching tabs.
- (void)hideContextualSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_

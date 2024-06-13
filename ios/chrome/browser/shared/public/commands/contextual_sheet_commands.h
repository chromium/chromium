// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_

// Commands related to contextual sheet
@protocol ContextualSheetCommands

// Opens the contextual sheet.
- (void)openContextualSheet;

// Closes the contextual sheet.
- (void)closeContextualSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_

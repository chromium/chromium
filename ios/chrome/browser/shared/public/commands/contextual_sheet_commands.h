// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_

// Commands related to contextual sheet
@protocol ContextualSheetCommands

// Shows the contextual sheet.
- (void)showContextualSheet;

// Hides the contextual sheet.
- (void)hideContextualSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CONTEXTUAL_SHEET_COMMANDS_H_

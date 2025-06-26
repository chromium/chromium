// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_ENTRY_POINT_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_ENTRY_POINT_COMMANDS_H_

// Commands to communication with the page action menu entry point.
@protocol PageActionMenuEntryPointCommands <NSObject>

// If YES, highlights PageActionMenu entry point. Otherwise, unhighlights.
- (void)toggleEntryPointHighlight:(BOOL)highlight;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_ENTRY_POINT_COMMANDS_H_

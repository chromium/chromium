// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_COMMANDS_H_

#import "base/ios/block_types.h"

// Commands relating to the page action menu.
@protocol PageActionMenuCommands

// Presents the page action menu.
- (void)showPageActionMenu;

// Dismisses the page action menu with a completion block.
- (void)dismissPageActionMenuWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_COMMANDS_H_

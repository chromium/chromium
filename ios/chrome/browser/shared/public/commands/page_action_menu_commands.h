// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_COMMANDS_H_

#import "base/ios/block_types.h"

// Enum to specify the entry point for showing the Page Action Menu.
typedef NS_ENUM(NSInteger, PageActionMenuEntryPoint) {
  PageActionMenuEntryPointLocationBar = 0,
  PageActionMenuEntryPointTabGrid = 1,
};

// Commands relating to the page action menu.
@protocol PageActionMenuCommands

// Presents the page action menu from the given entrypoint.
- (void)showPageActionMenuFromEntryPoint:(PageActionMenuEntryPoint)entryPoint;

// Dismisses the page action menu with a completion block.
- (void)dismissPageActionMenuWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PAGE_ACTION_MENU_COMMANDS_H_

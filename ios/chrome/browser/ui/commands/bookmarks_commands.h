// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARKS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARKS_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/bookmark_add_command.h"

@class ReadingListAddCommand;

// Protocol for commands arounds Bookmarks manipulation.
@protocol BookmarksCommands <NSObject>

// Bookmarks the page detailed in |command|'s data unless it's already
// bookmarked, in that case the "edit bookmark" flow will be triggered.
- (void)bookmarkPage:(BookmarkAddCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARKS_COMMANDS_H_

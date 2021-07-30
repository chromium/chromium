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

// Adds bookmarks for the given list of URLs in |command|.
// If there are multiple URLs provided:
// - the user will be prompted to choose a location to store the bookmarks.
// If a single URL is provided:
// - and it is already bookmarked, the "edit bookmark" flow will begin.
// - which is not already bookmarked, it will be bookmarked automatically and an
//   "Edit" button will be provided in the displayed snackbar message.
- (void)bookmark:(BookmarkAddCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARKS_COMMANDS_H_

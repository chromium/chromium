// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARKS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARKS_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/bookmark_add_command.h"

@class ReadingListAddCommand;

// Protocol for commands arounds Bookmarks manipulation.
@protocol BookmarksCommands <NSObject>

// Adds bookmarks for the given list of URLs in `command`.
// If `command.presentFolderChooser` is true:
// - the user will be prompted to choose a location to store the bookmarks.
// Otherwise, only a single URL must be provided:
// - If it is already bookmarked, the "edit bookmark" flow will begin.
// - If it is not already bookmarked, it will be bookmarked automatically and an
//   "Edit" button will be provided in the displayed snackbar message.
- (void)bookmark:(BookmarkAddCommand*)command;

// Opens the Bookmarks UI in edit mode and selects the bookmark node
// corresponding to the values provided within `command`.
- (void)openToExternalBookmark:(BookmarkAddCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARKS_COMMANDS_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARKS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARKS_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GURL;
@class ReadingListAddCommand;
@class URLWithTitle;
namespace web {
class WebState;
}  // namespace web

// TODO(crbug.com/40268151): Remove this commands.
// Protocol for commands arounds Bookmarks manipulation.
@protocol BookmarksCommands <NSObject>

// Adds bookmarks for the given list of URLs.
// The user will be prompted to choose a location to store the bookmarks.
- (void)bookmarkWithFolderChooser:(NSArray<URLWithTitle*>*)URLs;

// Adds bookmark for the last committed URL, and tab title.
// Behaves as `-(void)bookmark:(URLWithTitle*)` otherwise.
- (void)bookmarkWithWebState:(web::WebState*)webState;

// Bulk adds passed URLs to bookmarks. Toasts the amount of successfully added
// bookmarks with a button to view bookmarks. Does not add invalid URLs or
// already existing ones into the model.
- (void)bulkCreateBookmarksWithURLs:(NSArray<NSURL*>*)URLs;

// Adds bookmark for the URL.
// - If it is already bookmarked, the "edit bookmark" flow will begin.
// - If it is not already bookmarked, it will be bookmarked automatically and an
//   "Edit" button will be provided in the displayed snackbar message.
- (void)createOrEditBookmarkWithURL:(URLWithTitle*)URLWithTitle;

// Opens the Bookmarks UI in edit mode and selects the bookmark node
// corresponding to `URL`.
- (void)openToExternalBookmark:(GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARKS_COMMANDS_H_

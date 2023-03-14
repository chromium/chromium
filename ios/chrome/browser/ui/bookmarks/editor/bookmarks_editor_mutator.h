// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_MUTATOR_H_

#import <Foundation/Foundation.h>

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
}  // namespace bookmarks

// Allows the bookmark editor’s view controller to reflect user’s change in the
// model.
@protocol BookmarksEditorMutator <NSObject>

// TODO(crbug.com/1404311): Remove those accessor and setters.
// We temporarily use them to facilitate code migration.
- (const bookmarks::BookmarkNode*)bookmark;
- (void)setBookmark:(const bookmarks::BookmarkNode*)bookmark;
- (bookmarks::BookmarkModel*)bookmarkModel;
- (const bookmarks::BookmarkNode*)folder;
- (BOOL)ignoresBookmarkModelChanges;
- (BOOL*)ignoresBookmarkModelChangesPointer;
- (BOOL)shouldDisplayCloudSlashSymbolForParentFolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_MUTATOR_H_

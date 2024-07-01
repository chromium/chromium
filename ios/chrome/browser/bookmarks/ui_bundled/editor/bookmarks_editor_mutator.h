// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MUTATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MUTATOR_H_

#import <Foundation/Foundation.h>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Allows the bookmark editor’s view controller to reflect user’s change in the
// model.
// TODO(crbug.com/40255290): Change of model should be done through the mutator.
// Right now they are done through the coordinator, because some utils function
// deals simultaneously with changing the model and opening a toast.
@protocol BookmarksEditorMutator <NSObject>

// Save the bookmark being edited.
- (void)commitBookmarkChangesWithURLString:(NSString*)URL name:(NSString*)name;

// Delete the bookmark being edited. This will also dismiss the editor UI
// afterwards.
- (void)deleteBookmark;

// TODO(crbug.com/40251848): Remove those accessor and setters.
// We temporarily use them to facilitate code migration.
- (const bookmarks::BookmarkNode*)bookmark;
- (const bookmarks::BookmarkNode*)folder;
- (BOOL)ignoresBookmarkModelChanges;
- (BOOL)shouldDisplayCloudSlashSymbolForParentFolder;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MUTATOR_H_

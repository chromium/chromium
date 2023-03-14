// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <vector>

namespace bookmarks {
class BookmarkNode;
}

// Consumer protocol to receive updates from the model layer.
@protocol BookmarksFolderChooserConsumer <NSObject>

// Notifies the UI layer that model layer has been updated.
- (void)notifyModelUpdated;

@end

// TODO(crbug.com/1405746): Refactor all the methods in this protocol after the
// view controller has been refactored. View controller should not know about
// BookmarkNode.
// Data source protocol to get data on demand.
@protocol BookmarksFolderChooserDataSource <NSObject>

// Root folder in the bookmark model tree.
- (const bookmarks::BookmarkNode*)rootFolder;
// The folder that should have a blue check mark beside it in the UI.
- (const bookmarks::BookmarkNode*)selectedFolder;
// Whether to display the cloud slashed icon beside the folders.
- (BOOL)shouldDisplayCloudIconForProfileBookmarks;
// The list of visible folders to show in the folder chooser UI.
- (std::vector<const bookmarks::BookmarkNode*>)visibleFolders;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_CONSUMER_H_

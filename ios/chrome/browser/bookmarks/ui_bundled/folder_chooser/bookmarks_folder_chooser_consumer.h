// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_CONSUMER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_CONSUMER_H_

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

// A sub-data source protocol used to get data from a single bookmark model.
@protocol BookmarksFolderChooserSubDataSource <NSObject>

// "Mobile Bookmarks" folder node that always exists by default.
- (const bookmarks::BookmarkNode*)mobileFolderNode;
// The list of visible folders to show.
- (std::vector<const bookmarks::BookmarkNode*>)visibleFolderNodes;

@end

// TODO(crbug.com/40252439): Refactor all the methods in this protocol after the
// view controller has been refactored. View controller should not know about
// BookmarkNode.
// Data source protocol to get data on demand.
@protocol BookmarksFolderChooserDataSource <NSObject>

// Data source from account bookmark model. Clients should call
// `shouldShowAccountBookmarks` before accessing this property because if
// that method returns `NO` then account data source may not be available.
@property(nonatomic, readonly) id<BookmarksFolderChooserSubDataSource>
    accountDataSource;
// Data source from localOrSyncable bookmark model.
@property(nonatomic, readonly) id<BookmarksFolderChooserSubDataSource>
    localOrSyncableDataSource;

// The folder that should have a blue check mark beside it in the UI.
- (const bookmarks::BookmarkNode*)selectedFolderNode;
// Whether to display the cloud slashed icon beside the folders.
- (BOOL)shouldDisplayCloudIconForLocalOrSyncableBookmarks;
// Whether to show the account bookmarks section.
- (BOOL)shouldShowAccountBookmarks;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_CONSUMER_H_

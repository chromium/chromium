// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_SUB_DATA_SOURCE_IMPL_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_SUB_DATA_SOURCE_IMPL_H_

#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_consumer.h"

#import <Foundation/Foundation.h>
#import <set>

enum class BookmarkStorageType;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// Protocol to access and update data from parent data source object. Please
// note that the parent data source is not and should not be responsible for
// updating the UI in these method calls as that distributes UI update logic
// across multiple files. `BookmarksFolderChooserSubDataSourceImpl` is
// responsible for updating the UI after calling these methods.
@protocol BookmarksFolderChooserParentDataSource <NSObject>

// Called when a bookmark node is deleted from the model.
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)bookmarkNode;
// Called before all the bookmark nodes in the model are deleted.
- (void)bookmarkModelWillRemoveAllNodes;
// The set of nodes that are being considered for a move by folder chooser.
- (const std::set<const bookmarks::BookmarkNode*>&)editedNodes;

@end

// A data source class that encapsulates the interaction with the
// `BookmarkModel`.
@interface BookmarksFolderChooserSubDataSourceImpl
    : NSObject <BookmarksFolderChooserSubDataSource>

// Consumer to reflect model changes in the UI.
@property(nonatomic, weak) id<BookmarksFolderChooserConsumer> consumer;

// Both `bookmarkModel` and `parentDataSource` needs to be non null.
// Additionally, `bookmarkModel` needs to be fully loaded.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                                 type:(BookmarkStorageType)type
                     parentDataSource:
                         (id<BookmarksFolderChooserParentDataSource>)
                             parentDataSource NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_SUB_DATA_SOURCE_IMPL_H_

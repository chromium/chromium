// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol BookmarksFolderChooserDataSource;
@protocol BookmarksFolderChooserMutator;
@protocol BookmarksFolderChooserViewControllerPresentationDelegate;
class Browser;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

// A folder selector view controller.
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
// The bookmark model is assumed to be loaded, thus also not to be NULL.
@interface BookmarksFolderChooserViewController
    : ChromeTableViewController <BookmarksFolderChooserConsumer>

@property(nonatomic, weak)
    id<BookmarksFolderChooserViewControllerPresentationDelegate>
        delegate;
// Data source from the model layer.
@property(nonatomic, weak) id<BookmarksFolderChooserDataSource> dataSource;
// Mutator to apply changes to model layer.
@property(nonatomic, weak) id<BookmarksFolderChooserMutator> mutator;

// TODO(crbug.com/1405746): Move `bookmarkModel` and `browser` to the model
// layer.
// Initializes the view controller with a bookmark model.
// `bookmarkModel` must not be `nullptr` and must be loaded.
// `allowsNewFolders` will instruct the controller to provide the necessary UI
// to create a folder.
// `allowsCancel` puts a cancel and done button in the navigation bar instead
// of a back button, which is needed if this view controller is presented
// modally.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     allowsNewFolders:(BOOL)allowsNewFolders
                         allowsCancel:(BOOL)allowsCancel
                              browser:(Browser*)browser;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_

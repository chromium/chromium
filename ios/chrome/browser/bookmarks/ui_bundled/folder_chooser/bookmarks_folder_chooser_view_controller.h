// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_consumer.h"

@protocol BookmarksFolderChooserDataSource;
@protocol BookmarksFolderChooserMutator;
@protocol BookmarksFolderChooserViewControllerPresentationDelegate;

// A folder selector view controller.
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
// The bookmark model is assumed to be loaded, thus also not to be NULL.
@interface BookmarksFolderChooserViewController
    : LegacyChromeTableViewController <BookmarksFolderChooserConsumer>

@property(nonatomic, weak)
    id<BookmarksFolderChooserViewControllerPresentationDelegate>
        delegate;
// Data source from the model layer.
@property(nonatomic, weak) id<BookmarksFolderChooserDataSource> dataSource;
// Mutator to apply changes to model layer.
@property(nonatomic, weak) id<BookmarksFolderChooserMutator> mutator;

// Initializes the view controller.
// `allowsCancel` puts a cancel and done button in the navigation bar instead
// of a back button, which is needed if this view controller is presented
// modally.
// `allowsNewFolders` will instruct the controller to provide the necessary UI
// to create a folder.
- (instancetype)initWithAllowsCancel:(BOOL)allowsCancel
                    allowsNewFolders:(BOOL)allowsNewFolders
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_

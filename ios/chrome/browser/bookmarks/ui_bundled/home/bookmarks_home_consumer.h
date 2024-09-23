// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_CONSUMER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_CONSUMER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class BookmarksHomeMediator;
@class NSIndexPath;
@class ShowSigninCommand;
@class SigninPromoViewConfigurator;
@class UITableView;
@class TableViewModel;

typedef NS_ENUM(NSInteger, BookmarksHomeBackgroundStyle) {
  // The default background style.
  BookmarksHomeBackgroundStyleDefault,

  // A background style that indicates that bookmarks are loading.
  BookmarksHomeBackgroundStyleLoading,

  // A background style that indicates that no bookmarks are present.
  BookmarksHomeBackgroundStyleEmpty,
};

// BookmarksHomeConsumer provides methods that allow mediators to update the UI.
@protocol BookmarksHomeConsumer <LegacyChromeTableViewConsumer>

// The model backing the table view.
@property(nonatomic, readonly) TableViewModel* tableViewModel;

// The UITableView to show bookmarks.
@property(nonatomic, readonly) UITableView* tableView;

// The cell for the newly created folder while its name is being edited. Set
// to nil once the editing completes. Corresponds to `editingFolderNode`.
@property(nonatomic, weak)
    UITableViewCell<BookmarkTableCellTitleEditing>* editingFolderCell;

// Whether the displayed folder is the root node.
@property(nonatomic, assign, readonly) BOOL isDisplayingBookmarkRoot;

// Refreshes the UI.
- (void)refreshContents;

// Starts an asynchronous favicon load for the row at the given `indexPath`. Can
// optionally fetch a favicon from a Google server if nothing suitable is found
// locally; otherwise uses the fallback icon style.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
        fallbackToGoogleServer:(BOOL)fallbackToGoogleServer;

// Displays the table view background for the given `style`.
- (void)updateTableViewBackgroundStyle:(BookmarksHomeBackgroundStyle)style;

// Sets the editing mode for tableView, update context bar and search state
// accordingly.
- (void)setTableViewEditing:(BOOL)editing;

// Displays the signin UI configured by `command`.
- (void)showSignin:(ShowSigninCommand*)command;

// Reconfigures the cell at the given `indexPath` with the given `configurator`.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                                 atIndexPath:(NSIndexPath*)indexPath;

// Called when the set of edit nodes is cleared.
- (void)mediatorDidClearEditNodes:(BookmarksHomeMediator*)mediator;

// Displays the account settings.
- (void)showAccountSettings;

// Called when this folder is deleted.
- (void)closeThisFolder;

// Called when all account bookmarks are deleted (e.g. signout).
- (void)displayRoot;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_CONSUMER_H_

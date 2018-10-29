// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_CONSUMER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"

@class NSIndexPath;
@class ShowSigninCommand;
@class SigninPromoViewConfigurator;

typedef NS_ENUM(NSInteger, BookmarkHomeBackgroundStyle) {
  // The default background style.
  BookmarkHomeBackgroundStyleDefault,

  // A background style that indicates that bookmarks are loading.
  BookmarkHomeBackgroundStyleLoading,

  // A background style that indicates that no bookmarks are present.
  BookmarkHomeBackgroundStyleEmpty,
};

// BookmarkHomeConsumer provides methods that allow mediators to update the UI.
@protocol BookmarkHomeConsumer<ChromeTableViewConsumer>

// Refreshes the UI.
- (void)refreshContents;

// Starts an asynchronous favicon load for the row at the given |indexPath|. Can
// optionally fetch a favicon from a Google server if nothing suitable is found
// locally; otherwise uses the fallback icon style.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
        fallbackToGoogleServer:(BOOL)fallbackToGoogleServer;

// Displays the table view background for the given |style|.
- (void)updateTableViewBackgroundStyle:(BookmarkHomeBackgroundStyle)style;

// Displays the signin UI configured by |command|.
- (void)showSignin:(ShowSigninCommand*)command;

// Reconfigures the cell at the given |indexPath| with the given |configurator|.
// If |forceReloadCell| is YES, reloads the section when complete.
- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                                 atIndexPath:(NSIndexPath*)indexPath
                             forceReloadCell:(BOOL)forceReloadCell;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_CONSUMER_H_

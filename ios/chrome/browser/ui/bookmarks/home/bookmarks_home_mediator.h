// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import <set>
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

@protocol BookmarksHomeConsumer;
@class BookmarkTableCell;
@protocol BookmarkTableCellTitleEditing;
class Browser;
@class TableViewModel;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

typedef NS_ENUM(NSInteger, BookmarksHomeSectionIdentifier) {
  // Section to invite the user to sign in and sync.
  BookmarksHomeSectionIdentifierPromo = kSectionIdentifierEnumZero,
  // Section to display either:
  // * The bookmarks of current search result or
  // * the bookmarks of the currently displayed folder, assuming it’s not root.
  BookmarksHomeSectionIdentifierBookmarks,
  // Section to display the root folders of the profile. See go/b4b-ios.
  BookmarksHomeSectionIdentifierRootProfile,
  // Section to display the root folders of the account. See go/b4b-ios.
  BookmarksHomeSectionIdentifierRootAccount,
  // Section to display a message, such as "no result" for a search.
  BookmarksHomeSectionIdentifierMessages,
};

// Whether this section contains bookmarks nodes.
// This function return true even on a section that is empty, as soon as it
// could possibly contains bookmark node.
bool IsABookmarkNodeSectionForIdentifier(
    BookmarksHomeSectionIdentifier sectionIdentifier);

typedef NS_ENUM(NSInteger, BookmarksHomeItemType) {
  BookmarksHomeItemTypeHeader = kItemTypeEnumZero,
  BookmarksHomeItemTypePromo,
  BookmarksHomeItemTypeBookmark,
  BookmarksHomeItemTypeMessage,
};

// BookmarksHomeMediator manages model interactions for the
// BookmarksHomeViewController.
@interface BookmarksHomeMediator : NSObject

// Models.

// The model holding profile bookmark data.
@property(nonatomic, assign, readonly)
    bookmarks::BookmarkModel* profileBookmarkModel;
// The model holding account bookmark data.
@property(nonatomic, assign, readonly)
    bookmarks::BookmarkModel* accountBookmarkModel;

// State variables.

// The BookmarkNode that is currently being displayed by the table view.  May be
// nil.
@property(nonatomic, assign) const bookmarks::BookmarkNode* displayedNode;

// If the table view is in edit mode; i.e. when the user can select bookmarks.
@property(nonatomic, assign) BOOL currentlyInEditMode;

// The set of nodes currently being edited.
@property(nonatomic, readonly, assign)
    std::set<const bookmarks::BookmarkNode*>& selectedNodesForEditMode;

// If the table view showing search results.
@property(nonatomic, assign) BOOL currentlyShowingSearchResults;

// True if the promo is visible.
@property(nonatomic, assign) BOOL promoVisible;

@property(nonatomic, weak) id<BookmarksHomeConsumer> consumer;

// If a new folder is being added currently.
@property(nonatomic, assign) BOOL addingNewFolder;

// The newly created folder node its name is being edited.
@property(nonatomic, assign) const bookmarks::BookmarkNode* editingFolderNode;

// Designated initializer.
// `baseViewController` view controller used to present sign-in UI.
// TODO(crbug.com/1402758): `browser` and `baseViewController` need to be
// removed from `BookmarksHomeMediator`. A mediator should not be aware of
// those classes.
- (instancetype)initWithBrowser:(Browser*)browser
             baseViewController:(UIViewController*)baseViewController
           profileBookmarkModel:(bookmarks::BookmarkModel*)profileBookmarkModel
           accountBookmarkModel:(bookmarks::BookmarkModel*)accountBookmarkModel
                  displayedNode:(const bookmarks::BookmarkNode*)displayedNode
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Starts this mediator. Populates the table view model with current data and
// begins listening for backend model updates.
- (void)startMediating;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

// Rebuilds the table view model data for the Bookmarks section.  Deletes any
// existing data first.
- (void)computeBookmarkTableViewData;

// Rebuilds the table view model data for the bookmarks matching the given text.
// Deletes any existing data first.  If no items found, an entry with
// `noResults' message is added to the table.
- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults;

// Updates promo cell based on its current visibility.
- (void)computePromoTableViewData;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_MEDIATOR_H_

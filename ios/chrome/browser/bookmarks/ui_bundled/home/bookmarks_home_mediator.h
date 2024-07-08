// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_MEDIATOR_H_

#import <UIKit/UIKit.h>
#import <set>
#import <string>
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

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

typedef NS_ENUM(NSInteger, BookmarksHomeSectionIdentifier) {
  // Section to invite the user to sign in and sync.
  BookmarksHomeSectionIdentifierPromo = kSectionIdentifierEnumZero,
  // Section to display either:
  // * The bookmarks of current search result or
  // * the bookmarks of the currently displayed folder, assuming it’s not root.
  BookmarksHomeSectionIdentifierBookmarks,
  // Section to display the root folders of the account. See go/b4b-ios.
  BookmarksHomeSectionIdentifierRootAccount,
  // Section to display the root folders of the localOrSyncable. See go/b4b-ios.
  BookmarksHomeSectionIdentifierRootLocalOrSyncable,
  // Section to display a message, such as "no result" for a search.
  BookmarksHomeSectionIdentifierMessages,
  // Section to display the batch upload option.
  BookmarksBatchUploadSectionIdentifier,
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
  BookmarksHomeItemTypeBatchUploadButton,
  BookmarksHomeItemTypeBatchUploadRecommendation,
};

// BookmarksHomeMediator manages model interactions for the
// BookmarksHomeViewController.
@interface BookmarksHomeMediator : NSObject

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

// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Designated initializer.
// `bookmarkModel` must not be `nullptr`. It must also be loaded.
// TODO(crbug.com/40251259): `browser`  need to be removed from
// `BookmarksHomeMediator`. A mediator should not be aware of this class.
- (instancetype)initWithBrowser:(Browser*)browser
                  bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
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

// Triggers batch upload of local bookmarks using the sync service.
- (void)triggerBatchUpload;

// Queries the sync service for the count of local bookmarks.
- (void)queryLocalBookmarks:(void (^)(int local_bookmarks_count,
                                      std::string user_email))completion;

// Returns weather the slashed cloud icon should be displayed for
// `bookmarkNode`.
- (BOOL)shouldDisplayCloudSlashIconWithBookmarkNode:
    (const bookmarks::BookmarkNode*)bookmarkNode;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_HOME_MEDIATOR_H_

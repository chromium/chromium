// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_SHARED_STATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_SHARED_STATE_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

@protocol BookmarkTableCellTitleEditing;
@class BookmarksHomeSharedState;
@class BookmarkTableCell;
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
bool IsABookmarkNodeSectionIdentifier(
    BookmarksHomeSectionIdentifier sectionIdentifier);

typedef NS_ENUM(NSInteger, BookmarksHomeItemType) {
  BookmarksHomeItemTypeHeader = kItemTypeEnumZero,
  BookmarksHomeItemTypePromo,
  BookmarksHomeItemTypeBookmark,
  BookmarksHomeItemTypeMessage,
};

@protocol BookmarksHomeSharedStateObserver
// Called when the set of edit nodes is cleared.
- (void)sharedStateDidClearEditNodes:(BookmarksHomeSharedState*)sharedState;
@end

// BookmarksHomeSharedState is a data structure that contains a number of fields
// that were previously ivars of BookmarkTableView. They are moved to a separate
// data structure in order to ease moving code between files.
@interface BookmarksHomeSharedState : NSObject

// Models.

// The model backing the table view.
@property(nonatomic, strong) TableViewModel* tableViewModel;

// The model holding profile bookmark data.
@property(nonatomic, assign, readonly)
    bookmarks::BookmarkModel* profileBookmarkModel;
// The model holding account bookmark data.
@property(nonatomic, assign, readonly)
    bookmarks::BookmarkModel* accountBookmarkModel;

// Views.

// The UITableView to show bookmarks.
@property(nonatomic, strong) UITableView* tableView;

// State variables.

// The BookmarkNode that is currently being displayed by the table view.  May be
// nil.
@property(nonatomic, assign)
    const bookmarks::BookmarkNode* tableViewDisplayedRootNode;

// If the table view is in edit mode.
@property(nonatomic, assign) BOOL currentlyInEditMode;

// If the table view showing search results.
@property(nonatomic, assign) BOOL currentlyShowingSearchResults;

// The set of nodes currently being edited.
@property(nonatomic, readonly, assign)
    std::set<const bookmarks::BookmarkNode*>& editNodes;

// The cell for the newly created folder while its name is being edited. Set
// to nil once the editing completes. Corresponds to `editingFolderNode`.
@property(nonatomic, weak)
    UITableViewCell<BookmarkTableCellTitleEditing>* editingFolderCell;

// The newly created folder node its name is being edited.
@property(nonatomic, assign) const bookmarks::BookmarkNode* editingFolderNode;

// True if the promo is visible.
@property(nonatomic, assign) BOOL promoVisible;

// This object can have at most one observer.
@property(nonatomic, weak) id<BookmarksHomeSharedStateObserver> observer;

// Constants

// Minimal acceptable favicon size, in points.
+ (CGFloat)minFaviconSizePt;

// Desired favicon size, in points.
+ (CGFloat)desiredFaviconSizePt;

// Minimium spacing between keyboard and the titleText when creating new folder,
// in points.
+ (CGFloat)keyboardSpacingPt;

// Max number of favicon download requests in the lifespan of this tableView.
+ (NSUInteger)maxDownloadFaviconCount;

// Initializes BookmarksHomeSharedState.
// `profileBookmarkModel` cannot be nullptr.
// `accountBookmarkModel` if kEnableBookmarksAccountStorage is enabled,
// the model cannot be nullptr,. Otherwise, it has to be nullptr.
// `displayedRootNode` is the displayed folder. cannot be nullptr.
- (instancetype)
    initWithProfileBookmarkModel:(bookmarks::BookmarkModel*)profileBookmarkModel
            accountBookmarkModel:(bookmarks::BookmarkModel*)accountBookmarkModel
               displayedRootNode:
                   (const bookmarks::BookmarkNode*)displayedRootNode
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_HOME_BOOKMARKS_HOME_SHARED_STATE_H_

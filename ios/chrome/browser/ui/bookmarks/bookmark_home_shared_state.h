// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_SHARED_STATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_SHARED_STATE_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/ui/list_model/list_model.h"

@protocol BookmarkTableCellTitleEditing;
@class BookmarkHomeSharedState;
@class BookmarkTableCell;
@class TableViewModel;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

typedef NS_ENUM(NSInteger, BookmarkHomeSectionIdentifier) {
  BookmarkHomeSectionIdentifierPromo = kSectionIdentifierEnumZero,
  BookmarkHomeSectionIdentifierBookmarks,
  BookmarkHomeSectionIdentifierMessages,
};

typedef NS_ENUM(NSInteger, BookmarkHomeItemType) {
  BookmarkHomeItemTypePromo = kItemTypeEnumZero,
  BookmarkHomeItemTypeBookmark,
  BookmarkHomeItemTypeMessage,
};

@protocol BookmarkHomeSharedStateObserver
// Called when the set of edit nodes is cleared.
- (void)sharedStateDidClearEditNodes:(BookmarkHomeSharedState*)sharedState;
@end

// BookmarkHomeSharedState is a data structure that contains a number of fields
// that were previously ivars of BookmarkTableView. They are moved to a separate
// data structure in order to ease moving code between files.
@interface BookmarkHomeSharedState : NSObject

// Models.

// The model backing the table view.
@property(nonatomic, strong) TableViewModel* tableViewModel;

// The model holding bookmark data.
@property(nonatomic, readonly, assign) bookmarks::BookmarkModel* bookmarkModel;

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

// If a new folder is being added currently.
@property(nonatomic, assign) BOOL addingNewFolder;

// The cell for the newly created folder while its name is being edited. Set
// to nil once the editing completes. Corresponds to `editingFolderNode`.
@property(nonatomic, weak)
    UITableViewCell<BookmarkTableCellTitleEditing>* editingFolderCell;

// The newly created folder node its name is being edited.
@property(nonatomic, assign) const bookmarks::BookmarkNode* editingFolderNode;

// Counts the number of favicon download requests from Google server in the
// lifespan of this tableView.
@property(nonatomic, assign) NSUInteger faviconDownloadCount;

// True if the promo is visible.
@property(nonatomic, assign) BOOL promoVisible;

// This object can have at most one observer.
@property(nonatomic, weak) id<BookmarkHomeSharedStateObserver> observer;

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

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                    displayedRootNode:
                        (const bookmarks::BookmarkNode*)displayedRootNode
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_SHARED_STATE_H_

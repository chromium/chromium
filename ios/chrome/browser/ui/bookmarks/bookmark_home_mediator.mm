// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_mediator.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_shared_state.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_promo_item.h"
#import "ios/chrome/browser/ui/bookmarks/synced_bookmarks_bridge.h"
#import "ios/chrome/browser/ui/signin_interaction/public/signin_presenter.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {
// Maximum number of entries to fetch when searching.
const int kMaxBookmarksSearchResults = 50;
}  // namespace

@interface BookmarkHomeMediator ()<BookmarkHomePromoItemDelegate,
                                   BookmarkModelBridgeObserver,
                                   BookmarkPromoControllerDelegate,
                                   SigninPresenter,
                                   SyncObserverModelBridge> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _modelBridge;

  // Observer to keep track of the signin and syncing status.
  std::unique_ptr<sync_bookmarks::SyncedBookmarksObserverBridge>
      _syncedBookmarksObserver;
}

// Shared state between Bookmark home classes.
@property(nonatomic, strong) BookmarkHomeSharedState* sharedState;

// The browser state for this mediator.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

@end

@implementation BookmarkHomeMediator
@synthesize bookmarkPromoController = _bookmarkPromoController;
@synthesize browserState = _browserState;
@synthesize consumer = _consumer;
@synthesize sharedState = _sharedState;

- (instancetype)initWithSharedState:(BookmarkHomeSharedState*)sharedState
                       browserState:(ios::ChromeBrowserState*)browserState {
  if ((self = [super init])) {
    _sharedState = sharedState;
    _browserState = browserState;
  }
  return self;
}

- (void)startMediating {
  DCHECK(self.consumer);
  DCHECK(self.sharedState);

  // Set up observers.
  _modelBridge = std::make_unique<bookmarks::BookmarkModelBridge>(
      self, self.sharedState.bookmarkModel);
  _syncedBookmarksObserver =
      std::make_unique<sync_bookmarks::SyncedBookmarksObserverBridge>(
          self, self.browserState);
  _bookmarkPromoController =
      [[BookmarkPromoController alloc] initWithBrowserState:self.browserState
                                                   delegate:self
                                                  presenter:self];

  [self computePromoTableViewData];
  [self computeBookmarkTableViewData];
}

- (void)disconnect {
  _modelBridge = nullptr;
  _syncedBookmarksObserver = nullptr;
  self.browserState = nullptr;
  self.consumer = nil;
  self.sharedState = nil;
}

#pragma mark - Initial Model Setup

// Computes the bookmarks table view based on the current root node.
- (void)computeBookmarkTableViewData {
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarkHomeSectionIdentifierBookmarks];
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarkHomeSectionIdentifierMessages];

  // Regenerate the list of all bookmarks.
  if (!self.sharedState.bookmarkModel->loaded() ||
      !self.sharedState.tableViewDisplayedRootNode) {
    [self updateTableViewBackground];
    return;
  }

  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    [self generateTableViewDataForRootNode];
    [self updateTableViewBackground];
    return;
  }
  [self generateTableViewData];
  [self updateTableViewBackground];
}

// Generate the table view data when the current root node is a child node.
- (void)generateTableViewData {
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
  // Add all bookmarks and folders of the current root node to the table.
  for (const auto& child :
       self.sharedState.tableViewDisplayedRootNode->children()) {
    BookmarkHomeNodeItem* nodeItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:child.get()];
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }
}

// Generate the table view data when the current root node is the outermost
// root.
- (void)generateTableViewDataForRootNode {
  // Add "Mobile Bookmarks" to the table.
  const BookmarkNode* mobileNode =
      self.sharedState.bookmarkModel->mobile_node();
  BookmarkHomeNodeItem* mobileItem =
      [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                    bookmarkNode:mobileNode];
  [self.sharedState.tableViewModel
                      addItem:mobileItem
      toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];

  // Add "Bookmarks Bar" and "Other Bookmarks" only when they are not empty.
  const BookmarkNode* bookmarkBar =
      self.sharedState.bookmarkModel->bookmark_bar_node();
  if (!bookmarkBar->children().empty()) {
    BookmarkHomeNodeItem* barItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:bookmarkBar];
    [self.sharedState.tableViewModel
                        addItem:barItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }

  const BookmarkNode* otherBookmarks =
      self.sharedState.bookmarkModel->other_node();
  if (!otherBookmarks->children().empty()) {
    BookmarkHomeNodeItem* otherItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:otherBookmarks];
    [self.sharedState.tableViewModel
                        addItem:otherItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  }
}

- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarkHomeSectionIdentifierBookmarks];
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarkHomeSectionIdentifierMessages];

  std::vector<const BookmarkNode*> nodes;
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(new base::string16);
  *query.word_phrase_query = base::SysNSStringToUTF16(searchText);
  GetBookmarksMatchingProperties(self.sharedState.bookmarkModel, query,
                                 kMaxBookmarksSearchResults, &nodes);

  int count = 0;
  for (const BookmarkNode* node : nodes) {
    BookmarkHomeNodeItem* nodeItem =
        [[BookmarkHomeNodeItem alloc] initWithType:BookmarkHomeItemTypeBookmark
                                      bookmarkNode:node];
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
    count++;
  }

  if (count == 0) {
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:BookmarkHomeItemTypeMessage];
    item.textAlignment = NSTextAlignmentLeft;
    item.textColor = UIColor.cr_labelColor;
    item.text = noResults;
    [self.sharedState.tableViewModel
                        addItem:item
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierMessages];
    return;
  }

  [self updateTableViewBackground];
}

- (void)updateTableViewBackground {
  // If the current root node is the outermost root, check if we need to show
  // the spinner backgound.  Otherwise, check if we need to show the empty
  // background.
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    if (self.sharedState.bookmarkModel->HasNoUserCreatedBookmarksOrFolders() &&
        _syncedBookmarksObserver->IsPerformingInitialSync()) {
      [self.consumer
          updateTableViewBackgroundStyle:BookmarkHomeBackgroundStyleLoading];
    } else {
      [self.consumer
          updateTableViewBackgroundStyle:BookmarkHomeBackgroundStyleDefault];
    }
    return;
  }

  if (![self hasBookmarksOrFolders] &&
      !self.sharedState.currentlyShowingSearchResults) {
    [self.consumer
        updateTableViewBackgroundStyle:BookmarkHomeBackgroundStyleEmpty];
  } else {
    [self.consumer
        updateTableViewBackgroundStyle:BookmarkHomeBackgroundStyleDefault];
  }
}

#pragma mark - Public

- (void)computePromoTableViewData {
  // We show promo cell only on the root view, that is when showing
  // the permanent nodes.
  BOOL promoVisible = ((self.sharedState.tableViewDisplayedRootNode ==
                        self.sharedState.bookmarkModel->root_node()) &&
                       self.bookmarkPromoController.shouldShowSigninPromo &&
                       !self.sharedState.currentlyShowingSearchResults);

  if (promoVisible == self.sharedState.promoVisible) {
    return;
  }
  self.sharedState.promoVisible = promoVisible;

  SigninPromoViewMediator* mediator = self.signinPromoViewMediator;
  if (self.sharedState.promoVisible) {
    DCHECK(![self.sharedState.tableViewModel
        hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierPromo]);
    [self.sharedState.tableViewModel
        insertSectionWithIdentifier:BookmarkHomeSectionIdentifierPromo
                            atIndex:0];
    BookmarkHomePromoItem* item =
        [[BookmarkHomePromoItem alloc] initWithType:BookmarkHomeItemTypePromo];
    item.delegate = self;
    [self.sharedState.tableViewModel
                        addItem:item
        toSectionWithIdentifier:BookmarkHomeSectionIdentifierPromo];
    [mediator signinPromoViewIsVisible];
  } else {
    if (!mediator.invalidClosedOrNeverVisible) {
      // When the sign-in view is closed, the promo state changes, but
      // -[SigninPromoViewMediator signinPromoViewIsHidden] should not be
      // called.
      [mediator signinPromoViewIsHidden];
    }

    DCHECK([self.sharedState.tableViewModel
        hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierPromo]);
    [self.sharedState.tableViewModel
        removeSectionWithIdentifier:BookmarkHomeSectionIdentifierPromo];
  }
  [self.sharedState.tableView reloadData];
}

#pragma mark - BookmarkModelBridgeObserver Callbacks

// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded {
  [self.consumer refreshContents];
}

// The node has changed, but not its children.
- (void)bookmarkNodeChanged:(const BookmarkNode*)bookmarkNode {
  // The root folder changed. Do nothing.
  if (bookmarkNode == self.sharedState.tableViewDisplayedRootNode) {
    return;
  }

  // A specific cell changed. Reload, if currently shown.
  if ([self itemForNode:bookmarkNode] != nil) {
    [self.consumer refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // In search mode, we want to refresh any changes (like undo).
  if (self.sharedState.currentlyShowingSearchResults) {
    [self.consumer refreshContents];
  }
  // The current root folder's children changed. Reload everything.
  // (When adding new folder, table is already been updated. So no need to
  // reload here.)
  if (bookmarkNode == self.sharedState.tableViewDisplayedRootNode &&
      !self.sharedState.addingNewFolder) {
    [self.consumer refreshContents];
    return;
  }
}

// The node has moved to a new parent folder.
- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  if (oldParent == self.sharedState.tableViewDisplayedRootNode ||
      newParent == self.sharedState.tableViewDisplayedRootNode) {
    // A folder was added or removed from the current root folder.
    [self.consumer refreshContents];
  }
}

// |node| was deleted from |folder|.
- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
                 fromFolder:(const BookmarkNode*)folder {
  if (self.sharedState.currentlyShowingSearchResults) {
    [self.consumer refreshContents];
  } else if (self.sharedState.tableViewDisplayedRootNode == node) {
    self.sharedState.tableViewDisplayedRootNode = NULL;
    [self.consumer refreshContents];
  }
}

// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes {
  // TODO(crbug.com/695749) Check if this case is applicable in the new UI.
}

- (void)bookmarkNodeFaviconChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  // Only urls have favicons.
  DCHECK(bookmarkNode->is_url());

  // Update image of corresponding cell.
  BookmarkHomeNodeItem* nodeItem = [self itemForNode:bookmarkNode];
  if (!nodeItem) {
    return;
  }

  // Check that this cell is visible.
  NSIndexPath* indexPath =
      [self.sharedState.tableViewModel indexPathForItem:nodeItem];
  NSArray* visiblePaths = [self.sharedState.tableView indexPathsForVisibleRows];
  if (![visiblePaths containsObject:indexPath]) {
    return;
  }

  // Get the favicon from cache directly. (no need to fetch from server)
  [self.consumer loadFaviconAtIndexPath:indexPath fallbackToGoogleServer:NO];
}

- (BookmarkHomeNodeItem*)itemForNode:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:BookmarkHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    if (item.type == BookmarkHomeItemTypeBookmark) {
      BookmarkHomeNodeItem* nodeItem =
          base::mac::ObjCCastStrict<BookmarkHomeNodeItem>(item);
      if (nodeItem.bookmarkNode == bookmarkNode) {
        return nodeItem;
      }
    }
  }
  return nil;
}

#pragma mark - BookmarkHomePromoItemDelegate

- (SigninPromoViewMediator*)signinPromoViewMediator {
  return self.bookmarkPromoController.signinPromoViewMediator;
}

#pragma mark - BookmarkPromoControllerDelegate

- (void)promoStateChanged:(BOOL)promoEnabled {
  [self computePromoTableViewData];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  if (![self.sharedState.tableViewModel
          hasSectionForSectionIdentifier:BookmarkHomeSectionIdentifierPromo]) {
    return;
  }

  NSIndexPath* indexPath = [self.sharedState.tableViewModel
      indexPathForItemType:BookmarkHomeItemTypePromo
         sectionIdentifier:BookmarkHomeSectionIdentifierPromo];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                          atIndexPath:indexPath
                                      forceReloadCell:identityChanged];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  // Proxy this call along to the consumer.
  [self.consumer showSignin:command];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  // Permanent nodes ("Bookmarks Bar", "Other Bookmarks") at the root node might
  // be added after syncing.  So we need to refresh here.
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.bookmarkModel->root_node()) {
    [self.consumer refreshContents];
    return;
  }
  [self updateTableViewBackground];
}

#pragma mark - Private Helpers

- (BOOL)hasBookmarksOrFolders {
  return self.sharedState.tableViewDisplayedRootNode &&
         !self.sharedState.tableViewDisplayedRootNode->children().empty();
}

// Delete all items for the given |sectionIdentifier| section, or create it
// if it doesn't exist, hence ensuring the section exists and is empty.
- (void)deleteAllItemsOrAddSectionWithIdentifier:(NSInteger)sectionIdentifier {
  if ([self.sharedState.tableViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    [self.sharedState.tableViewModel
        deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
  } else {
    [self.sharedState.tableViewModel
        addSectionWithIdentifier:sectionIdentifier];
  }
}

@end

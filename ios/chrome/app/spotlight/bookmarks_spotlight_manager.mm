// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"

#include <memory>

#import <CoreSpotlight/CoreSpotlight.h>

#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Limit the size of the initial indexing. This will not limit the size of the
// index as new bookmarks can be added afterwards.
const int kMaxInitialIndexSize = 1000;

// Minimum delay between two global indexing of bookmarks.
const int kDelayBetweenTwoIndexingInSeconds = 7 * 86400;  // One week.

}  // namespace

class SpotlightBookmarkModelBridge;

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface BookmarksSpotlightManager () {
  __weak id<BookmarkUpdatedDelegate> _delegate;

  // Bridge to register for bookmark changes.
  std::unique_ptr<SpotlightBookmarkModelBridge> _bookmarkModelBridge;

  // Keep a reference to detach before deallocing. Life cycle of _bookmarkModel
  // is longer than life cycle of a SpotlightManager as
  // |BookmarkModelBeingDeleted| will cause deletion of SpotlightManager.
  bookmarks::BookmarkModel* _bookmarkModel;  // weak

  // Number of nodes indexed in initial scan.
  NSUInteger _nodesIndexed;

  // Tracks whether initial indexing has been done.
  BOOL _initialIndexDone;
}

// Detaches the |SpotlightBookmarkModelBridge| from the bookmark model. The
// manager must not be used after calling this method.
- (void)detachBookmarkModel;

// Removes the node from the Spotlight index.
- (void)removeNodeFromIndex:(const bookmarks::BookmarkNode*)node;

// Clears all the bookmarks in the Spotlight index then index the bookmarks in
// the model.
- (void)clearAndReindexModel;

// Refreshes all nodes in the subtree of node.
// If |initial| is YES, limit the number of nodes to kMaxInitialIndexSize.
- (void)refreshNodeInIndex:(const bookmarks::BookmarkNode*)node
                   initial:(BOOL)initial;

// Returns true is the current index is too old or from an incompatible version.
- (BOOL)shouldReindex;

@end

// Handles notification that bookmarks has been removed changed so we can update
// the Spotlight index.
class SpotlightBookmarkModelBridge : public bookmarks::BookmarkModelObserver {
 public:
  explicit SpotlightBookmarkModelBridge(BookmarksSpotlightManager* owner)
      : owner_(owner) {}

  ~SpotlightBookmarkModelBridge() override {}

  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override {}

  void OnWillRemoveBookmarks(bookmarks::BookmarkModel* model,
                             const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node) override {
    [owner_ removeNodeFromIndex:node];
  }

  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override {
    [owner_ detachBookmarkModel];
  }

  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override {
    [owner_ reindexBookmarksIfNeeded];
  }

  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override {
    [owner_ refreshNodeInIndex:parent->children()[index].get() initial:NO];
  }

  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override {
    [owner_ removeNodeFromIndex:node];
  }

  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override {
    [owner_ refreshNodeInIndex:node initial:NO];
  }

  void BookmarkNodeFaviconChanged(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {
    [owner_ refreshNodeInIndex:node initial:NO];
  }

  void BookmarkAllUserNodesRemoved(
      bookmarks::BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {
    [owner_ clearAllSpotlightItems:nil];
  }

  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override {
    [owner_ refreshNodeInIndex:new_parent->children()[new_index].get()
                       initial:NO];
  }

 private:
  __weak BookmarksSpotlightManager* owner_;
};

@implementation BookmarksSpotlightManager

+ (BookmarksSpotlightManager*)bookmarksSpotlightManagerWithBrowserState:
    (ios::ChromeBrowserState*)browserState {
  return [[BookmarksSpotlightManager alloc]
      initWithLargeIconService:IOSChromeLargeIconServiceFactory::
                                   GetForBrowserState(browserState)
                 bookmarkModel:ios::BookmarkModelFactory::GetForBrowserState(
                                   browserState)];
}

- (instancetype)
initWithLargeIconService:(favicon::LargeIconService*)largeIconService
           bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  self = [super initWithLargeIconService:largeIconService
                                  domain:spotlight::DOMAIN_BOOKMARKS];
  if (self) {
    _bookmarkModelBridge.reset(new SpotlightBookmarkModelBridge(self));
    _bookmarkModel = bookmarkModel;
    bookmarkModel->AddObserver(_bookmarkModelBridge.get());
  }
  return self;
}

- (void)detachBookmarkModel {
  [self cancelAllLargeIconPendingTasks];
  if (_bookmarkModelBridge.get()) {
    _bookmarkModel->RemoveObserver(_bookmarkModelBridge.get());
    _bookmarkModelBridge.reset();
  }
}

- (id<BookmarkUpdatedDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<BookmarkUpdatedDelegate>)delegate {
  _delegate = delegate;
}

- (void)getParentKeywordsForNode:(const bookmarks::BookmarkNode*)node
                         inArray:(NSMutableArray*)keywords {
  if (!node) {
    return;
  }
  if (node->is_folder() && !_bookmarkModel->is_permanent_node(node)) {
    [keywords addObject:base::SysUTF16ToNSString(node->GetTitle())];
  }
  [self getParentKeywordsForNode:node->parent() inArray:keywords];
}

- (void)removeNodeFromIndex:(const bookmarks::BookmarkNode*)node {
  if (node->is_url()) {
    GURL url(node->url());
    NSString* title = base::SysUTF16ToNSString(node->GetTitle());
    NSString* spotlightID = [self spotlightIDForURL:url title:title];
    __weak BookmarksSpotlightManager* weakself = self;
    BlockWithError completion = ^(NSError* error) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakself refreshItemsWithURL:url title:nil];
        [_delegate bookmarkUpdated];
      });
    };
    spotlight::DeleteItemsWithIdentifiers(@[ spotlightID ], completion);
    return;
  }
  for (const auto& child : node->children())
    [self removeNodeFromIndex:child.get()];
}

- (BOOL)shouldReindex {
  NSDate* date = [[NSUserDefaults standardUserDefaults]
      objectForKey:@(spotlight::kSpotlightLastIndexingDateKey)];
  if (!date) {
    return YES;
  }
  NSDate* expirationDate =
      [date dateByAddingTimeInterval:kDelayBetweenTwoIndexingInSeconds];
  if ([expirationDate compare:[NSDate date]] == NSOrderedAscending) {
    return YES;
  }
  NSNumber* lastIndexedVersionString = [[NSUserDefaults standardUserDefaults]
      objectForKey:@(spotlight::kSpotlightLastIndexingVersionKey)];
  if (!lastIndexedVersionString) {
    return YES;
  }

  if ([lastIndexedVersionString integerValue] <
      spotlight::kCurrentSpotlightIndexVersion) {
    return YES;
  }
  return NO;
}

- (void)reindexBookmarksIfNeeded {
  if (!_bookmarkModel->loaded() || _initialIndexDone) {
    return;
  }
  _initialIndexDone = YES;
  if ([self shouldReindex]) {
    [self clearAndReindexModel];
  }
}

- (void)addKeywords:(NSArray*)keywords
    toSearchableItem:(CSSearchableItem*)item {
  NSSet* itemKeywords = [NSSet setWithArray:[[item attributeSet] keywords]];
  itemKeywords = [itemKeywords setByAddingObjectsFromArray:keywords];
  [[item attributeSet] setKeywords:[itemKeywords allObjects]];
}

- (void)refreshNodeInIndex:(const bookmarks::BookmarkNode*)node
                   initial:(BOOL)initial {
  if (initial && _nodesIndexed > kMaxInitialIndexSize) {
    return;
  }
  if (node->is_url()) {
    _nodesIndexed++;
    [self refreshItemsWithURL:node->url() title:nil];
    if (!initial) {
      [_delegate bookmarkUpdated];
    }
    return;
  }
  for (const auto& child : node->children())
    [self refreshNodeInIndex:child.get() initial:initial];
}

- (void)shutdown {
  [self detachBookmarkModel];
  [super shutdown];
}

- (NSArray*)spotlightItemsWithURL:(const GURL&)URL
                          favicon:(UIImage*)favicon
                     defaultTitle:(NSString*)defaultTitle {
  NSMutableDictionary* spotlightItems = [[NSMutableDictionary alloc] init];
  std::vector<const bookmarks::BookmarkNode*> nodes;
  _bookmarkModel->GetNodesByURL(URL, &nodes);
  for (auto* node : nodes) {
    NSString* nodeTitle = base::SysUTF16ToNSString(node->GetTitle());
    NSString* spotlightID = [self spotlightIDForURL:URL title:nodeTitle];
    CSSearchableItem* item = [spotlightItems objectForKey:spotlightID];
    if (!item) {
      item = [[super spotlightItemsWithURL:URL
                                   favicon:favicon
                              defaultTitle:nodeTitle] objectAtIndex:0];
    }
    NSMutableArray* nodeKeywords = [[NSMutableArray alloc] init];
    [self getParentKeywordsForNode:node inArray:nodeKeywords];
    [self addKeywords:nodeKeywords toSearchableItem:item];
    [spotlightItems setObject:item forKey:spotlightID];
  }
  return [spotlightItems allValues];
}

- (void)clearAndReindexModel {
  [self cancelAllLargeIconPendingTasks];
  __weak BookmarksSpotlightManager* weakself = self;
  BlockWithError completion = ^(NSError* error) {
    if (!error) {
      dispatch_async(dispatch_get_main_queue(), ^{
        BookmarksSpotlightManager* strongSelf = weakself;
        if (!strongSelf)
          return;

        NSDate* startOfReindexing = [NSDate date];
        strongSelf->_nodesIndexed = 0;
        [strongSelf refreshNodeInIndex:strongSelf->_bookmarkModel->root_node()
                               initial:YES];
        NSDate* endOfReindexing = [NSDate date];
        NSTimeInterval indexingDuration =
            [endOfReindexing timeIntervalSinceDate:startOfReindexing];
        UMA_HISTOGRAM_TIMES(
            "IOS.Spotlight.BookmarksIndexingDuration",
            base::TimeDelta::FromMillisecondsD(1000 * indexingDuration));
        UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.BookmarksInitialIndexSize",
                                  [strongSelf pendingLargeIconTasksCount]);
        [[NSUserDefaults standardUserDefaults]
            setObject:endOfReindexing
               forKey:@(spotlight::kSpotlightLastIndexingDateKey)];

        [[NSUserDefaults standardUserDefaults]
            setObject:[NSNumber numberWithInteger:
                                    spotlight::kCurrentSpotlightIndexVersion]
               forKey:@(spotlight::kSpotlightLastIndexingVersionKey)];
        [_delegate bookmarkUpdated];
      });
    }
  };
  [self clearAllSpotlightItems:completion];
}

@end

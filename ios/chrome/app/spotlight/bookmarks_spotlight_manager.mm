// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"

#import <memory>

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"

namespace {
// Limit the size of the initial indexing. This will not limit the size of the
// index as new bookmarks can be added afterwards.
const int kMaxInitialIndexSize = 1000;

// Minimum delay between two global indexing of bookmarks.
const base::TimeDelta kDelayBetweenTwoIndexing = base::Days(7);

}  // namespace

class SpotlightBookmarkModelBridge;

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface BookmarksSpotlightManager ()

// Detaches the `SpotlightBookmarkModelBridge` from the bookmark model. The
// manager must not be used after calling this method.
- (void)detachBookmarkModel;

// Removes the node from the Spotlight index.
- (void)removeNodeFromIndex:(const bookmarks::BookmarkNode*)node;

// Refreshes all nodes in the subtree of node.
// If `initial` is YES, limit the number of nodes to kMaxInitialIndexSize.
- (void)refreshNodeInIndex:(const bookmarks::BookmarkNode*)node;

// Returns true is the current index is too old or from an incompatible version.
- (BOOL)shouldReindex;

// Clears all bookmark items in spotlight.
- (void)clearAllBookmarkSpotlightItems:(BlockWithError)completionHandler;

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
                         size_t index,
                         bool added_by_user) override {
    [owner_ refreshNodeInIndex:parent->children()[index].get()];
  }

  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override {
    [owner_ removeNodeFromIndex:node];
  }

  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override {
    [owner_ refreshNodeInIndex:node];
  }

  void BookmarkNodeFaviconChanged(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {
    [owner_ refreshNodeInIndex:node];
  }

  void BookmarkAllUserNodesRemoved(
      bookmarks::BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {
    [owner_ clearAllBookmarkSpotlightItems:nil];
  }

  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}

  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override {
    [owner_ refreshNodeInIndex:new_parent->children()[new_index].get()];
  }

 private:
  __weak BookmarksSpotlightManager* owner_;
};

@implementation BookmarksSpotlightManager {
  // Bridge to register for bookmark changes.
  std::unique_ptr<SpotlightBookmarkModelBridge> _bookmarkModelBridge;

  // Keep a reference to detach before deallocing. Life cycle of _bookmarkModel
  // is longer than life cycle of a SpotlightManager as
  // `BookmarkModelBeingDeleted` will cause deletion of SpotlightManager.
  bookmarks::BookmarkModel* _bookmarkModel;  // weak

  // Number of nodes indexed in initial scan.
  NSUInteger _nodesIndexed;

  // Tracks whether initial indexing has been done.
  BOOL _initialIndexDone;
}

+ (BookmarksSpotlightManager*)bookmarksSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(browserState);

  return [[BookmarksSpotlightManager alloc]
      initWithLargeIconService:largeIconService
                 bookmarkModel:ios::LocalOrSyncableBookmarkModelFactory::
                                   GetForBrowserState(browserState)
            spotlightInterface:[SpotlightInterface defaultInterface]
         searchableItemFactory:
             [[SearchableItemFactory alloc]
                 initWithLargeIconService:largeIconService
                                   domain:spotlight::DOMAIN_BOOKMARKS
                    useTitleInIdentifiers:YES]];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super initWithSpotlightInterface:spotlightInterface
                     searchableItemFactory:searchableItemFactory];
  if (self) {
    _pendingLargeIconTasksCount = 0;
    _bookmarkModelBridge.reset(new SpotlightBookmarkModelBridge(self));
    _bookmarkModel = bookmarkModel;
    bookmarkModel->AddObserver(_bookmarkModelBridge.get());
  }
  return self;
}

- (void)detachBookmarkModel {
  if (_bookmarkModelBridge.get()) {
    _bookmarkModel->RemoveObserver(_bookmarkModelBridge.get());
    _bookmarkModelBridge.reset();
  }
}

- (void)clearAllBookmarkSpotlightItems:(BlockWithError)completionHandler {
  [self.searchableItemFactory cancelItemsGeneration];
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_BOOKMARKS)
      ]
                               completionHandler:completionHandler];
}

- (NSMutableArray*)parentFolderNamesForNode:
    (const bookmarks::BookmarkNode*)node {
  if (!node) {
    return [[NSMutableArray alloc] init];
  }

  NSMutableArray* parentNames = [self parentFolderNamesForNode:node->parent()];

  if (node->is_folder() && !_bookmarkModel->is_permanent_node(node)) {
    [parentNames addObject:base::SysUTF16ToNSString(node->GetTitle())];
  }

  return parentNames;
}

- (void)removeNodeFromIndex:(const bookmarks::BookmarkNode*)node {
  if (node->is_url()) {
    [self removeURLNodeFromIndex:node];
    return;
  }
  for (const auto& child : node->children())
    [self removeNodeFromIndex:child.get()];
}

// Helper to remove URL nodes at the leaves of the bookmark index.
- (void)removeURLNodeFromIndex:(const bookmarks::BookmarkNode*)node {
  DCHECK(node->is_url());
  const GURL URL(node->url());
  NSString* title = base::SysUTF16ToNSString(node->GetTitle());
  NSString* spotlightID = [self.searchableItemFactory spotlightIDForURL:URL
                                                                  title:title];
  __weak BookmarksSpotlightManager* weakSelf = self;
  [self.spotlightInterface
      deleteSearchableItemsWithIdentifiers:@[ spotlightID ]
                         completionHandler:^(NSError*) {
                           dispatch_async(dispatch_get_main_queue(), ^{
                             [weakSelf onCompletedDeleteItemsWithURL:URL
                                                               title:title];
                           });
                         }];
}

// Completion helper for URL node deletion.
- (void)onCompletedDeleteItemsWithURL:(const GURL&)URL title:(NSString*)title {
  [self refreshItemWithURL:URL title:title];
}

- (BOOL)shouldReindex {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];

  NSDate* date = base::apple::ObjCCast<NSDate>(
      [userDefaults objectForKey:@(spotlight::kSpotlightLastIndexingDateKey)]);
  if (!date) {
    return YES;
  }
  const base::TimeDelta timeSinceLastIndexing =
      base::Time::Now() - base::Time::FromNSDate(date);
  if (timeSinceLastIndexing >= kDelayBetweenTwoIndexing) {
    return YES;
  }
  NSNumber* lastIndexedVersion = base::apple::ObjCCast<NSNumber>([userDefaults
      objectForKey:@(spotlight::kSpotlightLastIndexingVersionKey)]);
  if (!lastIndexedVersion) {
    return YES;
  }

  if ([lastIndexedVersion integerValue] <
      spotlight::kCurrentSpotlightIndexVersion) {
    return YES;
  }
  return NO;
}

- (void)reindexBookmarksIfNeeded {
  if (self.isShuttingDown) {
    return;
  }
  if (!_bookmarkModel->loaded() || _initialIndexDone) {
    return;
  }
  _initialIndexDone = YES;
  if ([self shouldReindex]) {
    [self clearAndReindexModel];
  }
}

// Refresh any bookmark nodes matching given URL and title. If there are
// multiple nodes with same URL and title, they will be merged into a single
// spotlight item but will have tags from each of the bookmrk nodes.
- (void)refreshItemWithURL:(const GURL&)URL title:(NSString*)title {
  if (self.isShuttingDown) {
    return;
  }

  std::vector<const bookmarks::BookmarkNode*> nodesMatchingURL =
      _bookmarkModel->GetNodesByURL(URL);

  NSMutableArray* itemKeywords = [[NSMutableArray alloc] init];

  // If there are no bookmarks nodes matching the url and title then we should
  // make sure to not create and index a spotlight item with the given url and
  // title.
  BOOL shouldIndexItem = false;

  // Build a list of tags for every node having the URL and title. Combine the
  // lists of tags into one, that will be used to search for the spotlight item.
  for (const bookmarks::BookmarkNode* node : nodesMatchingURL) {
    NSString* nodeTitle = base::SysUTF16ToNSString(node->GetTitle());
    if ([nodeTitle isEqualToString:title] == NO) {
      continue;
    }
    /// there still a bookmark node that matches the  given URL and title, so we
    /// should refresh/reindex it in spotlight.
    shouldIndexItem = true;

    [itemKeywords addObjectsFromArray:[self parentFolderNamesForNode:node]];
  }

  if (shouldIndexItem) {
    __weak BookmarksSpotlightManager* weakSelf = self;

    _pendingLargeIconTasksCount++;
    [self.searchableItemFactory
        generateSearchableItem:URL
                         title:title
            additionalKeywords:itemKeywords
             completionHandler:^(CSSearchableItem* item) {
               weakSelf.pendingLargeIconTasksCount--;
               [weakSelf.spotlightInterface indexSearchableItems:@[ item ]];
             }];
  }
}

- (void)refreshNodeInIndex:(const bookmarks::BookmarkNode*)node {
  if (self.isShuttingDown) {
    return;
  }
  if (_nodesIndexed > kMaxInitialIndexSize) {
    return;
  }
  if (node->is_url()) {
    _nodesIndexed++;
    [self refreshItemWithURL:node->url()
                       title:base::SysUTF16ToNSString(node->GetTitle())];
    return;
  }
  for (const auto& child : node->children())
    [self refreshNodeInIndex:child.get()];
}

- (void)shutdown {
  [super shutdown];
  [self detachBookmarkModel];
}

- (void)clearAndReindexModel {
  __weak BookmarksSpotlightManager* weakSelf = self;
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_BOOKMARKS)
      ]
                               completionHandler:^(NSError* error) {
                                 if (error) {
                                   [SpotlightLogger logSpotlightError:error];
                                   return;
                                 }
                                 [weakSelf completedClearAllSpotlightItems];
                               }];
}

- (void)completedClearAllSpotlightItems {
  if (self.isShuttingDown) {
    return;
  }

  // If this method is called before bookmark model loaded, or after it
  // unloaded, reindexing won't be possible. The latter should happen at
  // shutdown, so the reindex can't happen until next app start. In the former
  // case, unset _initialIndexDone flag. This makes sure indexing will happen
  // once the model loads.
  if (!_bookmarkModel->loaded()) {
    _initialIndexDone = NO;
  }

  const base::Time startOfReindexing = base::Time::Now();
  _nodesIndexed = 0;
  _pendingLargeIconTasksCount = 0;
  [self refreshNodeInIndex:_bookmarkModel->root_node()];
  const base::Time endOfReindexing = base::Time::Now();

  UMA_HISTOGRAM_TIMES("IOS.Spotlight.BookmarksIndexingDuration",
                      endOfReindexing - startOfReindexing);
  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.BookmarksInitialIndexSize",
                            _pendingLargeIconTasksCount);

  [[NSUserDefaults standardUserDefaults]
      setObject:endOfReindexing.ToNSDate()
         forKey:@(spotlight::kSpotlightLastIndexingDateKey)];

  [[NSUserDefaults standardUserDefaults]
      setObject:@(spotlight::kCurrentSpotlightIndexVersion)
         forKey:@(spotlight::kSpotlightLastIndexingVersionKey)];
}

@end

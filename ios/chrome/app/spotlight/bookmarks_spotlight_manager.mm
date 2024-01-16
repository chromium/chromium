// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"

#import <memory>
#import <stack>

#import <CoreSpotlight/CoreSpotlight.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "base/version.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"

namespace {
// Limit the size of the initial indexing. This will not limit the size of the
// index as new bookmarks can be added afterwards.
const int kMaxInitialIndexSize = 1000;

// At initial indexing, the # of bookmarks to index per batch before releasing
// the main queue.
const int kBatchSize = 100;

// Minimum delay between two global indexing of bookmarks.
const base::TimeDelta kDelayBetweenTwoIndexing = base::Days(7);

}  // namespace

class SpotlightBookmarkModelBridge;

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface BookmarksSpotlightManager () <BookmarkModelBridgeObserver>

//  This flag is used when rebuilding the spotlight index.
@property(nonatomic, assign) BOOL modelUpdatesShouldCauseFullReindex;
//  This flag is used when rebuilding the spotlight index.
@property(nonatomic, assign) BOOL modelUpdatesShouldBeIgnored;

// The operation for processing the next batch of bookmarks from the indexing
// queue, if any.
@property(nonatomic, weak) NSOperation* nextBatchOperation;

@end

@implementation BookmarksSpotlightManager {
  // Bridge to register for local or syncable bookmark model changes.
  std::unique_ptr<BookmarkModelBridge> _localOrSyncableBookmarkModelBridge;
  // Bridge to register for account bookmark model changes.
  std::unique_ptr<BookmarkModelBridge> _accountBookmarkModelBridge;

  // Keep a reference to detach before deallocing.
  bookmarks::BookmarkModel* _localOrSyncableBookmarkModel;  // weak
  // `_accountBookmarkModel` can be `nullptr`.
  bookmarks::BookmarkModel* _accountBookmarkModel;  // weak

  // Number of nodes indexed in initial scan.
  NSUInteger _nodesIndexed;

  // Tracks whether initial indexing has been done.
  BOOL _initialIndexDone;

  // Timer that counts how long it takes to index all bookmarks.
  std::unique_ptr<base::ElapsedTimer> _initialIndexTimer;

  // The nodes stored in this queue will be indexed.
  std::stack<const bookmarks::BookmarkNode*> _indexingStack;

  // Number of times the indexing was interrupted by model updates.
  NSInteger _reindexInterruptionCount;
}

+ (BookmarksSpotlightManager*)bookmarksSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(browserState);

  return [[BookmarksSpotlightManager alloc]
          initWithLargeIconService:largeIconService
      localOrSyncableBookmarkModel:ios::LocalOrSyncableBookmarkModelFactory::
                                       GetForBrowserState(browserState)
              accountBookmarkModel:ios::AccountBookmarkModelFactory::
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
    localOrSyncableBookmarkModel:
        (bookmarks::BookmarkModel*)localOrSyncableBookmarkModel
            accountBookmarkModel:(bookmarks::BookmarkModel*)accountBookmarkModel
              spotlightInterface:(SpotlightInterface*)spotlightInterface
           searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super initWithSpotlightInterface:spotlightInterface
                     searchableItemFactory:searchableItemFactory];
  if (self) {
    _pendingLargeIconTasksCount = 0;
    _localOrSyncableBookmarkModel = localOrSyncableBookmarkModel;
    _accountBookmarkModel = accountBookmarkModel;
    [self attachBookmarkModel];
  }
  return self;
}

- (void)attachBookmarkModel {
  if (_localOrSyncableBookmarkModel) {
    _localOrSyncableBookmarkModelBridge = std::make_unique<BookmarkModelBridge>(
        self, _localOrSyncableBookmarkModel);
  }
  if (_accountBookmarkModel) {
    _accountBookmarkModelBridge =
        std::make_unique<BookmarkModelBridge>(self, _accountBookmarkModel);
  }
}

// Detaches the `SpotlightBookmarkModelBridge` from the bookmark model. The
// manager must not be used after calling this method unless attachBookmarkModel
// is called.
- (void)detachBookmarkModel {
  if (_localOrSyncableBookmarkModelBridge.get()) {
    _localOrSyncableBookmarkModel->RemoveObserver(
        _localOrSyncableBookmarkModelBridge.get());
    _localOrSyncableBookmarkModelBridge.reset();
  }
  if (_accountBookmarkModelBridge.get()) {
    _accountBookmarkModel->RemoveObserver(_accountBookmarkModelBridge.get());
    _accountBookmarkModelBridge.reset();
  }
}

// Clears all bookmark items in spotlight.
- (void)clearAllBookmarkSpotlightItems {
  [self.searchableItemFactory cancelItemsGeneration];
  [self.spotlightInterface deleteSearchableItemsWithDomainIdentifiers:@[
    spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_BOOKMARKS)
  ]
                                                    completionHandler:nil];
}

- (NSMutableArray*)parentFolderNamesForNode:
    (const bookmarks::BookmarkNode*)node {
  if (!node) {
    return [[NSMutableArray alloc] init];
  }

  NSMutableArray* parentNames = [self parentFolderNamesForNode:node->parent()];
  bookmarks::BookmarkModel* parentModel = [self bookmarkModelForNode:node];

  if (node->is_folder() && !parentModel->is_permanent_node(node)) {
    [parentNames addObject:base::SysUTF16ToNSString(node->GetTitle())];
  }

  return parentNames;
}

// Removes the node from the Spotlight index.
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

// Returns true is the current index is too old or from an incompatible version.
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
  if (self.isShuttingDown || _initialIndexDone) {
    return;
  }
  if (!_localOrSyncableBookmarkModel->loaded()) {
    return;
  }
  if (_accountBookmarkModel && !_accountBookmarkModel->loaded()) {
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
      [self nodesByURL:URL];

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
    // there still a bookmark node that matches the  given URL and title, so we
    // should refresh/reindex it in spotlight.
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

// Refreshes all nodes in the subtree of node.
- (void)refreshNodeInIndex:(const bookmarks::BookmarkNode*)node {
  _indexingStack.push(node);
  if (!self.nextBatchOperation) {
    [self indexNextBatchInQueue];
  }
}

- (void)indexNextBatchInQueue {
  self.nextBatchOperation = nil;

  if (self.isShuttingDown) {
    [self stopIndexing];
    return;
  }

  for (int i = 0; i < kBatchSize; i++) {
    if (_indexingStack.empty() || _nodesIndexed > kMaxInitialIndexSize) {
      self.modelUpdatesShouldCauseFullReindex = NO;
      [self logInitialIndexComplete];
      [self stopIndexing];
      return;
    }

    const bookmarks::BookmarkNode* node = _indexingStack.top();
    _indexingStack.pop();

    if (node->is_url()) {
      _nodesIndexed++;
      [self refreshItemWithURL:node->url()
                         title:base::SysUTF16ToNSString(node->GetTitle())];
    } else {
      for (auto it = node->children().rbegin(); it != node->children().rend();
           ++it) {
        _indexingStack.push(it->get());
      }
    }
  }

  // Dispatch the next batch asynchronously to avoid blocking the main thread.
  __weak BookmarksSpotlightManager* weakSelf = self;
  NSOperation* nextBatchOperation = [NSBlockOperation blockOperationWithBlock:^{
    [weakSelf indexNextBatchInQueue];
  }];

  [[NSOperationQueue mainQueue] addOperation:nextBatchOperation];
  self.nextBatchOperation = nextBatchOperation;
}

- (void)shutdown {
  [super shutdown];
  [self detachBookmarkModel];
}

- (void)clearAndReindexModel {
  [self stopIndexing];

  self.modelUpdatesShouldBeIgnored = YES;
  self.modelUpdatesShouldCauseFullReindex = NO;
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
  self.modelUpdatesShouldBeIgnored = NO;
  self.modelUpdatesShouldCauseFullReindex = YES;

  // Indexing queue should be empty. There should be no ongoing indexing
  // operations.
  DCHECK(_indexingStack.empty());
  DCHECK(!self.nextBatchOperation);
  DCHECK(self.modelUpdatesShouldCauseFullReindex);

  // If this method is called before bookmark model loaded, or after it
  // unloaded, reindexing won't be possible. The latter should happen at
  // shutdown, so the reindex can't happen until next app start. In the former
  // case, unset _initialIndexDone flag. This makes sure indexing will happen
  // once the model loads.
  if (_localOrSyncableBookmarkModel &&
      !_localOrSyncableBookmarkModel->loaded()) {
    _initialIndexDone = NO;
  }
  if (_accountBookmarkModel && !_accountBookmarkModel->loaded()) {
    _initialIndexDone = NO;
  }

  _nodesIndexed = 0;
  _pendingLargeIconTasksCount = 0;
  if (_localOrSyncableBookmarkModel) {
    _indexingStack.push(_localOrSyncableBookmarkModel->root_node());
  }
  if (_accountBookmarkModel) {
    _indexingStack.push(_accountBookmarkModel->root_node());
  }
  _initialIndexTimer = std::make_unique<base::ElapsedTimer>();
  [self indexNextBatchInQueue];

  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.BookmarksInitialIndexSize",
                            _pendingLargeIconTasksCount);
}

- (bookmarks::BookmarkModel*)bookmarkModelForNode:
    (const bookmarks::BookmarkNode*)node {
  if (node->HasAncestor(_localOrSyncableBookmarkModel->root_node())) {
    return _localOrSyncableBookmarkModel;
  }
  DCHECK(_accountBookmarkModel &&
         node->HasAncestor(_accountBookmarkModel->root_node()));
  return _accountBookmarkModel;
}

- (std::vector<const bookmarks::BookmarkNode*>)nodesByURL:(const GURL&)url {
  std::vector<const bookmarks::BookmarkNode*> localOrSyncableNodes =
      _localOrSyncableBookmarkModel->GetNodesByURL(url);
  if (_accountBookmarkModel) {
    std::vector<const bookmarks::BookmarkNode*> accountNodes =
        _accountBookmarkModel->GetNodesByURL(url);
    localOrSyncableNodes.insert(localOrSyncableNodes.end(),
                                accountNodes.begin(), accountNodes.end());
  }
  return localOrSyncableNodes;
}

// Clears the reindex queue.
- (void)stopIndexing {
  _initialIndexTimer.reset();
  _indexingStack = std::stack<const bookmarks::BookmarkNode*>();
  _nodesIndexed = 0;
  [self.nextBatchOperation cancel];
  self.nextBatchOperation = nil;
}

// Records the time spent on initial indexing and registers the last date
// that initial indexing was completed.
- (void)logInitialIndexComplete {
  if (!_initialIndexTimer) {
    return;
  }
  UMA_HISTOGRAM_TIMES("IOS.Spotlight.BookmarksIndexingDuration",
                      _initialIndexTimer->Elapsed());
  _initialIndexTimer.reset();
  [[NSUserDefaults standardUserDefaults]
      setObject:base::Time::Now().ToNSDate()
         forKey:@(spotlight::kSpotlightLastIndexingDateKey)];

  [[NSUserDefaults standardUserDefaults]
      setObject:@(spotlight::kCurrentSpotlightIndexVersion)
         forKey:@(spotlight::kSpotlightLastIndexingVersionKey)];
}

- (void)logIndexingInterruption {
  _reindexInterruptionCount++;
  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.BookmarksReindexRestarted",
                            _reindexInterruptionCount);
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
  [self reindexBookmarksIfNeeded];
}

- (void)bookmarkModelBeingDeleted:(bookmarks::BookmarkModel*)model {
  if (_accountBookmarkModel == model) {
    _accountBookmarkModel = nullptr;
  }

  if (_localOrSyncableBookmarkModel == model) {
    _localOrSyncableBookmarkModel = nullptr;
  }
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:bookmarkNode];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
           didAddNode:(const bookmarks::BookmarkNode*)node
             toFolder:(const bookmarks::BookmarkNode*)folder {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:node];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:bookmarkNode];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
}

- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self clearAllBookmarkSpotlightItems];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
       willDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self removeNodeFromIndex:node];
}

// The node favicon changed.
- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:bookmarkNode];
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    willChangeBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  // Delete the node; it will be re-added from didChangeNode:.
  [self removeNodeFromIndex:bookmarkNode];
}

@end

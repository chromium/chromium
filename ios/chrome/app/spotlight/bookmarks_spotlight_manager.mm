// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"

#import <CoreSpotlight/CoreSpotlight.h>

#import <memory>
#import <stack>

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "base/version.h"
#import "components/bookmarks/browser/base_bookmark_model_observer.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_type.h"
#import "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

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
// stack, if any.
@property(nonatomic, weak) NSOperation* nextBatchOperation;

/// Tracks if a clear and reindex operation is pending e.g. while the app is
/// backgrounded.
@property(nonatomic, assign) BOOL needsClearAndReindex;

@end

@implementation BookmarksSpotlightManager {
  // Bridge to register for local or syncable bookmark model changes.
  std::unique_ptr<BookmarkModelBridge> _localOrSyncableBookmarkModelBridge;
  // Bridge to register for account bookmark model changes.
  std::unique_ptr<BookmarkModelBridge> _accountBookmarkModelBridge;

  // Keep a reference to detach before deallocing.
  raw_ptr<LegacyBookmarkModel> _localOrSyncableBookmarkModel;  // weak
  // `_accountBookmarkModel` can be `nullptr`.
  raw_ptr<LegacyBookmarkModel> _accountBookmarkModel;  // weak

  // Number of nodes indexed in initial scan.
  NSUInteger _nodesIndexed;

  // Tracks whether initial indexing has been done.
  BOOL _initialIndexDone;

  // Timer that counts how long it takes to index all bookmarks.
  std::unique_ptr<base::ElapsedTimer> _initialIndexTimer;

  // The nodes stored in this stack will be indexed.
  // Nodes are stored as a pair of flag indicating if it belongs to the local
  // model or account model, plus UUID itself.
  // TODO(crbug.com/326185948): Once a single BookmarkModel exists on iOS,
  // remove the enum.
  std::stack<std::pair<BookmarkModelType, base::Uuid>> _indexingStack;

  // Number of times the indexing was interrupted by model updates.
  NSInteger _reindexInterruptionCount;

  // PrefService per a browser state.
  PrefService* _prefService;
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
                        useTitleInIdentifiers:YES]
                       prefService:browserState->GetPrefs()];
}

- (instancetype)
        initWithLargeIconService:(favicon::LargeIconService*)largeIconService
    localOrSyncableBookmarkModel:
        (LegacyBookmarkModel*)localOrSyncableBookmarkModel
            accountBookmarkModel:(LegacyBookmarkModel*)accountBookmarkModel
              spotlightInterface:(SpotlightInterface*)spotlightInterface
           searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
                     prefService:(PrefService*)prefService {
  self = [super initWithSpotlightInterface:spotlightInterface
                     searchableItemFactory:searchableItemFactory];
  if (self) {
    _pendingLargeIconTasksCount = 0;
    _localOrSyncableBookmarkModel = localOrSyncableBookmarkModel;
    _accountBookmarkModel = accountBookmarkModel;
    _prefService = prefService;
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

- (NSArray*)parentFolderNamesForNode:(const bookmarks::BookmarkNode*)node {
  CHECK(node);

  NSMutableArray* parentNames = [[NSMutableArray alloc] init];

  if (!node->is_folder()) {
    node = node->parent();
  }

  while (!node->is_permanent_node()) {
    CHECK(node->is_folder());
    [parentNames addObject:base::SysUTF16ToNSString(node->GetTitle())];
    node = node->parent();
    CHECK(node);
  }

  return [[parentNames reverseObjectEnumerator] allObjects];
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

// Returns true is the current index is too old or from an incompatible
- (BOOL)shouldReindex {
  const base::Time date =
      _prefService->GetTime(spotlight::kSpotlightLastIndexingDateKey);
  const base::TimeDelta timeSinceLastIndexing = base::Time::Now() - date;
  if (timeSinceLastIndexing >= kDelayBetweenTwoIndexing) {
    return YES;
  }
  // The default value is 0 if the value isn't set up yet.
  const int lastIndexedVersion =
      _prefService->GetInteger(spotlight::kSpotlightLastIndexingVersionKey);
  if (lastIndexedVersion < spotlight::kCurrentSpotlightIndexVersion) {
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

  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodesMatchingURL = [self nodesByURL:URL];

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
- (void)refreshNodeInIndex:(const bookmarks::BookmarkNode*)node
                   inModel:(LegacyBookmarkModel*)model {
  DCHECK(node);
  DCHECK(model);

  bool isLocalModel = (model == _localOrSyncableBookmarkModel);
  _indexingStack.push(std::make_pair(isLocalModel
                                         ? BookmarkModelType::kLocalOrSyncable
                                         : BookmarkModelType::kAccount,
                                     node->uuid()));

  if (!self.nextBatchOperation) {
    [self indexNextBatchInStack];
  }
}

// Loads a node from the corresponding model, respecting the correct lookup
// order (see comment in NodeTypeForUuidLookup).
- (const bookmarks::BookmarkNode*)nodeWithUUID:(base::Uuid)uuid
                                usingModelType:(BookmarkModelType)modelType {
  LegacyBookmarkModel* model = nullptr;
  switch (modelType) {
    case BookmarkModelType::kLocalOrSyncable:
      model = _localOrSyncableBookmarkModel;
      break;
    case BookmarkModelType::kAccount:
      model = _accountBookmarkModel;
      break;
  }

  if (!model || !model->loaded()) {
    return nullptr;
  }

  return model->GetNodeByUuid(uuid);
}

- (void)indexNextBatchInStack {
  self.nextBatchOperation = nil;

  if (self.isShuttingDown) {
    [self stopIndexing];
    return;
  }

  if (self.isAppInBackground) {
    // The next batch will auto resume on foreground.
    return;
  }

  for (int i = 0; i < kBatchSize; i++) {
    if (_indexingStack.empty() || _nodesIndexed > kMaxInitialIndexSize) {
      self.modelUpdatesShouldCauseFullReindex = NO;
      [self logInitialIndexComplete];
      [self stopIndexing];
      return;
    }

    std::pair<BookmarkModelType, base::Uuid> nodeDescription =
        _indexingStack.top();
    _indexingStack.pop();

    const bookmarks::BookmarkNode* node =
        [self nodeWithUUID:nodeDescription.second
            usingModelType:nodeDescription.first];
    if (!node) {
      continue;
    }

    if (node->is_url()) {
      _nodesIndexed++;
      [self refreshItemWithURL:node->url()
                         title:base::SysUTF16ToNSString(node->GetTitle())];
    } else {
      for (auto it = node->children().rbegin(); it != node->children().rend();
           ++it) {
        _indexingStack.push(
            std::make_pair(nodeDescription.first, it->get()->uuid()));
      }
    }
  }

  // Dispatch the next batch asynchronously to avoid blocking the main thread.
  __weak BookmarksSpotlightManager* weakSelf = self;
  NSOperation* nextBatchOperation = [NSBlockOperation blockOperationWithBlock:^{
    [weakSelf indexNextBatchInStack];
  }];

  [[NSOperationQueue mainQueue] addOperation:nextBatchOperation];
  self.nextBatchOperation = nextBatchOperation;
}

- (void)shutdown {
  [super shutdown];
  [self detachBookmarkModel];
}

- (void)appWillEnterForeground {
  [super appWillEnterForeground];

  if (self.needsClearAndReindex) {
    [self clearAndReindexModelIfNeeded];
  } else {
    [self indexNextBatchInStack];
  }
}

- (void)clearAndReindexModel {
  [self stopIndexing];

  self.modelUpdatesShouldBeIgnored = YES;
  self.modelUpdatesShouldCauseFullReindex = NO;

  self.needsClearAndReindex = YES;
  [self clearAndReindexModelIfNeeded];
}

- (void)clearAndReindexModelIfNeeded {
  if (self.isAppInBackground || !self.needsClearAndReindex) {
    return;
  }

  [self stopIndexing];
  self.needsClearAndReindex = NO;

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

- (void)pushPermanentFoldersToIndexingStack:(const LegacyBookmarkModel*)model
                             usingModelType:(BookmarkModelType)modelType {
  if (!model || !model->loaded()) {
    return;
  }

  for (const bookmarks::BookmarkNode* permanent_folder :
       {model->bookmark_bar_node(), model->mobile_node(), model->other_node(),
        model->managed_node()}) {
    if (permanent_folder) {
      _indexingStack.push(std::make_pair(modelType, permanent_folder->uuid()));
    }
  }
}

- (void)completedClearAllSpotlightItems {
  if (self.isShuttingDown) {
    return;
  }

  // If the app is in background at this point, avoid accessing the spotlight
  // index and schedule a full reindex on foreground.
  if (self.isAppInBackground) {
    self.needsClearAndReindex = YES;
    return;
  }

  self.modelUpdatesShouldBeIgnored = NO;
  self.modelUpdatesShouldCauseFullReindex = YES;

  // Indexing stack should be empty. There should be no ongoing indexing
  // operations.
  DCHECK(_indexingStack.empty());
  DCHECK(!self.nextBatchOperation);

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
  // TODO(crbug.com/326185948): Once a single BookmarkModel exists on iOS, this
  // code could simply push the root node's UUID.
  [self
      pushPermanentFoldersToIndexingStack:_localOrSyncableBookmarkModel
                           usingModelType:BookmarkModelType::kLocalOrSyncable];
  [self pushPermanentFoldersToIndexingStack:_accountBookmarkModel
                             usingModelType:BookmarkModelType::kAccount];
  _initialIndexTimer = std::make_unique<base::ElapsedTimer>();
  [self indexNextBatchInStack];

  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.BookmarksInitialIndexSize",
                            _pendingLargeIconTasksCount);
}

- (LegacyBookmarkModel*)bookmarkModelForNode:
    (const bookmarks::BookmarkNode*)node {
  if (_localOrSyncableBookmarkModel->IsNodePartOfModel(node)) {
    return _localOrSyncableBookmarkModel;
  }
  DCHECK(_accountBookmarkModel &&
         _accountBookmarkModel->IsNodePartOfModel(node));
  return _accountBookmarkModel;
}

- (std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>)
    nodesByURL:(const GURL&)url {
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      allNodes;

  if (_localOrSyncableBookmarkModel) {
    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
        localOrSyncableNodes =
            _localOrSyncableBookmarkModel->GetNodesByURL(url);
    allNodes.insert(allNodes.end(), localOrSyncableNodes.begin(),
                    localOrSyncableNodes.end());
  }
  if (_accountBookmarkModel) {
    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
        accountNodes = _accountBookmarkModel->GetNodesByURL(url);
    allNodes.insert(allNodes.end(), accountNodes.begin(), accountNodes.end());
  }
  return allNodes;
}

// Clears the reindex stack.
- (void)stopIndexing {
  _initialIndexTimer.reset();
  _indexingStack = std::stack<std::pair<BookmarkModelType, base::Uuid>>();
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

  _prefService->SetTime(spotlight::kSpotlightLastIndexingDateKey,
                        base::Time::Now());
  _prefService->SetInteger(spotlight::kSpotlightLastIndexingVersionKey,
                           spotlight::kCurrentSpotlightIndexVersion);
}

- (void)logIndexingInterruption {
  _reindexInterruptionCount++;
  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.BookmarksReindexRestarted",
                            _reindexInterruptionCount);
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded:(LegacyBookmarkModel*)model {
  [self reindexBookmarksIfNeeded];
}

- (void)bookmarkModelBeingDeleted:(LegacyBookmarkModel*)model {
  if (_accountBookmarkModel == model) {
    _accountBookmarkModel = nullptr;
  }

  if (_localOrSyncableBookmarkModel == model) {
    _localOrSyncableBookmarkModel = nullptr;
  }

  [self stopIndexing];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:bookmarkNode inModel:model];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
           didAddNode:(const bookmarks::BookmarkNode*)node
             toFolder:(const bookmarks::BookmarkNode*)folder {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:node inModel:model];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:bookmarkNode inModel:model];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
}

- (void)bookmarkModelRemovedAllNodes:(LegacyBookmarkModel*)model {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

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

- (void)bookmarkModel:(LegacyBookmarkModel*)model
       willDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

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
- (void)bookmarkModel:(LegacyBookmarkModel*)model
    didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  if (self.modelUpdatesShouldBeIgnored) {
    return;
  }

  if (self.modelUpdatesShouldCauseFullReindex) {
    [self logIndexingInterruption];
    [self clearAndReindexModel];
    return;
  }

  [self refreshNodeInIndex:bookmarkNode inModel:model];
}

- (void)bookmarkModel:(LegacyBookmarkModel*)model
    willChangeBookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

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

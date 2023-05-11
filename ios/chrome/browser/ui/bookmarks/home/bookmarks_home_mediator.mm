// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_mediator.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_shared_state.h"
#import "ios/chrome/browser/ui/bookmarks/synced_bookmarks_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {
// Maximum number of entries to fetch when searching.
const int kMaxBookmarksSearchResults = 50;
}  // namespace

@interface BookmarksHomeMediator () <BookmarkModelBridgeObserver,
                                     BookmarkPromoControllerDelegate,
                                     PrefObserverDelegate,
                                     SigninPresenter,
                                     SyncObserverModelBridge> {
  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _modelBridge;

  // Observer to keep track of the signin and syncing status.
  std::unique_ptr<sync_bookmarks::SyncedBookmarksObserverBridge>
      _syncedBookmarksObserver;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // The browser for this mediator.
  base::WeakPtr<Browser> _browser;
  // The sync setup service for this mediator.
  SyncSetupService* _syncSetupService;
  // Base view controller to present sign-in UI.
  UIViewController* _baseViewController;
}

// Shared state between Bookmark home classes.
@property(nonatomic, strong) BookmarksHomeSharedState* sharedState;

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation BookmarksHomeMediator

- (instancetype)initWithSharedState:(BookmarksHomeSharedState*)sharedState
                            browser:(Browser*)browser
                 baseViewController:(UIViewController*)baseViewController {
  if ((self = [super init])) {
    DCHECK(browser);
    _sharedState = sharedState;
    _browser = browser->AsWeakPtr();
    _baseViewController = baseViewController;
  }
  return self;
}

- (void)startMediating {
  DCHECK(self.consumer);
  DCHECK(self.sharedState);

  // Set up observers.
  ChromeBrowserState* browserState = [self originalBrowserState];
  _modelBridge = std::make_unique<BookmarkModelBridge>(
      self, self.sharedState.profileBookmarkModel);
  _syncedBookmarksObserver =
      std::make_unique<sync_bookmarks::SyncedBookmarksObserverBridge>(
          self, browserState);
  _bookmarkPromoController =
      [[BookmarkPromoController alloc] initWithBrowser:_browser.get()
                                              delegate:self
                                             presenter:self
                                    baseViewController:_baseViewController];

  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(browserState->GetPrefs());
  _prefObserverBridge.reset(new PrefObserverBridge(self));

  _prefObserverBridge->ObserveChangesForPreference(
      bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());

  _prefObserverBridge->ObserveChangesForPreference(
      bookmarks::prefs::kManagedBookmarks, _prefChangeRegistrar.get());

  _syncService = SyncServiceFactory::GetForBrowserState(browserState);
  _syncSetupService = SyncSetupServiceFactory::GetForBrowserState(browserState);

  [self computePromoTableViewData];
  [self computeBookmarkTableViewData];
}

- (void)disconnect {
  [_bookmarkPromoController shutdown];
  _bookmarkPromoController = nil;

  _modelBridge = nullptr;
  _syncSetupService = nullptr;
  _syncedBookmarksObserver = nullptr;
  _browser = nullptr;
  self.consumer = nil;
  self.sharedState = nil;
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
}

#pragma mark - Initial Model Setup

// Computes the bookmarks table view based on the currently displayed node.
- (void)computeBookmarkTableViewData {
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarksHomeSectionIdentifierBookmarks];
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarksHomeSectionIdentifierMessages];

  // Regenerate the list of all bookmarks.
  if (!self.sharedState.profileBookmarkModel->loaded() ||
      !self.sharedState.tableViewDisplayedRootNode) {
    [self updateTableViewBackground];
    return;
  }

  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.profileBookmarkModel->root_node()) {
    [self generateTableViewDataForRootNode];
    [self updateTableViewBackground];
    return;
  }
  [self generateTableViewData];
  [self updateTableViewBackground];
}

// Generate the table view data when the currently displayed node is a child
// node.
- (void)generateTableViewData {
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
  // Add all bookmarks and folders of the currently displayed node to the table.
  for (const auto& child :
       self.sharedState.tableViewDisplayedRootNode->children()) {
    BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:child.get()];
    nodeItem.shouldDisplayCloudSlashIcon =
        bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }
}

// Generate the table view data when the current currently displayed node is the
// outermost root.
- (void)generateTableViewDataForRootNode {
  // If all the permanent nodes are empty, do not create items for any of them.
  if (![self hasBookmarksOrFolders]) {
    return;
  }

  // Add "Mobile Bookmarks" to the table.
  const BookmarkNode* mobileNode =
      self.sharedState.profileBookmarkModel->mobile_node();
  BookmarksHomeNodeItem* mobileItem =
      [[BookmarksHomeNodeItem alloc] initWithType:BookmarksHomeItemTypeBookmark
                                     bookmarkNode:mobileNode];
  mobileItem.shouldDisplayCloudSlashIcon =
      bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
  [self.sharedState.tableViewModel
                      addItem:mobileItem
      toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];

  // Add "Bookmarks Bar" and "Other Bookmarks" only when they are not empty.
  const BookmarkNode* bookmarkBar =
      self.sharedState.profileBookmarkModel->bookmark_bar_node();
  if (!bookmarkBar->children().empty()) {
    BookmarksHomeNodeItem* barItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:bookmarkBar];
    barItem.shouldDisplayCloudSlashIcon =
        bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
    [self.sharedState.tableViewModel
                        addItem:barItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }

  const BookmarkNode* otherBookmarks =
      self.sharedState.profileBookmarkModel->other_node();
  if (!otherBookmarks->children().empty()) {
    BookmarksHomeNodeItem* otherItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:otherBookmarks];
    otherItem.shouldDisplayCloudSlashIcon =
        bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
    [self.sharedState.tableViewModel
                        addItem:otherItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }

  // Add "Managed Bookmarks" to the table if it exists.
  ChromeBrowserState* browserState = [self originalBrowserState];
  bookmarks::ManagedBookmarkService* managedBookmarkService =
      ManagedBookmarkServiceFactory::GetForBrowserState(browserState);
  const BookmarkNode* managedNode = managedBookmarkService->managed_node();
  if (managedNode && managedNode->IsVisible()) {
    BookmarksHomeNodeItem* managedItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:managedNode];
    managedItem.shouldDisplayCloudSlashIcon =
        bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
    [self.sharedState.tableViewModel
                        addItem:managedItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }
}

- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarksHomeSectionIdentifierBookmarks];
  [self deleteAllItemsOrAddSectionWithIdentifier:
            BookmarksHomeSectionIdentifierMessages];

  std::vector<const BookmarkNode*> nodes;
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(new std::u16string);
  *query.word_phrase_query = base::SysNSStringToUTF16(searchText);
  GetBookmarksMatchingProperties(self.sharedState.profileBookmarkModel, query,
                                 kMaxBookmarksSearchResults, &nodes);

  int count = 0;
  for (const BookmarkNode* node : nodes) {
    BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:node];
    nodeItem.shouldDisplayCloudSlashIcon =
        bookmark_utils_ios::ShouldDisplayCloudSlashIcon(_syncSetupService);
    [self.sharedState.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
    count++;
  }

  if (count == 0) {
    TableViewTextItem* item =
        [[TableViewTextItem alloc] initWithType:BookmarksHomeItemTypeMessage];
    item.textAlignment = NSTextAlignmentLeft;
    item.textColor = [UIColor colorNamed:kTextPrimaryColor];
    item.text = noResults;
    [self.sharedState.tableViewModel
                        addItem:item
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierMessages];
    return;
  }

  [self updateTableViewBackground];
}

- (void)updateTableViewBackground {
  // If the currently displayed node is the outermost root, check if we need to
  // show the spinner backgound. Otherwise, check if we need to show the empty
  // background.
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.profileBookmarkModel->root_node()) {
    if (self.sharedState.profileBookmarkModel
            ->HasNoUserCreatedBookmarksOrFolders() &&
        _syncedBookmarksObserver->IsPerformingInitialSync()) {
      [self.consumer
          updateTableViewBackgroundStyle:BookmarksHomeBackgroundStyleLoading];
    } else if (![self hasBookmarksOrFolders]) {
      [self.consumer
          updateTableViewBackgroundStyle:BookmarksHomeBackgroundStyleEmpty];
    } else {
      [self.consumer
          updateTableViewBackgroundStyle:BookmarksHomeBackgroundStyleDefault];
    }
    return;
  }

  if (![self hasBookmarksOrFolders] &&
      !self.sharedState.currentlyShowingSearchResults) {
    [self.consumer
        updateTableViewBackgroundStyle:BookmarksHomeBackgroundStyleEmpty];
  } else {
    [self.consumer
        updateTableViewBackgroundStyle:BookmarksHomeBackgroundStyleDefault];
  }
}

#pragma mark - Public

- (void)computePromoTableViewData {
  // We show promo cell only on the root view, that is when showing
  // the permanent nodes.
  BOOL promoVisible = ((self.sharedState.tableViewDisplayedRootNode ==
                        self.sharedState.profileBookmarkModel->root_node()) &&
                       self.bookmarkPromoController.shouldShowSigninPromo &&
                       !self.sharedState.currentlyShowingSearchResults) &&
                      !self.isSyncDisabledByAdministrator;

  if (promoVisible == self.sharedState.promoVisible) {
    return;
  }
  self.sharedState.promoVisible = promoVisible;

  SigninPromoViewMediator* signinPromoViewMediator =
      self.bookmarkPromoController.signinPromoViewMediator;
  if (self.sharedState.promoVisible) {
    DCHECK(![self.sharedState.tableViewModel
        hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo]);
    [self.sharedState.tableViewModel
        insertSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo
                            atIndex:0];

    TableViewSigninPromoItem* signinPromoItem =
        [[TableViewSigninPromoItem alloc]
            initWithType:BookmarksHomeItemTypePromo];
    signinPromoItem.configurator = [signinPromoViewMediator createConfigurator];
    signinPromoItem.text =
        l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_BOOKMARKS_WITH_UNITY);
    signinPromoItem.delegate = signinPromoViewMediator;
    [signinPromoViewMediator signinPromoViewIsVisible];

    [self.sharedState.tableViewModel
                        addItem:signinPromoItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo];
  } else {
    if (!signinPromoViewMediator.invalidClosedOrNeverVisible) {
      // When the sign-in view is closed, the promo state changes, but
      // -[SigninPromoViewMediator signinPromoViewIsHidden] should not be
      // called.
      [signinPromoViewMediator signinPromoViewIsHidden];
    }

    DCHECK([self.sharedState.tableViewModel
        hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo]);
    [self.sharedState.tableViewModel
        removeSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo];
  }
  [self.sharedState.tableView reloadData];
  // Update the TabelView background to make sure the new state of the promo
  // does not affect the background.
  [self updateTableViewBackground];
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
  // The currently displayed folder's children changed. Reload everything.
  // (When adding new folder, table is already been updated. So no need to
  // reload here.)
  if (bookmarkNode == self.sharedState.tableViewDisplayedRootNode &&
      !self.sharedState.addingNewFolder) {
    if (self.sharedState.currentlyInEditMode && ![self hasBookmarksOrFolders]) {
      [self.consumer setTableViewEditing:NO];
    }
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
    // A folder was added or removed from the currently displayed folder.
    [self.consumer refreshContents];
  }
}

// `node` was deleted from `folder`.
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
  BookmarksHomeNodeItem* nodeItem = [self itemForNode:bookmarkNode];
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

- (BookmarksHomeNodeItem*)itemForNode:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    if (item.type == BookmarksHomeItemTypeBookmark) {
      BookmarksHomeNodeItem* nodeItem =
          base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(item);
      if (nodeItem.bookmarkNode == bookmarkNode) {
        return nodeItem;
      }
    }
  }
  return nil;
}

#pragma mark - BookmarkPromoControllerDelegate

- (void)promoStateChanged:(BOOL)promoEnabled {
  [self computePromoTableViewData];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  if (![self.sharedState.tableViewModel
          hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo] ||
      !identityChanged) {
    return;
  }

  NSIndexPath* indexPath = [self.sharedState.tableViewModel
      indexPathForItemType:BookmarksHomeItemTypePromo
         sectionIdentifier:BookmarksHomeSectionIdentifierPromo];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                          atIndexPath:indexPath];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  // Proxy this call along to the consumer.
  [self.consumer showSignin:command];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (!_browser.get()) {
    // If `_browser` has been removed, the mediator can be disconnected and the
    // event can be ignored. See http://crbug.com/1442174.
    // TODO(crbug.com/1440937): This `if` is a workaround until this bug is
    // fixed. This if should be remove when the bug will be closed.
    [self disconnect];
    return;
  }
  // If user starts or stops syncing bookmarks, we may have to remove or add the
  // slashed cloud icon. Also, permanent nodes ("Bookmarks Bar", "Other
  // Bookmarks") at the root node might be added after syncing.  So we need to
  // refresh here.
  [self.consumer refreshContents];
  if (self.sharedState.tableViewDisplayedRootNode !=
          self.sharedState.profileBookmarkModel->root_node() &&
      !self.isSyncDisabledByAdministrator) {
    [self updateTableViewBackground];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Editing capability may need to be updated on the bookmarks UI.
  // Or managed bookmarks contents may need to be updated.
  if (preferenceName == bookmarks::prefs::kEditBookmarksEnabled ||
      preferenceName == bookmarks::prefs::kManagedBookmarks) {
    [self.consumer refreshContents];
  }
}

#pragma mark - Private Helpers

// The original chrome browser state used for services that don't exist in
// incognito mode. E.g., `_syncSetupService`, `_syncService` and
// `ManagedBookmarkService`.
- (ChromeBrowserState*)originalBrowserState {
  return _browser->GetBrowserState()->GetOriginalChromeBrowserState();
}

- (BOOL)hasBookmarksOrFolders {
  if (self.sharedState.tableViewDisplayedRootNode ==
      self.sharedState.profileBookmarkModel->root_node()) {
    // The root node always has its permanent nodes. If all the permanent nodes
    // are empty, we treat it as if the root itself is empty.
    const auto& childrenOfRootNode =
        self.sharedState.tableViewDisplayedRootNode->children();
    for (const auto& child : childrenOfRootNode) {
      if (!child->children().empty()) {
        return YES;
      }
    }
    return NO;
  }
  return self.sharedState.tableViewDisplayedRootNode &&
         !self.sharedState.tableViewDisplayedRootNode->children().empty();
}

// Delete all items for the given `sectionIdentifier` section, or create it
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

// Returns YES if the user cannot turn on sync for enterprise policy reasons.
- (BOOL)isSyncDisabledByAdministrator {
  DCHECK(self.syncService);
  ChromeBrowserState* browserState = [self originalBrowserState];
  bool syncDisabledPolicy = self.syncService->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  PrefService* prefService = browserState->GetPrefs();
  bool syncTypesDisabledPolicy =
      IsManagedSyncDataType(prefService, SyncSetupService::kSyncBookmarks);
  return syncDisabledPolicy || syncTypesDisabledPolicy;
}
@end

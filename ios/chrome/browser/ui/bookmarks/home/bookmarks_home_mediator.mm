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
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell_title_editing.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_consumer.h"
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

bool IsABookmarkNodeSectionForIdentifier(
    BookmarksHomeSectionIdentifier section_identifier) {
  switch (section_identifier) {
    case BookmarksHomeSectionIdentifierPromo:
    case BookmarksHomeSectionIdentifierMessages:
      return false;
    case BookmarksHomeSectionIdentifierBookmarks:
    case BookmarksHomeSectionIdentifierRootProfile:
    case BookmarksHomeSectionIdentifierRootAccount:
      return true;
  }
  NOTREACHED_NORETURN();
}

@interface BookmarksHomeMediator () <BookmarkModelBridgeObserver,
                                     BookmarkPromoControllerDelegate,
                                     PrefObserverDelegate,
                                     SigninPresenter,
                                     SyncObserverModelBridge> {
  // Observer to keep track of the signin and syncing status.
  std::unique_ptr<sync_bookmarks::SyncedBookmarksObserverBridge>
      _syncedBookmarksObserver;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // The browser for this mediator.
  base::WeakPtr<Browser> _browser;
  // Base view controller to present sign-in UI.
  UIViewController* _baseViewController;
}

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation BookmarksHomeMediator {
  // The model holding profile bookmark data.
  base::WeakPtr<bookmarks::BookmarkModel> _profileBookmarkModel;
  // The model holding account bookmark data.
  base::WeakPtr<bookmarks::BookmarkModel> _accountBookmarkModel;
  // Bridge to register for bookmark changes in the profile model.
  std::unique_ptr<BookmarkModelBridge> _profileBookmarkModelBridge;
  // Bridge to register for bookmark changes in the account model.
  std::unique_ptr<BookmarkModelBridge> _accountBookmarkModelBridge;
  // List of nodes selected by the user when being in the edit mode.
  bookmark_utils_ios::NodeSet _selectedNodesForEditMode;
}

- (instancetype)initWithBrowser:(Browser*)browser
             baseViewController:(UIViewController*)baseViewController
           profileBookmarkModel:(bookmarks::BookmarkModel*)profileBookmarkModel
           accountBookmarkModel:(bookmarks::BookmarkModel*)accountBookmarkModel
                  displayedNode:(const bookmarks::BookmarkNode*)displayedNode {
  if ((self = [super init])) {
    DCHECK(browser);
    CHECK(displayedNode);
    CHECK(bookmark_utils_ios::AreAllAvailableBookmarkModelsLoaded(
        profileBookmarkModel, accountBookmarkModel));

    _browser = browser->AsWeakPtr();
    _profileBookmarkModel = profileBookmarkModel->AsWeakPtr();
    if (base::FeatureList::IsEnabled(
            bookmarks::kEnableBookmarksAccountStorage)) {
      _accountBookmarkModel = accountBookmarkModel->AsWeakPtr();
    }
    _displayedNode = displayedNode;
    _baseViewController = baseViewController;
  }
  return self;
}

- (void)startMediating {
  DCHECK(self.consumer);

  // Set up observers.
  ChromeBrowserState* browserState = [self originalBrowserState];
  _profileBookmarkModelBridge =
      std::make_unique<BookmarkModelBridge>(self, _profileBookmarkModel.get());
  if (base::FeatureList::IsEnabled(bookmarks::kEnableBookmarksAccountStorage)) {
    _accountBookmarkModelBridge = std::make_unique<BookmarkModelBridge>(
        self, _accountBookmarkModel.get());
  }
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

  [self computePromoTableViewData];
  [self computeBookmarkTableViewData];
}

- (void)disconnect {
  [_bookmarkPromoController shutdown];
  _bookmarkPromoController.delegate = nil;
  _bookmarkPromoController = nil;
  _syncService = nullptr;
  _syncedBookmarksObserver = nullptr;
  _browser = nullptr;
  self.consumer = nil;
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _profileBookmarkModel.reset();
  _accountBookmarkModel.reset();
  _profileBookmarkModelBridge.reset();
  _accountBookmarkModelBridge.reset();
}

#pragma mark - Initial Model Setup

// Computes the bookmarks table view based on the currently displayed node.
- (void)computeBookmarkTableViewData {
  [self resetSections];

  if (self.consumer.isDisplayingBookmarkRoot) {
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
  if (!self.displayedNode) {
    return;
  }
  BOOL shouldDisplayCloudSlashIcon = [self
      shouldDisplayCloudSlashIconWithBookmarkModel:self.displayedBookmarkModel];
  // Add all bookmarks and folders of the currently displayed node to the table.
  for (const auto& child : self.displayedNode->children()) {
    BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:child.get()];
    nodeItem.shouldDisplayCloudSlashIcon = shouldDisplayCloudSlashIcon;
    [self.consumer.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }
}

// Generate the table view data when the current currently displayed node is the
// outermost root.
- (void)generateTableViewDataForRootNode {
  BOOL showProfileSection =
      [self hasBookmarksOrFoldersInModel:_profileBookmarkModel.get()];
  BOOL showAccountSection =
      bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService) &&
      [self hasBookmarksOrFoldersInModel:_accountBookmarkModel.get()];
  if (showProfileSection) {
    [self
        generateTableViewDataForModel:_profileBookmarkModel.get()
                            inSection:BookmarksHomeSectionIdentifierRootProfile
                  addManagedBookmarks:YES];
  }
  if (showAccountSection) {
    [self
        generateTableViewDataForModel:_accountBookmarkModel.get()
                            inSection:BookmarksHomeSectionIdentifierRootAccount
                  addManagedBookmarks:NO];
  }
  if (showProfileSection && showAccountSection) {
    // Headers are only shown if both sections are visible.
    [self updateHeaderForProfileRootNode];
    [self updateHeaderForAccountRootNode];
  }
}

- (void)generateTableViewDataForModel:(bookmarks::BookmarkModel*)model
                            inSection:(BookmarksHomeSectionIdentifier)
                                          sectionIdentifier
                  addManagedBookmarks:(BOOL)addManagedBookmarks {
  BOOL shouldDisplayCloudSlashIcon =
      [self shouldDisplayCloudSlashIconWithBookmarkModel:model];
  // Add "Mobile Bookmarks" to the table.
  const BookmarkNode* mobileNode = model->mobile_node();
  BookmarksHomeNodeItem* mobileItem =
      [[BookmarksHomeNodeItem alloc] initWithType:BookmarksHomeItemTypeBookmark
                                     bookmarkNode:mobileNode];
  mobileItem.shouldDisplayCloudSlashIcon = shouldDisplayCloudSlashIcon;
  [self.consumer.tableViewModel addItem:mobileItem
                toSectionWithIdentifier:sectionIdentifier];

  // Add "Bookmarks Bar" and "Other Bookmarks" only when they are not empty.
  const BookmarkNode* bookmarkBar = model->bookmark_bar_node();
  if (!bookmarkBar->children().empty()) {
    BookmarksHomeNodeItem* barItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:bookmarkBar];
    barItem.shouldDisplayCloudSlashIcon = shouldDisplayCloudSlashIcon;
    [self.consumer.tableViewModel addItem:barItem
                  toSectionWithIdentifier:sectionIdentifier];
  }

  const BookmarkNode* otherBookmarks = model->other_node();
  if (!otherBookmarks->children().empty()) {
    BookmarksHomeNodeItem* otherItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:otherBookmarks];
    otherItem.shouldDisplayCloudSlashIcon = shouldDisplayCloudSlashIcon;
    [self.consumer.tableViewModel addItem:otherItem
                  toSectionWithIdentifier:sectionIdentifier];
  }

  if (!addManagedBookmarks) {
    return;
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
    managedItem.shouldDisplayCloudSlashIcon = shouldDisplayCloudSlashIcon;
    [self.consumer.tableViewModel addItem:managedItem
                  toSectionWithIdentifier:sectionIdentifier];
  }
}

- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {
  [self resetSections];
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(new std::u16string);
  *query.word_phrase_query = base::SysNSStringToUTF16(searchText);
  // Total count of search result for both models.
  int totalSearchResultCount = 0;
  if (bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(self.syncService)) {
    totalSearchResultCount =
        [self populateNodeItemWithQuery:query
                          bookmarkModel:_accountBookmarkModel.get()
                  displayCloudSlashIcon:NO];
  }
  BOOL displayCloudSlashIcon = [self
      shouldDisplayCloudSlashIconWithBookmarkModel:_profileBookmarkModel.get()];
  totalSearchResultCount +=
      [self populateNodeItemWithQuery:query
                        bookmarkModel:_profileBookmarkModel.get()
                displayCloudSlashIcon:displayCloudSlashIcon];
  if (totalSearchResultCount) {
    [self updateTableViewBackground];
    return;
  }
  // Add "no result" item.
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:BookmarksHomeItemTypeMessage];
  item.textAlignment = NSTextAlignmentLeft;
  item.textColor = [UIColor colorNamed:kTextPrimaryColor];
  item.text = noResults;
  [self.consumer.tableViewModel addItem:item
                toSectionWithIdentifier:BookmarksHomeSectionIdentifierMessages];
}

- (void)updateTableViewBackground {
  // If the currently displayed node is the outermost root, check if we need to
  // show the spinner backgound. Otherwise, check if we need to show the empty
  // background.
  if (self.consumer.isDisplayingBookmarkRoot) {
    if (_profileBookmarkModel->HasNoUserCreatedBookmarksOrFolders() &&
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

  if (![self hasBookmarksOrFolders] && !self.currentlyShowingSearchResults) {
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
  BOOL promoVisible = (self.consumer.isDisplayingBookmarkRoot &&
                       self.bookmarkPromoController.shouldShowSigninPromo &&
                       !self.currentlyShowingSearchResults) &&
                      !self.isSyncDisabledByAdministrator;

  if (promoVisible == self.promoVisible) {
    return;
  }
  self.promoVisible = promoVisible;

  SigninPromoViewMediator* signinPromoViewMediator =
      self.bookmarkPromoController.signinPromoViewMediator;
  if (self.promoVisible) {
    DCHECK(![self.consumer.tableViewModel
        hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo]);
    [self.consumer.tableViewModel
        insertSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo
                            atIndex:0];

    TableViewSigninPromoItem* signinPromoItem =
        [[TableViewSigninPromoItem alloc]
            initWithType:BookmarksHomeItemTypePromo];
    signinPromoItem.configurator = [signinPromoViewMediator createConfigurator];
    signinPromoItem.text =
        base::FeatureList::IsEnabled(bookmarks::kEnableBookmarksAccountStorage)
            ? l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_BOOKMARKS)
            : l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_BOOKMARKS_WITH_UNITY);
    signinPromoItem.delegate = signinPromoViewMediator;
    [signinPromoViewMediator signinPromoViewIsVisible];

    [self.consumer.tableViewModel addItem:signinPromoItem
                  toSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo];
  } else {
    if (!signinPromoViewMediator.invalidClosedOrNeverVisible) {
      // When the sign-in view is closed, the promo state changes, but
      // -[SigninPromoViewMediator signinPromoViewIsHidden] should not be
      // called.
      [signinPromoViewMediator signinPromoViewIsHidden];
    }

    DCHECK([self.consumer.tableViewModel
        hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo]);
    [self.consumer.tableViewModel
        removeSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo];
  }
  [self.consumer.tableView reloadData];
  // Update the TabelView background to make sure the new state of the promo
  // does not affect the background.
  [self updateTableViewBackground];
}

- (bookmark_utils_ios::NodeSet&)selectedNodesForEditMode {
  return _selectedNodesForEditMode;
}

- (void)setCurrentlyInEditMode:(BOOL)currentlyInEditMode {
  DCHECK(self.consumer.tableView);

  // If not in editing mode but the tableView's editing is ON, it means the
  // table is waiting for a swipe-to-delete confirmation.  In this case, we need
  // to close the confirmation by setting tableView.editing to NO.
  if (!_currentlyInEditMode && self.consumer.tableView.editing) {
    self.consumer.tableView.editing = NO;
  }
  [self.consumer.editingFolderCell stopEdit];
  _currentlyInEditMode = currentlyInEditMode;
  _selectedNodesForEditMode.clear();
  [self.consumer mediatorDidClearEditNodes:self];
  [self.consumer.tableView setEditing:currentlyInEditMode animated:YES];
}

- (BOOL)shouldDisplayCloudSlashIconWithBookmarkModel:
    (bookmarks::BookmarkModel*)bookmarkModel {
  if (bookmarkModel == _profileBookmarkModel.get()) {
    return bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(
        self.syncService);
  }
  CHECK_EQ(bookmarkModel, _accountBookmarkModel.get())
      << "bookmarkModel: " << bookmarkModel
      << ", profileBookmarkModel: " << _profileBookmarkModel.get()
      << ", accountBookmarkModel: " << _accountBookmarkModel.get();
  return NO;
}

#pragma mark - Properties

- (bookmarks::BookmarkModel*)displayedBookmarkModel {
  return bookmark_utils_ios::GetBookmarkModelForNode(
      self.displayedNode, _profileBookmarkModel.get(),
      _accountBookmarkModel.get());
}

#pragma mark - BookmarkModelBridgeObserver

// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
  NOTREACHED();
}

// The node has changed, but not its children.
- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // The root folder changed. Do nothing.
  if (bookmarkNode == self.displayedNode) {
    return;
  }

  // A specific cell changed. Reload, if currently shown.
  if ([self itemForNode:bookmarkNode] != nil) {
    [self.consumer refreshContents];
  }
}

// The node has not changed, but its children have.
- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // In search mode, we want to refresh any changes (like undo).
  if (self.currentlyShowingSearchResults) {
    [self.consumer refreshContents];
  }
  // If we're displaying bookmark root then `bookmarkNode` will never be equal
  // to `self.displayNode`. In this case always update the UI when a node is
  // added/deleted (this method is also called when a node is deleted). Because
  // this update may render bookmark list visible (if there were no bookmarks
  // before) or hide bookmark list (if the last node was deleted).
  if (self.consumer.isDisplayingBookmarkRoot) {
    [self.consumer refreshContents];
    return;
  }
  // The currently displayed folder's children changed. Reload everything.
  // (When adding new folder, table is already been updated. So no need to
  // reload here.)
  if (bookmarkNode == self.displayedNode && !self.addingNewFolder) {
    if (self.currentlyInEditMode && ![self hasBookmarksOrFolders]) {
      [self.consumer setTableViewEditing:NO];
    }
    [self.consumer refreshContents];
    return;
  }
}

// The node has moved to a new parent folder.
- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
  if (oldParent == self.displayedNode || newParent == self.displayedNode) {
    // A folder was added or removed from the currently displayed folder.
    [self.consumer refreshContents];
  }
}

// `node` will be deleted from `folder`.
- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
       willDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(node);
  if (self.displayedNode && self.displayedNode->HasAncestor(node)) {
    self.displayedNode = nullptr;
  }
}

// `node` was deleted from `folder`.
- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self.consumer refreshContents];
}

// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
  // TODO(crbug.com/695749) Check if this case is applicable in the new UI.
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeFaviconForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  // Only urls have favicons.
  DCHECK(bookmarkNode->is_url());

  // Update image of corresponding cell.
  BookmarksHomeNodeItem* nodeItem = [self itemForNode:bookmarkNode];
  if (!nodeItem) {
    return;
  }

  // Check that this cell is visible.
  NSIndexPath* indexPath =
      [self.consumer.tableViewModel indexPathForItem:nodeItem];
  NSArray* visiblePaths = [self.consumer.tableView indexPathsForVisibleRows];
  if (![visiblePaths containsObject:indexPath]) {
    return;
  }

  // Get the favicon from cache directly. (no need to fetch from server)
  [self.consumer loadFaviconAtIndexPath:indexPath fallbackToGoogleServer:NO];
}

- (BookmarksHomeNodeItem*)itemForNode:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  NSArray<TableViewItem*>* items = [self.consumer.tableViewModel
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
  if (![self.consumer.tableViewModel
          hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo]) {
    return;
  }

  NSIndexPath* indexPath = [self.consumer.tableViewModel
      indexPathForItemType:BookmarksHomeItemTypePromo
         sectionIdentifier:BookmarksHomeSectionIdentifierPromo];
  [self.consumer configureSigninPromoWithConfigurator:configurator
                                          atIndexPath:indexPath];
}

- (BOOL)isPerformingInitialSync {
  return _syncedBookmarksObserver->IsPerformingInitialSync();
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
  if (!self.consumer.isDisplayingBookmarkRoot &&
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

- (void)updateHeaderForProfileRootNode {
  TableViewTextHeaderFooterItem* profileHeader =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:BookmarksHomeItemTypeHeader];
  profileHeader.text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARKS_PROFILE_SECTION_TITLE);
  [self.consumer.tableViewModel
                     setHeader:profileHeader
      forSectionWithIdentifier:BookmarksHomeSectionIdentifierRootProfile];
}

- (void)updateHeaderForAccountRootNode {
  TableViewTextHeaderFooterItem* accountHeader =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:BookmarksHomeItemTypeHeader];
  accountHeader.text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARKS_ACCOUNT_SECTION_TITLE);
  [self.consumer.tableViewModel
                     setHeader:accountHeader
      forSectionWithIdentifier:BookmarksHomeSectionIdentifierRootAccount];
}

// The original chrome browser state used for services that don't exist in
// incognito mode. E.g., `_syncService` and `ManagedBookmarkService`.
- (ChromeBrowserState*)originalBrowserState {
  return _browser->GetBrowserState()->GetOriginalChromeBrowserState();
}

- (BOOL)hasBookmarksOrFolders {
  if (self.consumer.isDisplayingBookmarkRoot) {
    if ([self hasBookmarksOrFoldersInModel:_profileBookmarkModel.get()]) {
      return YES;
    }
    return bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService) &&
           [self hasBookmarksOrFoldersInModel:_accountBookmarkModel.get()];
  }
  return self.displayedNode && !self.displayedNode->children().empty();
}

// Returns whether there are bookmark nodes in `model` that are added by users.
- (BOOL)hasBookmarksOrFoldersInModel:(bookmarks::BookmarkModel*)model {
  // The root node always has its permanent nodes. If all the permanent nodes
  // are empty, we treat it as if the root itself is empty.
  const auto& childrenOfRootNode = model->root_node()->children();
  for (const auto& child : childrenOfRootNode) {
    if (!child->children().empty()) {
      return YES;
    }
  }
  return NO;
}

// Ensure all sections exists and are empty.
- (void)resetSections {
  NSArray<NSNumber*>* sectionsToDelete = @[
    @(BookmarksHomeSectionIdentifierBookmarks),
    @(BookmarksHomeSectionIdentifierRootAccount),
    @(BookmarksHomeSectionIdentifierRootProfile),
    @(BookmarksHomeSectionIdentifierMessages)
  ];

  for (NSNumber* section in sectionsToDelete) {
    [self deleteAllItemsOrAddSectionWithIdentifier:section.intValue];
  }
}

// Delete all items for the given `sectionIdentifier` section, or create it
// if it doesn't exist, hence ensuring the section exists and is empty.
- (void)deleteAllItemsOrAddSectionWithIdentifier:(NSInteger)sectionIdentifier {
  TableViewModel* model = self.consumer.tableViewModel;
  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    [model deleteAllItemsFromSectionWithIdentifier:sectionIdentifier];
  } else {
    [model addSectionWithIdentifier:sectionIdentifier];
  }
  [model setHeader:nil forSectionWithIdentifier:sectionIdentifier];
}

// Returns YES if the user cannot turn on sync for enterprise policy reasons.
- (BOOL)isSyncDisabledByAdministrator {
  DCHECK(self.syncService);
  bool syncDisabledPolicy = self.syncService->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  bool syncTypesDisabledPolicy = IsManagedSyncDataType(
      self.syncService, syncer::UserSelectableType::kBookmarks);
  return syncDisabledPolicy || syncTypesDisabledPolicy;
}

// Populates the table view model with BookmarksHomeNodeItem based on the search
// result done in `model` using `query`.
// For each BookmarksHomeNodeItem, the cloud icon is displayed or not according
// to `displayCloudSlashIcon`.
// Returns the number of added items in the table view model.
- (int)populateNodeItemWithQuery:(const bookmarks::QueryFields&)query
                   bookmarkModel:(bookmarks::BookmarkModel*)model
           displayCloudSlashIcon:(BOOL)displayCloudSlashIcon {
  std::vector<const BookmarkNode*> nodes;
  GetBookmarksMatchingProperties(model, query, kMaxBookmarksSearchResults,
                                 &nodes);
  for (const BookmarkNode* node : nodes) {
    BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:node];
    nodeItem.shouldDisplayCloudSlashIcon = displayCloudSlashIcon;
    [self.consumer.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }
  return nodes.size();
}

@end

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/local_data_description.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_table_cell_title_editing.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmark_promo_controller.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_consumer.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/synced_bookmarks_bridge.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_settings_presenter.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using bookmarks::BookmarkNode;

// The size of the symbol displayed in the batch upload dialog.
constexpr CGFloat kBatchUploadSymbolPointSize = 22.;

// Maximum number of entries to fetch when searching.
const int kMaxBookmarksSearchResults = 50;

// Returns true if `node` contains at least one child node.
bool NodeHasChildren(const BookmarkNode* node) {
  CHECK(node);
  return !node->children().empty();
}

// Returns true if at least one node in `nodes` contains at least one child
// node.
bool AnyNodeHasChildren(const std::vector<const BookmarkNode*>& nodes) {
  for (const BookmarkNode* node : nodes) {
    if (NodeHasChildren(node)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool IsABookmarkNodeSectionForIdentifier(
    BookmarksHomeSectionIdentifier section_identifier) {
  switch (section_identifier) {
    case BookmarksHomeSectionIdentifierPromo:
    case BookmarksHomeSectionIdentifierMessages:
      return false;
    case BookmarksHomeSectionIdentifierBookmarks:
    case BookmarksHomeSectionIdentifierRootLocalOrSyncable:
    case BookmarksHomeSectionIdentifierRootAccount:
      return true;
    case BookmarksBatchUploadSectionIdentifier:
      return false;
  }
  NOTREACHED();
}

@interface BookmarksHomeMediator () <AccountSettingsPresenter,
                                     BookmarkModelBridgeObserver,
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
}

// The controller managing the display of the promo cell and the promo view
// controller.
@property(nonatomic, strong) BookmarkPromoController* bookmarkPromoController;

// Sync service.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation BookmarksHomeMediator {
  // The model holding bookmark data.
  base::WeakPtr<bookmarks::BookmarkModel> _bookmarkModel;
  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
  // List of nodes selected by the user when being in the edit mode.
  bookmark_utils_ios::NodeSet _selectedNodesForEditMode;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(
      prefs::kIosBookmarkUploadSyncLeftBehindCompleted, false);
}

- (instancetype)initWithBrowser:(Browser*)browser
                  bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                  displayedNode:(const BookmarkNode*)displayedNode {
  if ((self = [super init])) {
    DCHECK(browser);
    CHECK(displayedNode);
    CHECK(bookmarkModel);
    CHECK(bookmarkModel->loaded());

    _browser = browser->AsWeakPtr();
    _bookmarkModel = bookmarkModel->AsWeakPtr();
    _displayedNode = displayedNode;
  }
  return self;
}

- (void)startMediating {
  DCHECK(self.consumer);

  // Set up observers.
  ProfileIOS* profile = [self originalBrowserState];
  _bookmarkModelBridge =
      std::make_unique<BookmarkModelBridge>(self, _bookmarkModel.get());
  _syncedBookmarksObserver =
      std::make_unique<sync_bookmarks::SyncedBookmarksObserverBridge>(self,
                                                                      profile);
  _syncService = SyncServiceFactory::GetForProfile(profile);
  _bookmarkPromoController =
      [[BookmarkPromoController alloc] initWithBrowser:_browser.get()
                                           syncService:_syncService
                                              delegate:self
                                       signinPresenter:self
                              accountSettingsPresenter:self];

  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(profile->GetPrefs());
  _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

  _prefObserverBridge->ObserveChangesForPreference(
      bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());

  _prefObserverBridge->ObserveChangesForPreference(
      bookmarks::prefs::kManagedBookmarks, _prefChangeRegistrar.get());

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
  _bookmarkModel.reset();
  _bookmarkModelBridge.reset();
}

- (void)dealloc {
  DCHECK(!_bookmarkPromoController);
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
  BOOL shouldDisplayCloudSlashIcon =
      [self shouldDisplayCloudSlashIconWithBookmarkNode:self.displayedNode];
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
  ProfileIOS* profile = [self originalBrowserState];
  bookmarks::ManagedBookmarkService* managedBookmarkService =
      ManagedBookmarkServiceFactory::GetForProfile(profile);
  const BookmarkNode* managedNode = managedBookmarkService->managed_node();

  std::vector<const bookmarks::BookmarkNode*> localPermanentNodes =
      PrimaryPermanentNodes(_bookmarkModel.get(),
                            BookmarkStorageType::kLocalOrSyncable);
  std::vector<const bookmarks::BookmarkNode*> accountPermanentNodes =
      PrimaryPermanentNodes(_bookmarkModel.get(),
                            BookmarkStorageType::kAccount);

  if (managedNode) {
    localPermanentNodes.push_back(managedNode);
  }

  BOOL showProfileSection = AnyNodeHasChildren(localPermanentNodes);

  // Whether the account part should be displayed, if possible.
  BOOL shouldShowIfPossible =
      showProfileSection || AnyNodeHasChildren(accountPermanentNodes);
  BOOL showAccountSection =
      shouldShowIfPossible &&
      bookmark_utils_ios::IsAccountBookmarkStorageAvailable(
          _bookmarkModel.get());
  if (showProfileSection) {
    [self
        generateTableViewDataWithPermanentNodes:localPermanentNodes
                                      inSection:
                                          BookmarksHomeSectionIdentifierRootLocalOrSyncable];
  }
  if (showAccountSection) {
    [self
        generateTableViewDataWithPermanentNodes:accountPermanentNodes
                                      inSection:
                                          BookmarksHomeSectionIdentifierRootAccount];
  }
  if (showProfileSection && showAccountSection) {
    // Headers are only shown if both sections are visible.
    [self updateHeaderForProfileRootNode];
    [self updateHeaderForAccountRootNode];
  }

  // Show the batch upload section if required.
  [self maybeShowBatchUploadSection];
}

- (void)generateTableViewDataWithPermanentNodes:
            (const std::vector<const BookmarkNode*>&)permanentNodes
                                      inSection:(BookmarksHomeSectionIdentifier)
                                                    sectionIdentifier {
  for (const BookmarkNode* permanentNode : permanentNodes) {
    CHECK(permanentNode);
    if (!permanentNode->IsVisible()) {
      continue;
    }

    BookmarksHomeNodeItem* item = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:permanentNode];
    item.shouldDisplayCloudSlashIcon =
        [self shouldDisplayCloudSlashIconWithBookmarkNode:permanentNode];
    [self.consumer.tableViewModel addItem:item
                  toSectionWithIdentifier:sectionIdentifier];
  }
}

- (void)computeBookmarkTableViewDataMatching:(NSString*)searchText
                  orShowMessageWhenNoResults:(NSString*)noResults {
  [self resetSections];
  bookmarks::QueryFields query;
  query.word_phrase_query.reset(new std::u16string);
  *query.word_phrase_query = base::SysNSStringToUTF16(searchText);
  if ([self populateNodeItemWithQuery:query]) {
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
    if (_bookmarkModel->HasNoUserCreatedBookmarksOrFolders() &&
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

    if (signinPromoViewMediator.signinPromoAction ==
        SigninPromoAction::kReviewAccountSettings) {
      signinPromoItem.text = l10n_util::GetNSString(
          IDS_IOS_SIGNIN_PROMO_REVIEW_BOOKMARKS_SETTINGS);
    } else {
      signinPromoItem.text =
          l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_BOOKMARKS);
    }

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

- (void)triggerBatchUpload {
  self.syncService->TriggerLocalDataMigration(
      syncer::DataTypeSet({syncer::BOOKMARKS}));

  ProfileIOS* profile = [self originalBrowserState];
  PrefService* prefService = profile->GetPrefs();
  prefService->SetBoolean(prefs::kIosBookmarkUploadSyncLeftBehindCompleted,
                          true);
}

- (void)queryLocalBookmarks:(void (^)(int local_bookmarks_count,
                                      std::string user_email))completion {
  std::string user_email = self.syncService->GetAccountInfo().email;
  self.syncService->GetLocalDataDescriptions(
      syncer::DataTypeSet({syncer::BOOKMARKS}),
      base::BindOnce(^(std::map<syncer::DataType, syncer::LocalDataDescription>
                           description) {
        auto it = description.find(syncer::BOOKMARKS);
        // GetLocalDataDescriptions() can return an empty result if data type is
        // still in configuration, or has an error.
        if (it != description.end()) {
          completion(it->second.item_count, std::move(user_email));
          return;
        }
        completion(0, std::move(user_email));
      }));
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

- (BOOL)shouldDisplayCloudSlashIconWithBookmarkNode:
    (const BookmarkNode*)bookmarkNode {
  return bookmarkNode &&
         bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(
             self.syncService) &&
         _bookmarkModel->IsLocalOnlyNode(*bookmarkNode);
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelWillRemoveAllNodes {
  if (self.displayedNode && self.displayedNode->is_permanent_node()) {
    // All Bookmarks home mediators will receive
    // `bookmarkModelWillRemoveAllNodes:`. However, the navigation controller
    // should be edited only once. In order to ensure a single Bookmarks home
    // view controller request the navigation controller to change we call
    // `displayRoot` a single time, in the permanent folder.
    [self.consumer displayRoot];
  }
  self.displayedNode = nullptr;
}

// BookmarkModelBridgeObserver Callbacks
// Instances of this class automatically observe the bookmark model.
// The bookmark model has loaded.
- (void)bookmarkModelLoaded {
  NOTREACHED_IN_MIGRATION();
}

// The node has changed, but not its children.
- (void)didChangeNode:(const BookmarkNode*)bookmarkNode {
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
- (void)didChangeChildrenForNode:(const BookmarkNode*)bookmarkNode {
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
- (void)didMoveNode:(const BookmarkNode*)bookmarkNode
         fromParent:(const BookmarkNode*)oldParent
           toParent:(const BookmarkNode*)newParent {
  if (oldParent == self.displayedNode || newParent == self.displayedNode) {
    // A folder was added or removed from the currently displayed folder.
    [self.consumer refreshContents];
  }
}

// `node` will be deleted from `folder`.
- (void)willDeleteNode:(const BookmarkNode*)node
            fromFolder:(const BookmarkNode*)folder {
  DCHECK(node);
  if (self.displayedNode == node) {
    [self.consumer closeThisFolder];
  }
}

// `node` was deleted from `folder`.
- (void)didDeleteNode:(const BookmarkNode*)node
           fromFolder:(const BookmarkNode*)folder {
  [self.consumer refreshContents];
}

// All non-permanent nodes have been removed.
- (void)bookmarkModelRemovedAllNodes {
  // TODO(crbug.com/40508042) Check if this case is applicable in the new UI.
}

- (void)didChangeFaviconForNode:(const BookmarkNode*)bookmarkNode {
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

- (BookmarksHomeNodeItem*)itemForNode:(const BookmarkNode*)bookmarkNode {
  NSArray<TableViewItem*>* items = [self.consumer.tableViewModel
      itemsInSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    if (item.type == BookmarksHomeItemTypeBookmark) {
      BookmarksHomeNodeItem* nodeItem =
          base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(item);
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

#pragma mark - AccountSettingsPresenter

- (void)showAccountSettings {
  [self.consumer showAccountSettings];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (!_browser.get()) {
    // If `_browser` has been removed, the mediator can be disconnected and the
    // event can be ignored. See http://crbug.com/1442174.
    // TODO(crbug.com/40064261): This `if` is a workaround until this bug is
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
  [self updateReviewSettingsPromo];
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

// Updates the sign-in promo.
- (void)updateReviewSettingsPromo {
  self.promoVisible = NO;
  if ([self.consumer.tableViewModel
          hasSectionForSectionIdentifier:BookmarksHomeSectionIdentifierPromo]) {
    [self.consumer.tableViewModel
        removeSectionWithIdentifier:BookmarksHomeSectionIdentifierPromo];
  }
  // Decide if a sign in promo should be visible.
  [self computePromoTableViewData];
  // Decide if the promo should be removed.
  [self.bookmarkPromoController updateShouldShowSigninPromo];
}

- (void)updateHeaderForProfileRootNode {
  TableViewTextHeaderFooterItem* localOrSyncableHeader =
      [[TableViewTextHeaderFooterItem alloc]
          initWithType:BookmarksHomeItemTypeHeader];
  localOrSyncableHeader.text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARKS_PROFILE_SECTION_TITLE);
  [self.consumer.tableViewModel setHeader:localOrSyncableHeader
                 forSectionWithIdentifier:
                     BookmarksHomeSectionIdentifierRootLocalOrSyncable];
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

// Returns true if batch upload dialog should be shown. This checks for the
// appropriate feature flags, bookmarks state and sync state.
- (BOOL)shouldShowBatchUploadSection {
  // Do not show if profile section is empty.
  BOOL showProfileSection = AnyNodeHasChildren(PrimaryPermanentNodes(
      _bookmarkModel.get(), BookmarkStorageType::kLocalOrSyncable));
  if (!showProfileSection) {
    return NO;
  }
  // Do not show if sync is disabled or is paused.
  // This implicitly covers the case when Bookmarks are disabled by
  // SyncTypesListDisabled.
  if (!self.syncService || self.syncService->GetAccountInfo().IsEmpty() ||
      !self.syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kBookmarks) ||
      self.syncService->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return NO;
  }
  // Do not show for syncing users.
  // TODO(crbug.com/40066949): Remove this after UNO phase 3. See
  // ConsentLevel::kSync documentation for more details.
  if (self.syncService->HasSyncConsent()) {
    return NO;
  }
  // Do not show if last syncing account is different from the current one.
  // Note that the "last syncing" account pref is cleared during the migration
  // of syncing users to the signed-in state, but these users should also be
  // covered here, so check the "migrated syncing user" pref too.
  // This implicitly covers the case when SyncDisabled policy is enabled, as
  // kGoogleServicesLastSyncingGaiaId will be empty.
  ProfileIOS* profile = [self originalBrowserState];
  const std::string lastSyncingGaiaId =
      profile->GetPrefs()->GetString(prefs::kGoogleServicesLastSyncingGaiaId);
  const std::string migratedGaiaId = profile->GetPrefs()->GetString(
      prefs::kGoogleServicesSyncingGaiaIdMigratedToSignedIn);
  if (self.syncService->GetAccountInfo().gaia != lastSyncingGaiaId &&
      self.syncService->GetAccountInfo().gaia != migratedGaiaId) {
    return NO;
  }
  // Do not show if the user is in an error state that makes data upload
  // impossible.
  if (self.syncService->GetUserActionableError() !=
      syncer::SyncService::UserActionableError::kNone) {
    return NO;
  }
  // Do not show if the user has already uploaded the left-behind bookmarks.
  if (profile->GetPrefs()->GetBoolean(
          prefs::kIosBookmarkUploadSyncLeftBehindCompleted)) {
    return NO;
  }
  return YES;
}

// Asynchronously show the batch upload dialog, if required (i.e. if there
// exists local bookmarks and the pre-requisites are met).
- (void)maybeShowBatchUploadSection {
  if (![self shouldShowBatchUploadSection]) {
    return;
  }

  __weak BookmarksHomeMediator* weakSelf = self;
  [self
      queryLocalBookmarks:^(int local_bookmarks_count, std::string user_email) {
        [weakSelf addBatchUploadSection:local_bookmarks_count];
      }];
}

// Populates the batch upload section with recommendation item and button.
- (void)addBatchUploadSection:(int)local_bookmarks_count {
  // Remove any existing batch upload cards and replace with one with the latest
  // info.
  [self deleteAllItemsInBatchUploadSectionIfExists];

  if (local_bookmarks_count == 0) {
    [self.consumer.tableView reloadData];
    return;
  }

  TableViewImageItem* item = [[TableViewImageItem alloc]
      initWithType:BookmarksHomeItemTypeBatchUploadRecommendation];
  item.detailText = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_SECTION_DESCRIPTION),
          "count", local_bookmarks_count, "email",
          _syncService->GetAccountInfo().email));
  item.image = CustomSymbolWithPointSize(kCloudAndArrowUpSymbol,
                                         kBatchUploadSymbolPointSize);
  item.enabled = NO;
  item.accessibilityIdentifier =
      kBookmarksHomeBatchUploadRecommendationItemIdentifier;

  TableViewTextItem* button = [[TableViewTextItem alloc]
      initWithType:BookmarksHomeItemTypeBatchUploadButton];
  button.text = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_BUTTON_ITEM);
  button.textColor = [UIColor colorNamed:kBlueColor];
  button.accessibilityIdentifier = kBookmarksHomeBatchUploadButtonIdentifier;

  [self.consumer.tableViewModel addItem:item
                toSectionWithIdentifier:BookmarksBatchUploadSectionIdentifier];
  [self.consumer.tableViewModel addItem:button
                toSectionWithIdentifier:BookmarksBatchUploadSectionIdentifier];

  base::UmaHistogramBoolean(
      "IOS.Bookmarks.BulkSaveBookmarksInAccountViewRecreated", true);

  [self.consumer.tableView reloadData];
}

// Removes all the items from the batch upload section, if they exist.
- (void)deleteAllItemsInBatchUploadSectionIfExists {
  NSInteger itemsInSection = [self.consumer.tableViewModel
      numberOfItemsInSection:[self.consumer.tableViewModel
                                 sectionForSectionIdentifier:
                                     BookmarksBatchUploadSectionIdentifier]];
  if (itemsInSection == 0) {
    return;
  }
  // The recommendation item and the button exist together.
  CHECK_EQ(2, itemsInSection);
  CHECK([self.consumer.tableViewModel
      hasItemForItemType:BookmarksHomeItemTypeBatchUploadRecommendation
       sectionIdentifier:BookmarksBatchUploadSectionIdentifier]);
  CHECK([self.consumer.tableViewModel
      hasItemForItemType:BookmarksHomeItemTypeBatchUploadButton
       sectionIdentifier:BookmarksBatchUploadSectionIdentifier]);

  [self.consumer.tableViewModel deleteAllItemsFromSectionWithIdentifier:
                                    BookmarksBatchUploadSectionIdentifier];
}

// The original chrome profile used for services that don't exist in
// incognito mode. E.g., `_syncService` and `ManagedBookmarkService`.
- (ProfileIOS*)originalBrowserState {
  return _browser->GetProfile()->GetOriginalProfile();
}

- (BOOL)hasBookmarksOrFolders {
  if (!self.consumer.isDisplayingBookmarkRoot) {
    return self.displayedNode && !self.displayedNode->children().empty();
  }

  // For the root node, it is necessary to check if any of the top-level
  // permanent nodes (local ones, account ones and possibly the managed node)
  // has children.
  for (const auto& node : _bookmarkModel->root_node()->children()) {
    if (NodeHasChildren(node.get())) {
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
    @(BookmarksHomeSectionIdentifierRootLocalOrSyncable),
    @(BookmarksHomeSectionIdentifierMessages),
    @(BookmarksBatchUploadSectionIdentifier)
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
  bool syncTypesDisabledPolicy =
      self.syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kBookmarks);
  return syncDisabledPolicy || syncTypesDisabledPolicy;
}

// Populates the table view model with BookmarksHomeNodeItem based on the search
// result done in BookmarkModel using `query`.
// For each BookmarksHomeNodeItem, the cloud icon is displayed or not according
// to the bookmark node's properties.
// Returns the number of added items in the table view model.
- (int)populateNodeItemWithQuery:(const bookmarks::QueryFields&)query {
  std::vector<const BookmarkNode*> nodes =
      bookmarks::GetBookmarksMatchingProperties(_bookmarkModel.get(), query,
                                                kMaxBookmarksSearchResults);
  for (const BookmarkNode* node : nodes) {
    BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
        initWithType:BookmarksHomeItemTypeBookmark
        bookmarkNode:node];
    nodeItem.shouldDisplayCloudSlashIcon =
        [self shouldDisplayCloudSlashIconWithBookmarkNode:node];
    [self.consumer.tableViewModel
                        addItem:nodeItem
        toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  }
  return nodes.size();
}

@end

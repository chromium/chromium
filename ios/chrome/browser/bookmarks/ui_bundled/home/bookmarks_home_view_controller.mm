// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_view_controller.h"

#import <set>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/i18n/message_formatter.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/numerics/safe_conversions.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_navigation_controller.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_path_cache.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_table_cell_title_edit_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/cells/table_view_bookmarks_folder_item.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_consumer.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_home_mediator.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

using bookmarks::BookmarkNode;
using l10n_util::GetNSString;

using BookmarkNodeIDSet = std::set<int64_t>;

// Used to store a pair of NSIntegers when storing a NSIndexPath in C++
// collections.
using IntegerPair = std::pair<NSInteger, NSInteger>;

typedef NS_ENUM(NSInteger, BookmarksContextBarState) {
  BookmarksContextBarNone,            // No state.
  BookmarksContextBarDefault,         // No selection is possible in this state.
  BookmarksContextBarBeginSelection,  // This is the clean start state,
  // selection is possible, but nothing is
  // selected yet.
  BookmarksContextBarSingleURLSelection,       // Single URL selected state.
  BookmarksContextBarMultipleURLSelection,     // Multiple URLs selected state.
  BookmarksContextBarSingleFolderSelection,    // Single folder selected.
  BookmarksContextBarMultipleFolderSelection,  // Multiple folders selected.
  BookmarksContextBarMixedSelection,  // Multiple URL / Folders selected.
};

// Estimated TableView row height.
constexpr CGFloat kEstimatedRowHeight = 65.0;
// Separation between non-empty account and profile sections.
constexpr CGFloat kSpaceBetweenAccountAndProfileSections = 32.0;

// Returns a vector of all URLs in `nodes`.
std::vector<GURL> GetUrlsToOpen(const std::vector<const BookmarkNode*>& nodes) {
  std::vector<GURL> urls;
  for (const BookmarkNode* node : nodes) {
    if (node->is_url()) {
      urls.push_back(node->url());
    }
  }
  return urls;
}

// Given a set of BookmarkNode pointers, it returns their IDs in a set.
BookmarkNodeIDSet GetBookmarkNodeIDSet(
    const std::set<const BookmarkNode*>& nodes) {
  BookmarkNodeIDSet nodeIDs;
  for (const BookmarkNode* node : nodes) {
    nodeIDs.emplace(node->id());
  }
  return nodeIDs;
}

}  // namespace

@interface BookmarksHomeViewController () <
    BookmarksCoordinatorDelegate,
    BookmarksFolderChooserCoordinatorDelegate,
    BookmarksHomeConsumer,
    BookmarkModelBridgeObserver,
    BookmarkTableCellTitleEditDelegate,
    TableViewURLDragDataSource,
    TableViewURLDropDelegate,
    UIGestureRecognizerDelegate,
    UISearchControllerDelegate,
    UISearchResultsUpdating,
    UITableViewDataSource,
    UITableViewDelegate>

// The mediator that provides data for this view controller.
@property(nonatomic, strong) BookmarksHomeMediator* mediator;

// TODO(crbug.com/40251259): Move this to BookmarksHomeCoordinator.
// A reference to the presented folder chooser.
@property(nonatomic, strong)
    BookmarksFolderChooserCoordinator* folderChooserCoordinator;

// FaviconLoader is a keyed service that uses LargeIconService to retrieve
// favicon images.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// The current state of the context bar UI.
@property(nonatomic, assign) BookmarksContextBarState contextBarState;

// When the view is first shown on the screen, this property represents the
// cached value of the top most visible indexPath row of the table view. This
// property is set to nil after it is used.
@property(nonatomic, assign) int cachedIndexPathRow;

// Set to YES, only when this view controller instance is being created
// from cached path. Once the view controller is shown, this is set to NO.
// This is so that the cache code is called only once in loadBookmarkViews.
@property(nonatomic, assign) BOOL isReconstructingFromCache;

// The current search term.  Set to the empty string when no search is active.
@property(nonatomic, copy) NSString* searchTerm;

// This ViewController's searchController;
@property(nonatomic, strong) UISearchController* searchController;

// Navigation UIToolbar Delete button.
@property(nonatomic, strong) UIBarButtonItem* deleteButton;

// Navigation UIToolbar More button.
@property(nonatomic, strong) UIBarButtonItem* moreButton;

// Scrim when search box in focused.
@property(nonatomic, strong) UIControl* scrimView;

// Illustrated View displayed when the current root node is empty.
@property(nonatomic, strong) TableViewIllustratedEmptyView* emptyViewBackground;

// The loading spinner background which appears when loading the BookmarkModel
// or syncing.
@property(nonatomic, strong) HomeWaitingView* spinnerView;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) AlertCoordinator* actionSheetCoordinator;

@property(nonatomic, strong) BookmarksCoordinator* bookmarksCoordinator;

@property(nonatomic, assign) WebStateList* webStateList;

// Handler for URL drag and drop interactions.
@property(nonatomic, strong) TableViewURLDragDropHandler* dragDropHandler;

// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

@end

@implementation BookmarksHomeViewController {
  // The bookmark model used.
  base::WeakPtr<bookmarks::BookmarkModel> _bookmarkModel;
  // The Browser in which bookmarks are presented
  base::WeakPtr<Browser> _browser;
  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;
  // The bookmark node that was choosen by an entity outside of the Bookmarks UI
  // and is selected when the view is loaded.
  raw_ptr<const BookmarkNode> _externalBookmark;
  // Whether the view controller was requested to shutdown.
  BOOL _isShutDown;
  // Whether the navigation controller is being dismissed.
  // In which case, do not open anything on top of it.
  BOOL _isBeingDismissed;
}

@synthesize editingFolderCell = _editingFolderCell;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser->AsWeakPtr();
    ProfileIOS* profile = self.profile;
    _webStateList = browser->GetWebStateList();

    _faviconLoader = IOSChromeFaviconLoaderFactory::GetForProfile(profile);

    _bookmarkModel =
        ios::BookmarkModelFactory::GetForProfile(profile)->AsWeakPtr();
    _bookmarkModelBridge =
        std::make_unique<BookmarkModelBridge>(self, _bookmarkModel.get());
  }
  return self;
}

- (void)dealloc {
  DCHECK(_isShutDown);
}

- (void)shutdown {
  _isShutDown = YES;
  [self.editingFolderCell stopEdit];
  [self stopFolderChooserCoordinator];
  [self.bookmarksCoordinator stop];
  self.bookmarksCoordinator = nil;
  [self.mediator disconnect];
  self.mediator.consumer = nil;
  self.mediator = nil;
  _browser = nullptr;
  [self.searchController dismissViewControllerAnimated:YES completion:nil];
  [self dismissActionSheetCoordinator];
  _bookmarkModel = nullptr;
  _bookmarkModelBridge.reset();
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
}

- (void)setExternalBookmark:(const BookmarkNode*)node {
  _externalBookmark = node;
}

- (BOOL)canDismiss {
  if (self.folderChooserCoordinator &&
      ![self.folderChooserCoordinator canDismiss]) {
    return NO;
  }
  if (self.bookmarksCoordinator && ![self.bookmarksCoordinator canDismiss]) {
    return NO;
  }
  return YES;
}

- (NSArray<BookmarksHomeViewController*>*)cachedViewControllerStack {
  // This method is only designed to be called for the view controller
  // associated with the root node.
  CHECK(_bookmarkModel->loaded());
  DCHECK([self isDisplayingBookmarkRoot]);

  NSMutableArray<BookmarksHomeViewController*>* stack = [NSMutableArray array];
  // Configure the root controller Navigationbar at this time when
  // reconstructing from cache, or there will be a loading flicker if this gets
  // done on viewDidLoad.
  [self setupNavigationForBookmarksHomeViewController:self
                                    usingBookmarkNode:self.displayedFolderNode];
  [stack addObject:self];

  int64_t cachedFolderID;
  int cachedIndexPathRow;
  // If cache is present then reconstruct the last visited bookmark from
  // cache.
  if (![BookmarkPathCache
          bookmarkTopMostRowCacheWithPrefService:self.profile->GetPrefs()
                                   bookmarkModel:_bookmarkModel.get()
                                        folderId:&cachedFolderID
                                      topMostRow:&cachedIndexPathRow] ||
      cachedFolderID == _bookmarkModel->root_node()->id()) {
    return stack;
  }

  NSArray<NSNumber*>* path = bookmark_utils_ios::CreateBookmarkPath(
      _bookmarkModel.get(), cachedFolderID);
  if (!path) {
    return stack;
  }

  for (NSUInteger ii = 0; ii < [path count]; ii++) {
    int64_t nodeID = [[path objectAtIndex:ii] longLongValue];
    const BookmarkNode* node =
        bookmark_utils_ios::FindFolderById(_bookmarkModel.get(), nodeID);
    DCHECK(node);
    // if node is an empty permanent node, stop.
    if (node->children().empty() && node->is_permanent_node()) {
      break;
    }

    BookmarksHomeViewController* controller =
        [self createControllerWithDisplayedFolderNode:node];
    // Configure the controller's Navigationbar at this time when
    // reconstructing from cache, or there will be a loading flicker if this
    // gets done on viewDidLoad.
    [self setupNavigationForBookmarksHomeViewController:controller
                                      usingBookmarkNode:node];
    if (nodeID == cachedFolderID) {
      controller.cachedIndexPathRow = cachedIndexPathRow;
    }
    [stack addObject:controller];
  }
  return stack;
}

- (void)willDismiss {
  _isBeingDismissed = YES;
}

- (void)willDismissBySwipeDown {
  if (self.searchController.active) {
    // Dismiss the keyboard if trying to dismiss the VC so the keyboard doesn't
    // linger until the VC dismissal has completed.
    [self.searchController.searchBar endEditing:YES];
  }
  [self willDismiss];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set Navigation Bar, Toolbar and TableView appearance.
  self.navigationController.navigationBarHidden = NO;

  self.navigationController.toolbar.accessibilityIdentifier =
      kBookmarksHomeUIToolbarIdentifier;

  // SearchController Configuration.
  // Init the searchController with nil so the results are displayed on the
  // same TableView.
  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchBar.userInteractionEnabled = NO;
  self.searchController.delegate = self;
  self.searchController.searchResultsUpdater = self;
  self.searchController.searchBar.backgroundColor = UIColor.clearColor;
  self.searchController.searchBar.accessibilityIdentifier =
      kBookmarksHomeSearchBarIdentifier;

  // UIKit needs to know which controller will be presenting the
  // searchController. If we don't add this trying to dismiss while
  // SearchController is active will fail.
  self.definesPresentationContext = YES;

  self.scrimView = [[UIControl alloc] init];
  self.scrimView.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrimView.accessibilityIdentifier = kBookmarksHomeSearchScrimIdentifier;
  [self.scrimView addTarget:self
                     action:@selector(dismissSearchController:)
           forControlEvents:UIControlEventTouchUpInside];

  // Place the search bar in the navigation bar.
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  self.searchTerm = @"";

  if (_bookmarkModel->loaded()) {
    [self loadBookmarkViews];
  } else {
    [self showLoadingSpinnerBackground];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (_isShutDown) {
    // After `shutdown` is called, `_profile` is null.
    return;
  }
  // Set the delegate here to make sure it is working when navigating in the
  // ViewController hierarchy (as each view controller is setting itself as
  // delegate).
  self.navigationController.interactivePopGestureRecognizer.delegate = self;

  // Hide the toolbar if we're displaying the root node.
  if (_bookmarkModel->loaded() &&
      (![self isDisplayingBookmarkRoot] ||
       self.mediator.currentlyShowingSearchResults)) {
    self.navigationController.toolbarHidden = NO;
  } else {
    self.navigationController.toolbarHidden = YES;
  }

  // If we navigate back to the root level, we need to make sure the root level
  // folders are created or deleted if needed.
  if ([self isDisplayingBookmarkRoot]) {
    [self refreshContents];
  }

  [IntentDonationHelper donateIntent:IntentType::kOpenBookmarks];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (_isShutDown) {
    return;
  }

  if (parent) {
    // This view controller is added to the navigation controller.
    return;
  }

  // This view controller is removed from its parent, shutdown can be done.
  [self shutdown];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Check that the tableView still contains as many rows, and that
  // `self.cachedIndexPathRow` is not 0.
  if (self.cachedIndexPathRow &&
      [self.tableView numberOfRowsInSection:0] > self.cachedIndexPathRow) {
    NSIndexPath* indexPath =
        [NSIndexPath indexPathForRow:self.cachedIndexPathRow inSection:0];
    [self.tableView scrollToRowAtIndexPath:indexPath
                          atScrollPosition:UITableViewScrollPositionTop
                                  animated:NO];
    self.cachedIndexPathRow = 0;
  }
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Stop edit of current bookmark folder name, if any.
  [self.editingFolderCell stopEdit];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleDefault;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self navigationBarCancel:nil];
}

#pragma mark - Protected

- (void)loadBookmarkViews {
  DCHECK(self.displayedFolderNode);
  [self loadModel];

  self.dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityBookmarksOrigin;
  self.dragDropHandler.dragDataSource = self;
  self.dragDropHandler.dropDelegate = self;
  self.tableView.dragDelegate = self.dragDropHandler;
  self.tableView.dropDelegate = self.dragDropHandler;
  self.tableView.dragInteractionEnabled = true;

  self.tableView.accessibilityIdentifier = kBookmarksHomeTableViewIdentifier;
  self.tableView.estimatedRowHeight = kEstimatedRowHeight;
  self.tableView.allowsMultipleSelectionDuringEditing = YES;

  // Create the mediator and hook up the table view.
  self.mediator =
      [[BookmarksHomeMediator alloc] initWithBrowser:_browser.get()
                                       bookmarkModel:_bookmarkModel.get()
                                       displayedNode:self.displayedFolderNode];
  self.mediator.currentlyShowingSearchResults = NO;
  // Configure the table view.
  self.mediator.consumer = self;
  [self.mediator startMediating];

  [self setupNavigationForBookmarksHomeViewController:self
                                    usingBookmarkNode:self.displayedFolderNode];

  [self setupContextBar];

  if (self.isReconstructingFromCache) {
    [self setupUIStackCacheIfApplicable];
  }

  self.searchController.searchBar.userInteractionEnabled = YES;

  [self editExternalBookmarkIfSet];

  DCHECK(_bookmarkModel->loaded());
  DCHECK([self isViewLoaded]);
}

- (void)cacheIndexPathRow {
  // Cache IndexPathRow for BookmarkTableView.
  int topMostVisibleIndexPathRow = [self topMostVisibleIndexPathRow];
  if (self.displayedFolderNode) {
    [BookmarkPathCache
        cacheBookmarkTopMostRowWithPrefService:self.profile->GetPrefs()
                                      folderId:self.displayedFolderNode->id()
                                     inStorage:bookmark_utils_ios::
                                                   GetBookmarkStorageType(
                                                       self.displayedFolderNode,
                                                       _bookmarkModel.get())
                                    topMostRow:topMostVisibleIndexPathRow];
  } else {
    // TODO(crbug.com/40679851):Remove DCHECK once we know the root cause of the
    // bug, for now this will cause a crash on Dev/Canary and we should get
    // breadcrumbs.
    DCHECK(NO);
  }
}

#pragma mark - BookmarksHomeConsumer

- (void)closeThisFolder {
  [self jumpToFolder:self.displayedFolderNode->parent()];
}

- (void)displayRoot {
  [self jumpToFolder:_bookmarkModel->root_node()];
}

- (void)setTableViewEditing:(BOOL)editing {
  self.mediator.currentlyInEditMode = editing;
  [self setContextBarState:editing ? BookmarksContextBarBeginSelection
                                   : BookmarksContextBarDefault];
  self.searchController.searchBar.userInteractionEnabled = !editing;
  self.searchController.searchBar.alpha =
      editing ? kTableViewNavigationAlphaForDisabledSearchBar : 1.0;

  self.tableView.dragInteractionEnabled = !editing;
}

- (void)refreshContents {
  if (self.mediator.currentlyShowingSearchResults) {
    NSString* noResults = GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self.mediator computeBookmarkTableViewDataMatching:self.searchTerm
                             orShowMessageWhenNoResults:noResults];
  } else {
    [self.mediator computeBookmarkTableViewData];
  }
  [self handleRefreshContextBar];
  [self.editingFolderCell stopEdit];
  [self.tableView.contextMenuInteraction dismissMenu];
  [self.tableView reloadData];
  if (self.mediator.currentlyInEditMode &&
      !self.mediator.selectedNodesForEditMode.empty()) {
    [self restoreRowSelection];
  }
}

- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
        fallbackToGoogleServer:(BOOL)fallbackToGoogleServer {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [self loadFaviconAtIndexPath:indexPath
                       forCell:cell
        fallbackToGoogleServer:fallbackToGoogleServer];
}

// Asynchronously loads favicon for given index path. The loads are cancelled
// upon cell reuse automatically.  When the favicon is not found in cache, try
// loading it from a Google server if `fallbackToGoogleServer` is YES,
// otherwise, use the fall back icon style.
- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
                       forCell:(UITableViewCell*)cell
        fallbackToGoogleServer:(BOOL)fallbackToGoogleServer {
  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  if (node->is_folder()) {
    return;
  }

  // Start loading a favicon.
  __weak BookmarksHomeViewController* weakSelf = self;
  GURL blockURL(node->url());
  auto faviconLoadedBlock = ^(FaviconAttributes* attributes) {
    BookmarksHomeViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // Due to search filtering, we also need to validate the indexPath
    // requested versus what is in the table now.
    if (![strongSelf hasItemAtIndexPath:indexPath] ||
        [strongSelf nodeAtIndexPath:indexPath] != node) {
      return;
    }
    TableViewURLCell* URLCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cell);
    [URLCell.faviconView configureWithAttributes:attributes];
  };

  self.faviconLoader->FaviconForPageUrl(
      blockURL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/fallbackToGoogleServer, faviconLoadedBlock);
}

- (void)updateTableViewBackgroundStyle:(BookmarksHomeBackgroundStyle)style {
  if (style == BookmarksHomeBackgroundStyleDefault) {
    [self hideLoadingSpinnerBackground];
    [self hideEmptyBackground];
  } else if (style == BookmarksHomeBackgroundStyleLoading) {
    [self hideEmptyBackground];
    [self showLoadingSpinnerBackground];
  } else if (style == BookmarksHomeBackgroundStyleEmpty) {
    [self hideLoadingSpinnerBackground];
    [self showEmptyBackground];
  }
}

- (void)showSignin:(ShowSigninCommand*)command {
  [self.applicationCommandsHandler showSignin:command
                           baseViewController:self.navigationController];
}

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                                 atIndexPath:(NSIndexPath*)indexPath {
  TableViewSigninPromoItem* signinPromoItem =
      base::apple::ObjCCast<TableViewSigninPromoItem>(
          [self.tableViewModel itemAtIndexPath:indexPath]);
  if (!signinPromoItem) {
    return;
  }

  signinPromoItem.configurator = configurator;
  [self reloadCellsForItems:@[ signinPromoItem ]
           withRowAnimation:UITableViewRowAnimationNone];
}

- (void)mediatorDidClearEditNodes:(BookmarksHomeMediator*)mediator {
  [self handleSelectEditNodes:mediator.selectedNodesForEditMode];
}

- (void)showAccountSettings {
  [self ensureBookmarksCoordinator];
  [self.bookmarksCoordinator showAccountSettings];
}

#pragma mark - Action sheet callbacks

// Returns contextual menu for a bookmark node at `indexPath`.
- (UIMenu*)bookmarkNodeContextualMenuWithIndexPath:(NSIndexPath*)indexPath
                                       canEditNode:(BOOL)canEditNode {
  const BookmarkNode* bookmarkNode = [self nodeAtIndexPath:indexPath];
  DCHECK_EQ(bookmarkNode->type(), BookmarkNode::URL);
  const GURL nodeURL = bookmarkNode->url();
  const int64_t nodeID = bookmarkNode->id();

  // Record that this context menu was shown to the user.
  RecordMenuShown(kMenuScenarioHistogramBookmarkEntry);
  BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:_browser.get()
             scenario:kMenuScenarioHistogramBookmarkEntry];
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  __weak __typeof(self) weakSelf = self;
  // Add open URL menu item.
  UIAction* openAction = [actionFactory actionToOpenInNewTabWithBlock:^{
    if ([weakSelf isIncognitoForced]) {
      return;
    }
    [weakSelf openAllURLs:{nodeURL} inIncognito:NO newTab:YES];
  }];
  if ([self isIncognitoForced]) {
    openAction.attributes = UIMenuElementAttributesDisabled;
  }
  [menuElements addObject:openAction];
  // Add open URL in incognito menu item.
  UIAction* openInIncognito =
      [actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
        if (![weakSelf isIncognitoAvailable]) {
          return;
        }
        [weakSelf openAllURLs:{nodeURL} inIncognito:YES newTab:YES];
      }];
  if (![self isIncognitoAvailable]) {
    openInIncognito.attributes = UIMenuElementAttributesDisabled;
  }
  [menuElements addObject:openInIncognito];
  // Add open URL in new window menu item.
  if (base::ios::IsMultipleScenesSupported()) {
    [menuElements
        addObject:
            [actionFactory
                actionToOpenInNewWindowWithURL:nodeURL
                                activityOrigin:WindowActivityBookmarksOrigin]];
  }
  [menuElements
      addObject:[actionFactory
                    actionToCopyURL:[[CrURL alloc] initWithGURL:nodeURL]]];
  // Add edit menu item.
  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    [strongSelf editBookmarkNodeWithID:nodeID];
  }];
  [menuElements addObject:editAction];
  // Add share menu item.
  [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                  __strong __typeof(weakSelf) strongSelf = weakSelf;
                  [strongSelf shareURLBookmarkNodeWithID:nodeID
                                               indexPath:indexPath];
                }]];
  // Add delete menu item.
  UIAction* deleteAction = [actionFactory actionToDeleteWithBlock:^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    [strongSelf deleteBookmarkNodeWithID:nodeID
                              userAction:"MobileBookmarkManagerEntryDeleted"];
  }];
  [menuElements addObject:deleteAction];
  // Disable Edit and Delete if the node cannot be edited.
  if (!canEditNode) {
    editAction.attributes = UIMenuElementAttributesDisabled;
    deleteAction.attributes = UIMenuElementAttributesDisabled;
  }
  return [UIMenu menuWithTitle:@"" children:menuElements];
}

// Returns contextual menu for a folder node at `indexPath`.
- (UIMenu*)folderNodeContextualMenuWithIndexPath:(NSIndexPath*)indexPath
                                     canEditNode:(BOOL)canEditNode {
  const BookmarkNode* folderNode = [self nodeAtIndexPath:indexPath];
  const int64_t nodeID = folderNode->id();
  DCHECK_EQ(folderNode->type(), BookmarkNode::FOLDER);
  // Record that this context menu was shown to the user.
  RecordMenuShown(kMenuScenarioHistogramBookmarkFolder);
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramBookmarkFolder];
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  // Add edit menu item.
  __weak __typeof(self) weakSelf = self;
  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    [strongSelf editFolderNodeWithID:nodeID];
  }];
  [menuElements addObject:editAction];
  // Add move menu item.
  UIAction* moveAction = [actionFactory actionToMoveFolderWithBlock:^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    [strongSelf moveBookmarkNodeWithIDs:{nodeID}
                             userAction:"MobileBookmarkManagerMoveToFolder"];
  }];
  [menuElements addObject:moveAction];
  // Disable Edit and Move if the node cannot be edited.
  if (!canEditNode) {
    editAction.attributes = UIMenuElementAttributesDisabled;
    moveAction.attributes = UIMenuElementAttributesDisabled;
  }
  return [UIMenu menuWithTitle:@"" children:menuElements];
}

// Opens the folder move editor for the given node IDs.
- (void)moveBookmarkNodeWithIDs:(const BookmarkNodeIDSet&)nodeIDs
                     userAction:(const char*)userAction {
  DCHECK(!_folderChooserCoordinator);
  DCHECK(nodeIDs.size() > 0);

  bookmark_utils_ios::NodeSet nodes;
  for (int64_t nodeID : nodeIDs) {
    const BookmarkNode* node = [self findNodeByID:nodeID];
    if (node) {
      nodes.insert(node);
    }
  }

  if (nodes.empty()) {
    // While the contextual menu was opened, the nodes might have been removed.
    // If the nodes don't exist anymore, there nothing to do.
    return;
  }

  base::RecordAction(base::UserMetricsAction(userAction));
  const BookmarkNode* editedNode = *(nodes.begin());
  const BookmarkNode* selectedFolder = editedNode->parent();
  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:_browser.get()
                     hiddenNodes:nodes];
  [_folderChooserCoordinator setSelectedFolder:selectedFolder];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

// Deletes `nodeID` if it still exists and records `userAction`.
- (void)deleteBookmarkNodeWithID:(int64_t)nodeID
                      userAction:(const char*)userAction {
  const BookmarkNode* node = [self findNodeByID:nodeID];
  if (!node) {
    // While the contextual menu was opened, the nodes might have been removed.
    // If the nodes don't exist anymore, there nothing to do.
    return;
  }
  bookmark_utils_ios::NodeSet nodes = {node};
  [self deleteBookmarkNodes:nodes userAction:userAction];
}

// Deletes the `nodes` and records `userAction`.
- (void)deleteBookmarkNodes:(const bookmark_utils_ios::NodeSet&)nodes
                 userAction:(const char*)userAction {
  DCHECK_GE(nodes.size(), 1u);
  base::RecordAction(base::UserMetricsAction(userAction));
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                              nodes, _bookmarkModel.get(), self.profile,
                              FROM_HERE)];
  [self setTableViewEditing:NO];
}

// Ensures bookmarkInteractionController is set.
- (void)ensureBookmarksCoordinator {
  if (!self.bookmarksCoordinator) {
    self.bookmarksCoordinator =
        [[BookmarksCoordinator alloc] initWithBrowser:_browser.get()];
    self.bookmarksCoordinator.baseViewController = self;
    self.bookmarksCoordinator.delegate = self;
  }
}

// Opens the editor for `nodeID` node, if it still exists. The node has to be
// a bookmark node.
- (void)editBookmarkNodeWithID:(int64_t)nodeID {
  const BookmarkNode* bookmarkNode = [self findNodeByID:nodeID];
  if (!bookmarkNode) {
    // While the contextual menu was opened, the node might has been removed.
    // If the node doesn't exist anymore, there nothing to do.
    return;
  }
  DCHECK_EQ(bookmarkNode->type(), BookmarkNode::URL);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEditBookmark"));
  [self ensureBookmarksCoordinator];
  [self.bookmarksCoordinator presentEditorForURLNode:bookmarkNode];
}

// Opens the editor for `nodeID` node, if it still exists. The node has to be
// a folder node.
- (void)editFolderNodeWithID:(int64_t)nodeID {
  const BookmarkNode* bookmarkNode = [self findNodeByID:nodeID];
  if (!bookmarkNode) {
    // While the contextual menu was opened, the node might has been removed.
    // If the node doesn't exist anymore, there nothing to do.
    return;
  }
  DCHECK_EQ(bookmarkNode->type(), BookmarkNode::FOLDER);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEditFolder"));
  [self ensureBookmarksCoordinator];
  [self.bookmarksCoordinator presentEditorForFolderNode:bookmarkNode];
}

- (void)openAllURLs:(std::vector<GURL>)urls
        inIncognito:(BOOL)inIncognito
             newTab:(BOOL)newTab {
  if (inIncognito) {
    IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
        agentFromScene:_browser.get()->GetSceneState()];
    if (reauthAgent.authenticationRequired) {
      __weak BookmarksHomeViewController* weakSelf = self;
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success) {
              [weakSelf openAllURLs:urls inIncognito:inIncognito newTab:newTab];
            }
          }];
      return;
    }
  }

  [self cacheIndexPathRow];
  [self.homeDelegate bookmarkHomeViewControllerWantsDismissal:self
                                             navigationToUrls:urls
                                                  inIncognito:inIncognito
                                                       newTab:newTab];
}

- (void)showBatchUploadDialog:(CGRect)targetRect {
  if (self.actionSheetCoordinator) {
    return;
  }
  __weak BookmarksHomeViewController* weakSelf = self;
  [self.mediator queryLocalBookmarks:^(int local_bookmarks_count,
                                       std::string user_email) {
    BookmarksHomeViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    NSString* alertTitle = l10n_util::GetPluralNSStringF(
        IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_TITLE, local_bookmarks_count);
    NSString* alertDescription = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_DESCRIPTION),
            "count", local_bookmarks_count, "email", user_email));
    // queryLocalBookmarks() should execute the callback almost immediately.
    // This CHECK ensures that the action sheet coordinator is never opened
    // twice.
    CHECK(!self.actionSheetCoordinator);
    strongSelf.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:strongSelf
                           browser:strongSelf->_browser.get()
                             title:alertTitle
                           message:alertDescription
                              rect:targetRect
                              view:strongSelf.tableView];
    // Create the confirm button.
    [strongSelf.actionSheetCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_BUTTON)
                  action:^{
                    base::RecordAction(base::UserMetricsAction(
                        "MobileBookmarksManagerBulkSaveBookmarksToAccountDialog"
                        "Accepted"));
                    [weakSelf triggerBatchUploadFor:local_bookmarks_count
                                          userEmail:std::move(user_email)];
                  }
                   style:UIAlertActionStyleDefault];

    // Create the cancel button.
    [strongSelf.actionSheetCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_ALERT_CANCEL)
                  action:^{
                    base::RecordAction(base::UserMetricsAction(
                        "MobileBookmarksManagerBulkSaveBookmarksToAccountDialog"
                        "Cancelled"));
                    [weakSelf dismissActionSheetCoordinator];
                  }
                   style:UIAlertActionStyleCancel];

    // Show the alert.
    [strongSelf.actionSheetCoordinator start];
  }];
}

- (void)triggerBatchUploadFor:(int)localBookmarksCount
                    userEmail:(std::string)userEmail {
  [self dismissActionSheetCoordinator];
  [self.mediator triggerBatchUpload];

  base::UmaHistogramCounts100000(
      "IOS.Bookmarks.BulkSaveBookmarksInAccountCount", localBookmarksCount);

  [self refreshContents];

  NSString* snackbarMessage = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_IOS_BOOKMARKS_HOME_BULK_UPLOAD_SNACKBAR_MESSAGE),
          "count", localBookmarksCount, "email", userEmail));
  [self.snackbarCommandsHandler showSnackbarWithMessage:snackbarMessage
                                             buttonText:nil
                                          messageAction:nil
                                       completionAction:nil];
}

#pragma mark - Navigation Bar Callbacks

- (void)navigationBarCancel:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileBookmarkManagerClose"));
  [self navigateAway];
  [self dismissWithURL:GURL()];
}

#pragma mark - More Private Methods

- (void)handleSelectUrlForNavigation:(const GURL&)url {
  [self dismissWithURL:url];
}

- (void)handleSelectFolderForNavigation:(const BookmarkNode*)folder {
  if (!self.mediator.currentlyShowingSearchResults) {
    BookmarksHomeViewController* controller =
        [self createControllerWithDisplayedFolderNode:folder];
    [self.navigationController pushViewController:controller animated:YES];
    return;
  }
  [self jumpToFolder:folder];
}

- (void)jumpToFolder:(const BookmarkNode*)folder {
  // Clear bookmark path cache.
  if (_isBeingDismissed) {
    // The navigation controller is being dismissed.
    // Do not open more views.
    return;
  }
  int64_t unusedFolderId;
  int unusedIndexPathRow;
  PrefService* prefService = self.profile->GetPrefs();
  while ([BookmarkPathCache
      bookmarkTopMostRowCacheWithPrefService:prefService
                               bookmarkModel:_bookmarkModel.get()
                                    folderId:&unusedFolderId
                                  topMostRow:&unusedIndexPathRow]) {
    [BookmarkPathCache clearBookmarkTopMostRowCacheWithPrefService:prefService];
  }

  // Rebuild folder controller list, going back up the tree.
  NSMutableArray<BookmarksHomeViewController*>* stack = [NSMutableArray array];
  std::vector<const BookmarkNode*> nodes;
  const BookmarkNode* cursor = folder;
  while (cursor) {
    // Build reversed list of nodes to restore bookmark path below.
    nodes.insert(nodes.begin(), cursor);

    // Build reversed list of controllers.
    BookmarksHomeViewController* controller =
        [self createControllerWithDisplayedFolderNode:cursor];
    [stack insertObject:controller atIndex:0];

    // Setup now, so that the back button labels shows parent folder
    // title and that we don't show large title everywhere.
    [self setupNavigationForBookmarksHomeViewController:controller
                                      usingBookmarkNode:cursor];

    cursor = cursor->parent();
  }

  // Reconstruct bookmark path cache.
  for (const BookmarkNode* node : nodes) {
    [BookmarkPathCache
        cacheBookmarkTopMostRowWithPrefService:prefService
                                      folderId:node->id()
                                     inStorage:bookmark_utils_ios::
                                                   GetBookmarkStorageType(
                                                       node,
                                                       _bookmarkModel.get())
                                    topMostRow:0];
  }

  [self navigateAway];

  // At root, since there's a large title, the search bar is lower than on
  // whatever destination folder it is transitioning to (root is never
  // reachable through search). To avoid a kink in the animation, the title
  // is set to regular size, which means the search bar is at same level at
  // beginning and end of animation. This controller will be replaced in
  // `stack` so there's no need to care about restoring this.
  if ([self isDisplayingBookmarkRoot]) {
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
  }

  __weak BookmarksHomeViewController* weakSelf = self;
  auto completion = ^{
    NSArray<__kindof UIViewController*>* previousStack =
        weakSelf.navigationController.viewControllers;
    [weakSelf.navigationController setViewControllers:stack animated:YES];
    for (UIViewController* controller in previousStack) {
      BookmarksHomeViewController* bookmarksHomeViewController =
          base::apple::ObjCCastStrict<BookmarksHomeViewController>(controller);
      [bookmarksHomeViewController shutdown];
    }
  };

  [self.searchController dismissViewControllerAnimated:YES
                                            completion:completion];
}

- (void)handleSelectEditNodes:(const std::set<const BookmarkNode*>&)nodes {
  // Early return if bookmarks table is not in edit mode.
  if (!self.mediator.currentlyInEditMode) {
    return;
  }

  if (nodes.empty()) {
    // if nothing to select, exit edit mode.
    if (![self hasBookmarksOrFolders]) {
      [self setTableViewEditing:NO];
      return;
    }
    [self setContextBarState:BookmarksContextBarBeginSelection];
    return;
  }

  if (nodes.size() == 1) {
    const BookmarkNode* node = *nodes.begin();
    if (node->is_url()) {
      [self setContextBarState:BookmarksContextBarSingleURLSelection];
    } else {
      DCHECK_EQ(node->type(), BookmarkNode::FOLDER);
      [self setContextBarState:BookmarksContextBarSingleFolderSelection];
    }
    return;
  }

  BOOL foundURL = NO;
  BOOL foundFolder = NO;
  for (const BookmarkNode* node : nodes) {
    if (!foundURL && node->is_url()) {
      foundURL = YES;
    } else if (!foundFolder && node->is_folder()) {
      foundFolder = YES;
    }
    // Break early, if we found both types of nodes.
    if (foundURL && foundFolder) {
      break;
    }
  }

  // Only URLs are selected.
  if (foundURL && !foundFolder) {
    [self setContextBarState:BookmarksContextBarMultipleURLSelection];
    return;
  }
  // Only Folders are selected.
  if (!foundURL && foundFolder) {
    [self setContextBarState:BookmarksContextBarMultipleFolderSelection];
    return;
  }
  // Mixed selection.
  if (foundURL && foundFolder) {
    [self setContextBarState:BookmarksContextBarMixedSelection];
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

- (void)handleMoveNode:(const BookmarkNode*)node toPosition:(size_t)position {
  [self.snackbarCommandsHandler
      showSnackbarMessage:
          bookmark_utils_ios::UpdateBookmarkPositionWithUndoToast(
              node, self.displayedFolderNode, position, _bookmarkModel.get(),
              self.profile)];
}

- (void)handleRefreshContextBar {
  // At default state, the enable state of context bar buttons could change
  // during refresh.
  if (self.contextBarState == BookmarksContextBarDefault) {
    [self setBookmarksContextBarButtonsDefaultState];
  }
}

- (BOOL)isAtTopOfNavigation {
  return (self.navigationController.topViewController == self);
}

#pragma mark - BookmarkTableCellTitleEditDelegate

- (void)textDidChangeTo:(NSString*)newName {
  DCHECK(self.mediator.editingFolderNode);
  self.mediator.addingNewFolder = NO;
  if (newName.length > 0) {
    _bookmarkModel->SetTitle(self.mediator.editingFolderNode,
                             base::SysNSStringToUTF16(newName),
                             bookmarks::metrics::BookmarkEditSource::kUser);
  }
  self.mediator.editingFolderNode = nullptr;
  self.editingFolderCell = nil;
  [self refreshContents];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);

  // Copy the list of edited nodes from BookmarksFolderChooserCoordinator
  // as the reference may become invalid when `_folderChooserCoordinator`
  // is set to nil (if `self` holds the last reference to the object).
  std::set<const BookmarkNode*> editedNodesSet =
      _folderChooserCoordinator.editedNodes;
  // TODO(crbug.com/40268466): Change the type of `editedNodes` to std::vector.
  std::vector<const BookmarkNode*> editedNodesVector(editedNodesSet.begin(),
                                                     editedNodesSet.end());
  [self stopFolderChooserCoordinator];

  DCHECK(!folder->is_url());
  DCHECK_GE(editedNodesVector.size(), 1u);

  [self setTableViewEditing:NO];
  ProfileIOS* profile = self.profile;
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::MoveBookmarksWithUndoToast(
                              editedNodesVector, _bookmarkModel.get(), folder,
                              profile,
                              AuthenticationServiceFactory::GetForProfile(
                                  profile)
                                  ->GetWeakPtr(),
                              SyncServiceFactory::GetForProfile(profile))];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopFolderChooserCoordinator];
  [self setTableViewEditing:NO];
}

#pragma mark - BookmarksCoordinatorDelegate

- (void)bookmarksCoordinatorWillCommitTitleOrURLChange:
    (BookmarksCoordinator*)coordinator {
  [self setTableViewEditing:NO];
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModelLoaded {
  DCHECK(!self.displayedFolderNode);
  self.displayedFolderNode = _bookmarkModel->root_node();

  // If the view hasn't loaded yet, then return early. The eventual call to
  // viewDidLoad will properly initialize the views.  This early return must
  // come *after* setting displayedFolderNode above.
  if (![self isViewLoaded]) {
    return;
  }

  int64_t unusedFolderId;
  int unusedIndexPathRow;
  // Bookmark Model is loaded after presenting Bookmarks,  we need to check
  // again here if restoring of cache position is needed.  It is to prevent
  // crbug.com/765503.
  if ([BookmarkPathCache
          bookmarkTopMostRowCacheWithPrefService:self.profile->GetPrefs()
                                   bookmarkModel:_bookmarkModel.get()
                                        folderId:&unusedFolderId
                                      topMostRow:&unusedIndexPathRow]) {
    self.isReconstructingFromCache = YES;
  }

  DCHECK(self.spinnerView);
  __weak BookmarksHomeViewController* weakSelf = self;
  [self.spinnerView stopWaitingWithCompletion:^{
    // Early return if the controller has been deallocated.
    BookmarksHomeViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [UIView animateWithDuration:0.2f
        animations:^{
          weakSelf.spinnerView.alpha = 0.0;
        }
        completion:^(BOOL finished) {
          BookmarksHomeViewController* innerStrongSelf = weakSelf;
          if (!innerStrongSelf) {
            return;
          }

          // By the time completion block is called, the backgroundView could be
          // another view, like the empty view background. Only clear the
          // background if it is still the spinner.
          if (innerStrongSelf.tableView.backgroundView ==
              innerStrongSelf.spinnerView) {
            innerStrongSelf.tableView.backgroundView = nil;
          }
          innerStrongSelf.spinnerView = nil;
        }];
    [strongSelf loadBookmarkViews];
    [strongSelf.tableView reloadData];
  }];
}

- (void)didChangeNode:(const BookmarkNode*)bookmarkNode {
  // No-op here.  Bookmarks might be refreshed in BookmarksHomeMediator.
}

- (void)didChangeChildrenForNode:(const BookmarkNode*)bookmarkNode {
  // No-op here.  Bookmarks might be refreshed in BookmarksHomeMediator.
}

- (void)didMoveNode:(const BookmarkNode*)bookmarkNode
         fromParent:(const BookmarkNode*)oldParent
           toParent:(const BookmarkNode*)newParent {
  // No-op here.  Bookmarks might be refreshed in BookmarksHomeMediator.
}

- (void)didDeleteNode:(const BookmarkNode*)node
           fromFolder:(const BookmarkNode*)folder {
  if (self.displayedFolderNode == node) {
    [self setTableViewEditing:NO];
  }
}

- (void)bookmarkModelRemovedAllNodes {
  // No-op
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  if ([self isDisplayingBookmarkRoot]) {
    [self navigationBarCancel:self];
  } else {
    [self back];
  }
  return YES;
}

#pragma mark - private

// Returns the profile.
- (ProfileIOS*)profile {
  if (Browser* browser = _browser.get()) {
    return browser->GetProfile()->GetOriginalProfile();
  }
  return nullptr;
}

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

// Stops the folder chooser coordinator.
- (void)stopFolderChooserCoordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

- (BOOL)isDisplayingBookmarkRoot {
  return self.displayedFolderNode == _bookmarkModel->root_node();
}

// Check if any of our controller is presenting. We don't consider when this
// controller is presenting the search controller.
// Note that when adding a controller that can present, it should be added in
// context here.
- (BOOL)isAnyControllerPresenting {
  return (([self presentedViewController] &&
           [self presentedViewController] != self.searchController) ||
          [self.searchController presentedViewController] ||
          [self.navigationController presentedViewController]);
}

- (void)setupUIStackCacheIfApplicable {
  self.isReconstructingFromCache = NO;

  NSArray<BookmarksHomeViewController*>* replacementViewControllers =
      [self cachedViewControllerStack];
  DCHECK(replacementViewControllers);
  [self.navigationController setViewControllers:replacementViewControllers];
}

// Set up context bar for the new UI.
- (void)setupContextBar {
  if (_isShutDown) {
    return;
  }
  if (![self isDisplayingBookmarkRoot] ||
      self.mediator.currentlyShowingSearchResults) {
    self.navigationController.toolbarHidden = NO;
    [self setContextBarState:BookmarksContextBarDefault];
  } else {
    self.navigationController.toolbarHidden = YES;
  }
}

// Set up navigation bar for `viewController`'s navigationBar using `node`.
- (void)setupNavigationForBookmarksHomeViewController:
            (BookmarksHomeViewController*)viewController
                                    usingBookmarkNode:
                                        (const BookmarkNode*)node {
  viewController.navigationItem.leftBarButtonItem.action = @selector(back);
  // Disable large titles on every VC but the root controller.
  if (node != _bookmarkModel->root_node()) {
    viewController.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
  }

  // Add custom title.
  viewController.title = bookmark_utils_ios::TitleForBookmarkNode(node);

  // Add custom done button.
  viewController.navigationItem.rightBarButtonItem =
      [self customizedDoneButton];
}

// Back button callback for the new ui.
- (void)back {
  [self navigateAway];
  [self.navigationController popViewControllerAnimated:YES];
}

- (UIBarButtonItem*)customizedDoneButton {
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithTitle:GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(navigationBarCancel:)];
  doneButton.accessibilityLabel =
      GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
  doneButton.accessibilityIdentifier =
      kBookmarksHomeNavigationBarDoneButtonIdentifier;
  return doneButton;
}

// Saves the current position and asks the delegate to open the url, if delegate
// is set, otherwise opens the URL using URL loading service.
- (void)dismissWithURL:(const GURL&)url {
  [self cacheIndexPathRow];
  if (self.homeDelegate) {
    std::vector<GURL> urls;
    if (url.is_valid()) {
      urls.push_back(url);
    }
    [self.homeDelegate bookmarkHomeViewControllerWantsDismissal:self
                                               navigationToUrls:urls];
  } else {
    // Before passing the URL to the block, make sure the block has a copy of
    // the URL and not just a reference.
    const GURL localUrl(url);
    __weak BookmarksHomeViewController* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf loadURL:localUrl];
    });
  }
}

- (void)loadURL:(const GURL&)url {
  if (url.is_empty() || url.SchemeIs(url::kJavaScriptScheme)) {
    return;
  }

  bool is_ntp = self.webStateList->GetActiveWebState()->GetVisibleURL() ==
                kChromeUINewTabURL;
  new_tab_page_uma::RecordNTPAction(self.profile->IsOffTheRecord(), is_ntp,
                                    new_tab_page_uma::ACTION_OPENED_BOOKMARK);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));

  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(_browser.get())->Load(params);
}

- (void)addNewFolder {
  [self.editingFolderCell stopEdit];
  if (!self.mediator.displayedNode) {
    return;
  }
  self.mediator.addingNewFolder = YES;
  std::u16string folderTitle =
      l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME);
  self.mediator.editingFolderNode = _bookmarkModel->AddFolder(
      self.mediator.displayedNode,
      self.mediator.displayedNode->children().size(), folderTitle);

  BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
      initWithType:BookmarksHomeItemTypeBookmark
      bookmarkNode:self.mediator.editingFolderNode];
  nodeItem.shouldDisplayCloudSlashIcon = [self.mediator
      shouldDisplayCloudSlashIconWithBookmarkNode:self.mediator.displayedNode];
  [self.tableViewModel addItem:nodeItem
       toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];

  // Insert the new folder cell at the end of the table.
  NSIndexPath* newRowIndexPath =
      [self.tableViewModel indexPathForItem:nodeItem];
  NSMutableArray* newRowIndexPaths =
      [[NSMutableArray alloc] initWithObjects:newRowIndexPath, nil];
  [self.tableView beginUpdates];
  [self.tableView insertRowsAtIndexPaths:newRowIndexPaths
                        withRowAnimation:UITableViewRowAnimationNone];
  [self.tableView endUpdates];

  // Scroll to the end of the table
  [self.tableView scrollToRowAtIndexPath:newRowIndexPath
                        atScrollPosition:UITableViewScrollPositionBottom
                                animated:YES];
}

- (BookmarksHomeViewController*)createControllerWithDisplayedFolderNode:
    (const BookmarkNode*)displayedFolderNode {
  BookmarksHomeViewController* controller =
      [[BookmarksHomeViewController alloc] initWithBrowser:_browser.get()];
  controller.displayedFolderNode = displayedFolderNode;
  controller.homeDelegate = self.homeDelegate;
  controller.applicationCommandsHandler = self.applicationCommandsHandler;
  controller.snackbarCommandsHandler = self.snackbarCommandsHandler;

  return controller;
}

// Row selection of the tableView will be cleared after reloadData.  This
// function is used to restore the row selection.  It also updates
// selectedNodesForEditMode in case some selected nodes are removed.
- (void)restoreRowSelection {
  // Create a new selectedNodesForEditMode set to check if some selected nodes
  // are removed.
  std::set<const BookmarkNode*> newEditNodes;

  // Add selected nodes to selectedNodesForEditMode only if they are not removed
  // (still exist in the table).
  NSArray<TableViewItem*>* items = [self.tableViewModel
      itemsInSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    BookmarksHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    const BookmarkNode* node = nodeItem.bookmarkNode;
    if (base::Contains(self.mediator.selectedNodesForEditMode, node)) {
      newEditNodes.insert(node);
      // Reselect the row of this node.
      NSIndexPath* itemPath = [self.tableViewModel indexPathForItem:nodeItem];
      [self.tableView selectRowAtIndexPath:itemPath
                                  animated:NO
                            scrollPosition:UITableViewScrollPositionNone];
    }
  }

  // if selectedNodesForEditMode is changed, update it.
  if (self.mediator.selectedNodesForEditMode.size() != newEditNodes.size()) {
    self.mediator.selectedNodesForEditMode = newEditNodes;
    [self handleSelectEditNodes:self.mediator.selectedNodesForEditMode];
  }
}

- (BOOL)allowsNewFolder {
  // When the current root node has been removed remotely (becomes NULL),
  // or when displaying search results, creating new folder is forbidden.
  // The root folder displayed by the table view must also be editable to allow
  // creation of new folders. Note that Bookmarks Bar, Mobile Bookmarks, and
  // Other Bookmarks return as "editable" since the user can edit the contents
  // of those folders. Editing bookmarks must also be allowed.
  return self.mediator.displayedNode != NULL &&
         !self.mediator.currentlyShowingSearchResults &&
         [self isEditBookmarksEnabled] &&
         [self isNodeEditableByUser:self.mediator.displayedNode];
}

- (int)topMostVisibleIndexPathRow {
  // If on root node screen, return 0.
  if (_bookmarkModel && [self isDisplayingBookmarkRoot]) {
    return 0;
  }

  // If no rows in table, return 0.
  NSArray* visibleIndexPaths = [self.tableView indexPathsForVisibleRows];
  if (!visibleIndexPaths.count) {
    return 0;
  }

  // Return the first visible row.
  NSIndexPath* topMostIndexPath = [visibleIndexPaths objectAtIndex:0];
  return topMostIndexPath.row;
}

- (void)navigateAway {
  [self.editingFolderCell stopEdit];
}

// Returns YES if the given node is a url or folder node.
- (BOOL)isUrlOrFolder:(const BookmarkNode*)node {
  return node->type() == BookmarkNode::URL ||
         node->type() == BookmarkNode::FOLDER;
}

// Returns YES if the given node can be edited by user.
- (BOOL)isNodeEditableByUser:(const BookmarkNode*)node {
  // Note that IsNodeManaged() below returns false for Bookmarks Bar, Mobile
  // Bookmarks, and Other Bookmarks since the user can add, delete, and edit
  // items within those folders. IsNodeManaged() returns true for the
  // managed_node and all nodes that are descendants of managed_node.
  bookmarks::ManagedBookmarkService* managedBookmarkService =
      ManagedBookmarkServiceFactory::GetForProfile(self.profile);
  return managedBookmarkService ? !managedBookmarkService->IsNodeManaged(node)
                                : YES;
}

// Returns YES if user is allowed to edit any bookmarks.
- (BOOL)isEditBookmarksEnabled {
  ProfileIOS* profile = self.profile;
  if (!profile) {
    // The view is being closed.
    return NO;
  }
  return profile->GetPrefs()->GetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled);
}

// Returns the bookmark node associated with `indexPath`.
- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == BookmarksHomeItemTypeBookmark) {
    BookmarksHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    return nodeItem.bookmarkNode;
  }

  DUMP_WILL_BE_NOTREACHED() << "Unexpected item type " << item.type;
  return nullptr;
}

- (BOOL)hasItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self.tableViewModel hasItemAtIndexPath:indexPath];
}

// Whether the view is currently displaying bookmarks or folders.
- (BOOL)hasBookmarksOrFolders {
  if (!self.mediator.displayedNode) {
    return NO;
  }
  if (self.mediator.currentlyShowingSearchResults) {
    return [self
        hasItemsInSectionIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  } else {
    return !self.mediator.displayedNode->children().empty();
  }
}

- (BOOL)hasItemsInSectionIdentifier:(NSInteger)sectionIdentifier {
  BOOL hasSection =
      [self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier];
  if (!hasSection) {
    return NO;
  }
  NSInteger section =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  return [self.tableViewModel numberOfItemsInSection:section] > 0;
}

- (std::vector<const BookmarkNode*>)selectedNodesForEditMode {
  std::vector<const BookmarkNode*> nodes;
  if (self.mediator.currentlyShowingSearchResults) {
    // Create a vector of edit nodes in the same order as the selected nodes.
    base::ranges::copy(self.mediator.selectedNodesForEditMode,
                       std::back_inserter(nodes));
  } else {
    // Create a vector of edit nodes in the same order as the nodes in folder.
    for (const auto& child : self.mediator.displayedNode->children()) {
      if (base::Contains(self.mediator.selectedNodesForEditMode, child.get())) {
        nodes.push_back(child.get());
      }
    }
  }
  return nodes;
}

// Dismiss the search controller when there's a touch event on the scrim.
- (void)dismissSearchController:(UIControl*)sender {
  if (self.searchController.active) {
    self.searchController.active = NO;
  }
}

// Show scrim overlay and hide toolbar.
- (void)showScrim {
  self.navigationController.toolbarHidden = YES;
  self.scrimView.alpha = 0.0f;
  [self.tableView addSubview:self.scrimView];
  // We attach our constraints to the superview because the tableView is
  // a scrollView and it seems that we get an empty frame when attaching to it.
  AddSameConstraints(self.scrimView, self.view.superview);
  self.tableView.accessibilityElementsHidden = YES;
  self.tableView.scrollEnabled = NO;
  __weak BookmarksHomeViewController* weakSelf = self;
  [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
                   animations:^{
                     BookmarksHomeViewController* strongSelf = weakSelf;
                     if (!strongSelf) {
                       return;
                     }
                     strongSelf.scrimView.alpha = 1.0f;
                     [strongSelf.view layoutIfNeeded];
                   }];
}

// Hide scrim and restore toolbar.
- (void)hideScrim {
  __weak BookmarksHomeViewController* weakSelf = self;
  [UIView animateWithDuration:kTableViewNavigationScrimFadeDuration
      animations:^{
        weakSelf.scrimView.alpha = 0.0f;
      }
      completion:^(BOOL finished) {
        BookmarksHomeViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf.scrimView removeFromSuperview];
        strongSelf.tableView.accessibilityElementsHidden = NO;
        strongSelf.tableView.scrollEnabled = YES;
      }];
  [self setupContextBar];
}

- (BOOL)scrimIsVisible {
  return self.scrimView.superview ? YES : NO;
}

// Returns a bookmark node (URL or folder) with `nodeID`, or nil if no node
// exists with such ID.
- (const BookmarkNode*)findNodeByID:(int64_t)nodeID {
  return bookmarks::GetBookmarkNodeByID(_bookmarkModel.get(), nodeID);
}

// Triggers the URL sharing flow for `bookmarkNodeID` node, if it still exists.
- (void)shareURLBookmarkNodeWithID:(int64_t)nodeID
                         indexPath:(NSIndexPath*)indexPath {
  const BookmarkNode* bookmarkNode = [self findNodeByID:nodeID];
  if (!bookmarkNode) {
    // While the contextual menu was opened, the node might has been removed.
    // If the node doesn't exist anymore, there nothing to do.
    return;
  }
  DCHECK(bookmarkNode->is_url());
  GURL bookmarkURL = bookmarkNode->url();
  NSString* title = bookmark_utils_ios::TitleForBookmarkNode(bookmarkNode);
  SharingParams* params =
      [[SharingParams alloc] initWithURL:bookmarkURL
                                   title:title
                                scenario:SharingScenario::BookmarkEntry];
  UIView* cellView = [self.tableView cellForRowAtIndexPath:indexPath];
  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self
                                                     browser:_browser.get()
                                                      params:params
                                                  originView:cellView];
  [self.sharingCoordinator start];
}

// Returns whether the incognito mode is forced.
- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(self.profile->GetPrefs());
}

// Returns whether the incognito mode is available.
- (BOOL)isIncognitoAvailable {
  return !IsIncognitoModeDisabled(self.profile->GetPrefs());
}

#pragma mark - Loading and Empty States

// Shows loading spinner background view.
- (void)showLoadingSpinnerBackground {
  if (!self.spinnerView) {
    self.spinnerView =
        [[HomeWaitingView alloc] initWithFrame:self.tableView.bounds
                               backgroundColor:UIColor.clearColor];
    [self.spinnerView startWaiting];
  }
  self.tableView.backgroundView = self.spinnerView;
}

// Hide the loading spinner if it is showing.
- (void)hideLoadingSpinnerBackground {
  if (self.spinnerView) {
    __weak BookmarksHomeViewController* weakSelf = self;
    [self.spinnerView stopWaitingWithCompletion:^{
      [UIView animateWithDuration:0.2
          animations:^{
            weakSelf.spinnerView.alpha = 0.0;
          }
          completion:^(BOOL finished) {
            BookmarksHomeViewController* strongSelf = weakSelf;
            if (!strongSelf) {
              return;
            }
            // By the time completion block is called, the backgroundView could
            // be another view, like the empty view background. Only clear the
            // background if it is still the spinner.
            if (strongSelf.tableView.backgroundView == strongSelf.spinnerView) {
              strongSelf.tableView.backgroundView = nil;
            }
            strongSelf.spinnerView = nil;
          }];
    }];
  }
}

// Shows empty bookmarks background view.
- (void)showEmptyBackground {
  if (!self.emptyViewBackground) {
    self.emptyViewBackground = [[TableViewIllustratedEmptyView alloc]
        initWithFrame:self.tableView.bounds
                image:[UIImage imageNamed:@"bookmark_empty"]
                title:GetNSString(IDS_IOS_BOOKMARK_EMPTY_TITLE)
             subtitle:GetNSString(IDS_IOS_BOOKMARK_EMPTY_MESSAGE)];
  }
  // If the Signin promo is visible on the root view, we have to shift the
  // empty TableView background to make it fully visible on all devices.
  if ([self isDisplayingBookmarkRoot]) {
    // Reload the data to ensure consistency between the model and the table
    // (an example scenario can be found at crbug.com/1116408). Reloading the
    // data should only be done for the root bookmark folder since it can be
    // very expensive in other folders.
    [self.tableView reloadData];

    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    if (self.mediator.promoVisible && self.tableView.visibleCells.count) {
      CGFloat signinPromoHeight =
          self.tableView.visibleCells.firstObject.bounds.size.height;
      self.emptyViewBackground.scrollViewContentInsets =
          UIEdgeInsetsMake(signinPromoHeight, 0.0, 0.0, 0.0);
    } else {
      self.emptyViewBackground.scrollViewContentInsets =
          self.view.safeAreaInsets;
    }
  }

  self.tableView.backgroundView = self.emptyViewBackground;
  self.navigationItem.searchController = nil;
}

- (void)hideEmptyBackground {
  if (self.tableView.backgroundView == self.emptyViewBackground) {
    self.tableView.backgroundView = nil;
  }
  self.navigationItem.searchController = self.searchController;
  if ([self isDisplayingBookmarkRoot]) {
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeAutomatic;
  }
}

#pragma mark - ContextBarDelegate implementation

// Called when the leading button is clicked.
- (void)leadingButtonClicked {
  // Ignore the button tap if any of our controllers is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  const std::set<const BookmarkNode*> nodes =
      self.mediator.selectedNodesForEditMode;
  switch (self.contextBarState) {
    case BookmarksContextBarDefault:
      // New Folder clicked.
      [self addNewFolder];
      break;
    case BookmarksContextBarBeginSelection:
      // This must never happen, as the leading button is disabled at this
      // point.
      NOTREACHED_IN_MIGRATION();
      break;
    case BookmarksContextBarSingleURLSelection:
    case BookmarksContextBarMultipleURLSelection:
    case BookmarksContextBarSingleFolderSelection:
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
      // Delete clicked.
      [self deleteBookmarkNodes:nodes
                     userAction:"MobileBookmarkManagerRemoveSelected"];
      break;
    case BookmarksContextBarNone:
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

// Called when the center button is clicked.
- (void)centerButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  const std::set<const BookmarkNode*> nodes =
      self.mediator.selectedNodesForEditMode;
  // Center button is shown and is clickable only when at least
  // one node is selected.
  DCHECK(nodes.size() > 0);

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser.get()
                           title:nil
                         message:nil
                   barButtonItem:self.moreButton];

  switch (self.contextBarState) {
    case BookmarksContextBarSingleURLSelection:
      [self configureCoordinator:self.actionSheetCoordinator
            forSingleBookmarkURL:*(nodes.begin())];
      break;
    case BookmarksContextBarMultipleURLSelection:
      [self configureCoordinator:self.actionSheetCoordinator
          forMultipleBookmarkURLs:nodes];
      break;
    case BookmarksContextBarSingleFolderSelection:
      [self configureCoordinator:self.actionSheetCoordinator
          forSingleBookmarkFolder:*(nodes.begin())];
      break;
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
      [self configureCoordinator:self.actionSheetCoordinator
          forMixedAndMultiFolderSelection:nodes];
      break;
    case BookmarksContextBarDefault:
    case BookmarksContextBarBeginSelection:
    case BookmarksContextBarNone:
      // Center button is disabled in these states.
      NOTREACHED_IN_MIGRATION();
      break;
  }

  [self addCancelActionToCoordinator:self.actionSheetCoordinator];
  [self.actionSheetCoordinator start];
}

// Called when the trailing button, "Select" or "Cancel" is clicked.
- (void)trailingButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  // Toggle edit mode.
  [self setTableViewEditing:!self.mediator.currentlyInEditMode];
}

// Displays the UITableView edit mode and selects the row containing the
// `_externalBookmark`.
- (void)editExternalBookmarkIfSet {
  if (!_externalBookmark) {
    return;
  }

  [self setTableViewEditing:YES];
  NSArray<NSIndexPath*>* paths = [self.tableViewModel
      indexPathsForItemType:BookmarksHomeItemTypeBookmark
          sectionIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  for (id path in paths) {
    BookmarksHomeNodeItem* node =
        base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(
            [self.tableViewModel itemAtIndexPath:path]);
    if (node.bookmarkNode == _externalBookmark) {
      [self.tableView selectRowAtIndexPath:path
                                  animated:NO
                            scrollPosition:UITableViewScrollPositionMiddle];
      [self.tableView.delegate tableView:self.tableView
                 didSelectRowAtIndexPath:path];
      break;
    }
  }
}

#pragma mark - ContextBarStates

// Customizes the context bar buttons based the `state` passed in.
- (void)setContextBarState:(BookmarksContextBarState)state {
  _contextBarState = state;
  switch (state) {
    case BookmarksContextBarDefault:
      [self setBookmarksContextBarButtonsDefaultState];
      break;
    case BookmarksContextBarBeginSelection:
      [self setBookmarksContextBarSelectionStartState];
      break;
    case BookmarksContextBarSingleURLSelection:
    case BookmarksContextBarMultipleURLSelection:
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
    case BookmarksContextBarSingleFolderSelection:
      // Reset to start state, and then override with customizations that apply.
      [self setBookmarksContextBarSelectionStartState];
      self.moreButton.enabled = YES;
      self.deleteButton.enabled = YES;
      break;
    case BookmarksContextBarNone:
    default:
      break;
  }
}

- (void)setBookmarksContextBarButtonsDefaultState {
  // Set New Folder button
  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER);
  UIBarButtonItem* newFolderButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(leadingButtonClicked)];
  newFolderButton.accessibilityIdentifier =
      kBookmarksHomeLeadingButtonIdentifier;
  newFolderButton.enabled = [self allowsNewFolder];

  // Spacer button.
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  // Set Edit button.
  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_EDIT);
  UIBarButtonItem* editButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(trailingButtonClicked)];
  editButton.accessibilityIdentifier = kBookmarksHomeTrailingButtonIdentifier;
  // The edit button is only enabled if the displayed root folder is editable
  // and has items. Note that Bookmarks Bar, Mobile Bookmarks, and Other
  // Bookmarks return as "editable" since their contents can be edited. Editing
  // bookmarks must also be allowed.
  editButton.enabled = [self isEditBookmarksEnabled] &&
                       [self hasBookmarksOrFolders] &&
                       [self isNodeEditableByUser:self.mediator.displayedNode];

  [self setToolbarItems:@[ newFolderButton, spaceButton, editButton ]
               animated:NO];
}

- (void)setBookmarksContextBarSelectionStartState {
  // Disabled Delete button.
  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE);
  self.deleteButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(leadingButtonClicked)];
  self.deleteButton.tintColor = [UIColor colorNamed:kRedColor];
  self.deleteButton.enabled = NO;
  self.deleteButton.accessibilityIdentifier =
      kBookmarksHomeLeadingButtonIdentifier;

  // Disabled More button.
  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
  self.moreButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(centerButtonClicked)];
  self.moreButton.enabled = NO;
  self.moreButton.accessibilityIdentifier =
      kBookmarksHomeCenterButtonIdentifier;

  // Enabled Cancel button.
  titleString = GetNSString(IDS_CANCEL);
  UIBarButtonItem* cancelButton =
      [[UIBarButtonItem alloc] initWithTitle:titleString
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(trailingButtonClicked)];
  cancelButton.accessibilityIdentifier = kBookmarksHomeTrailingButtonIdentifier;

  // Spacer button.
  UIBarButtonItem* spaceButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  [self setToolbarItems:@[
    self.deleteButton, spaceButton, self.moreButton, spaceButton, cancelButton
  ]
               animated:NO];
}

#pragma mark - Context Menu

- (void)configureCoordinator:(AlertCoordinator*)coordinator
     forMultipleBookmarkURLs:(const std::set<const BookmarkNode*>)nodes {
  __weak BookmarksHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;

  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  if ([strongSelf isIncognitoForced]) {
                    return;
                  }
                  std::vector<const BookmarkNode*> selectedNodesForEditMode =
                      [strongSelf selectedNodesForEditMode];
                  [strongSelf
                      openAllURLs:GetUrlsToOpen(selectedNodesForEditMode)
                      inIncognito:NO
                           newTab:NO];
                }
                 style:UIAlertActionStyleDefault
               enabled:![self isIncognitoForced]];

  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  if (![strongSelf isIncognitoAvailable]) {
                    return;
                  }
                  std::vector<const BookmarkNode*> selectedNodesForEditMode =
                      [strongSelf selectedNodesForEditMode];
                  [strongSelf
                      openAllURLs:GetUrlsToOpen(selectedNodesForEditMode)
                      inIncognito:YES
                           newTab:NO];
                }
                 style:UIAlertActionStyleDefault
               enabled:[self isIncognitoAvailable]];

  const BookmarkNodeIDSet nodeIDs = GetBookmarkNodeIDSet(nodes);

  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  [strongSelf
                      moveBookmarkNodeWithIDs:nodeIDs
                                   userAction:"MobileBookmarkManagerMove"
                                              "ToFolderBulk"];
                }
                 style:UIAlertActionStyleDefault];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
        forSingleBookmarkURL:(const BookmarkNode*)node {
  __weak BookmarksHomeViewController* weakSelf = self;
  const GURL nodeURL = node->url();
  const int64_t nodeID = node->id();
  const std::string urlString = nodeURL.possibly_invalid_spec();
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;
  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT);
  // Disable the edit menu option if the node is not editable by user, or if
  // editing bookmarks is not allowed.
  BOOL editEnabled =
      [self isEditBookmarksEnabled] && [self isNodeEditableByUser:node];

  [coordinator addItemWithTitle:titleString
                         action:^{
                           [weakSelf dismissActionSheetCoordinator];
                           BookmarksHomeViewController* strongSelf = weakSelf;
                           [strongSelf editBookmarkNodeWithID:nodeID];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];

  titleString = GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [coordinator addItemWithTitle:titleString
                         action:^{
                           [weakSelf dismissActionSheetCoordinator];
                           if ([weakSelf isIncognitoForced]) {
                             return;
                           }
                           [weakSelf openAllURLs:{nodeURL}
                                     inIncognito:NO
                                          newTab:YES];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:![self isIncognitoForced]];

  if (base::ios::IsMultipleScenesSupported()) {
    titleString = GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW);
    auto action = ^{
      [weakSelf dismissActionSheetCoordinator];
      [weakSelf.applicationCommandsHandler
          openNewWindowWithActivity:ActivityToLoadURL(
                                        WindowActivityBookmarksOrigin,
                                        nodeURL)];
    };
    [coordinator addItemWithTitle:titleString
                           action:action
                            style:UIAlertActionStyleDefault];
  }

  titleString = GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  [coordinator addItemWithTitle:titleString
                         action:^{
                           [weakSelf dismissActionSheetCoordinator];
                           if (![weakSelf isIncognitoAvailable]) {
                             return;
                           }
                           [weakSelf openAllURLs:{nodeURL}
                                     inIncognito:YES
                                          newTab:YES];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:[self isIncognitoAvailable]];

  titleString = GetNSString(IDS_IOS_CONTENT_CONTEXT_COPY);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  // Use strongSelf even though the object is only used once
                  // because we do not want to change the global pasteboard
                  // if the view has been deallocated.
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  [strongSelf setTableViewEditing:NO];
                  StoreTextInPasteboard(base::SysUTF8ToNSString(urlString));
                }
                 style:UIAlertActionStyleDefault];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
     forSingleBookmarkFolder:(const BookmarkNode*)node {
  __weak BookmarksHomeViewController* weakSelf = self;
  const int64_t nodeID = node->id();
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;

  NSString* titleString =
      GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER);
  // Disable the edit and move menu options if the folder is not editable by
  // user, or if editing bookmarks is not allowed.
  BOOL editEnabled =
      [self isEditBookmarksEnabled] && [self isNodeEditableByUser:node];

  [coordinator addItemWithTitle:titleString
                         action:^{
                           [weakSelf dismissActionSheetCoordinator];
                           BookmarksHomeViewController* strongSelf = weakSelf;
                           [strongSelf editFolderNodeWithID:nodeID];
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];

  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  [strongSelf
                      moveBookmarkNodeWithIDs:{nodeID}
                                   userAction:"MobileBookmarkManagerMove"
                                              "ToFolder"];
                }
                 style:UIAlertActionStyleDefault
               enabled:editEnabled];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
    forMixedAndMultiFolderSelection:(const std::set<const BookmarkNode*>)nodes {
  __weak BookmarksHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;

  const BookmarkNodeIDSet nodeIDs = GetBookmarkNodeIDSet(nodes);
  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  [strongSelf
                      moveBookmarkNodeWithIDs:nodeIDs
                                   userAction:"MobileBookmarkManagerMove"
                                              "ToFolderBulk"];
                }
                 style:UIAlertActionStyleDefault];
}

- (void)addCancelActionToCoordinator:(AlertCoordinator*)coordinator {
  __weak BookmarksHomeViewController* weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];
}

#pragma mark - UIGestureRecognizerDelegate and gesture handling

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  if (gestureRecognizer ==
      self.navigationController.interactivePopGestureRecognizer) {
    return self.navigationController.viewControllers.count > 1;
  }
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Ignore long press in edit mode or search mode.
  if (self.mediator.currentlyInEditMode || [self scrimIsVisible]) {
    return NO;
  }
  return YES;
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.mediator.currentlyInEditMode ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  CGPoint touchPoint = [gestureRecognizer locationInView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:touchPoint];

  if (![self canShowContextMenuFor:indexPath]) {
    return;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser.get()
                           title:nil
                         message:nil
                            rect:CGRectMake(touchPoint.x, touchPoint.y, 1, 1)
                            view:self.tableView];

  if (node->is_url()) {
    [self configureCoordinator:self.actionSheetCoordinator
          forSingleBookmarkURL:node];
  } else if (node->is_folder()) {
    [self configureCoordinator:self.actionSheetCoordinator
        forSingleBookmarkFolder:node];
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  [self addCancelActionToCoordinator:self.actionSheetCoordinator];
  [self.actionSheetCoordinator start];
}

- (BOOL)canShowContextMenuFor:(NSIndexPath*)indexPath {
  if (indexPath == nil ||
      [self.tableViewModel
          sectionIdentifierForSectionIndex:indexPath.section] !=
          BookmarksHomeSectionIdentifierBookmarks) {
    return NO;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  // Don't show context menus for permanent nodes, which include Bookmarks Bar,
  // Mobile Bookmarks, Other Bookmarks, and Managed Bookmarks. Permanent nodes
  // do not include descendants of Managed Bookmarks. Also, context menus are
  // only supported on URLs or folders.
  return node && !node->is_permanent_node() &&
         (node->is_url() || node->is_folder());
}

#pragma mark UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  DCHECK_EQ(self.searchController, searchController);
  NSString* text = searchController.searchBar.text;
  self.searchTerm = text;

  if (text.length == 0) {
    if (self.mediator.currentlyShowingSearchResults) {
      self.mediator.currentlyShowingSearchResults = NO;
      // Restore current list.
      [self.mediator computeBookmarkTableViewData];
      [self.mediator computePromoTableViewData];
      [self.tableView reloadData];
      [self showScrim];
    }
  } else {
    if (!self.mediator.currentlyShowingSearchResults) {
      self.mediator.currentlyShowingSearchResults = YES;
      [self.mediator computePromoTableViewData];
      [self hideScrim];
    }
    // Replace current list with search result, but doesn't change
    // the 'regular' model for this page, which we can restore when search
    // is terminated.
    NSString* noResults = GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self.mediator computeBookmarkTableViewDataMatching:text
                             orShowMessageWhenNoResults:noResults];
    [self.tableView reloadData];
    [self setupContextBar];
  }
}

#pragma mark UISearchControllerDelegate

- (void)willPresentSearchController:(UISearchController*)searchController {
  [self showScrim];
}

- (void)willDismissSearchController:(UISearchController*)searchController {
  // Avoid scrim being put back on in updateSearchResultsForSearchController.
  self.mediator.currentlyShowingSearchResults = NO;
  // Restore current list.
  [self.mediator computeBookmarkTableViewData];
  [self.tableView reloadData];
}

- (void)didDismissSearchController:(UISearchController*)searchController {
  [self hideScrim];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];

  cell.userInteractionEnabled =
      (item.type != BookmarksHomeItemTypeMessage &&
       item.type != BookmarksHomeItemTypeBatchUploadRecommendation);
  if (item.type == BookmarksHomeItemTypeBatchUploadRecommendation) {
    cell.separatorInset =
        UIEdgeInsetsMake(0.f, kTableViewSeparatorInset, 0.f, 0.f);
  }

  if (item.type == BookmarksHomeItemTypeBookmark) {
    BookmarksHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    if (nodeItem.bookmarkNode->is_folder() &&
        nodeItem.bookmarkNode == self.mediator.editingFolderNode) {
      TableViewBookmarksFolderCell* tableCell =
          base::apple::ObjCCastStrict<TableViewBookmarksFolderCell>(cell);
      // Delay starting edit, so that the cell is fully created. This is
      // needed when scrolling away and then back into the editingCell,
      // without the delay the cell will resign first responder before its
      // created.
      __weak BookmarksHomeViewController* weakSelf = self;
      dispatch_async(dispatch_get_main_queue(), ^{
        BookmarksHomeViewController* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        strongSelf.editingFolderCell = tableCell;
        [tableCell startEdit];
        tableCell.textDelegate = strongSelf;
      });
    }

    // Load the favicon from cache. If not found, try fetching it from a
    // Google Server.
    [self loadFaviconAtIndexPath:indexPath
                         forCell:cell
          fallbackToGoogleServer:YES];
  }

  return cell;
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != BookmarksHomeItemTypeBookmark) {
    // Can only edit bookmarks.
    return NO;
  }

  // If the cell at `indexPath` is being edited (which happens when creating a
  // new Folder) return NO.
  if ([tableView indexPathForCell:self.editingFolderCell] == indexPath) {
    return NO;
  }

  // Enable the swipe-to-delete gesture and reordering control for editable
  // nodes of type URL or Folder, but not the permanent ones. Only enable
  // swipe-to-delete if editing bookmarks is allowed.
  BookmarksHomeNodeItem* nodeItem =
      base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(item);
  const BookmarkNode* node = nodeItem.bookmarkNode;
  return [self isEditBookmarksEnabled] && [self isUrlOrFolder:node] &&
         [self isNodeEditableByUser:node];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != BookmarksHomeItemTypeBookmark) {
    // Can only commit edits for bookmarks.
    return;
  }

  if (editingStyle == UITableViewCellEditingStyleDelete) {
    BookmarksHomeNodeItem* nodeItem =
        base::apple::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    const BookmarkNode* node = nodeItem.bookmarkNode;
    std::set<const BookmarkNode*> nodes;
    nodes.insert(node);
    [self deleteBookmarkNodes:nodes
                   userAction:"MobileBookmarkManagerEntryDeleted"];
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canMoveRowAtIndexPath:(NSIndexPath*)indexPath {
  // No reorering with filtered results or when displaying the top-most
  // Bookmarks node.
  if (self.mediator.currentlyShowingSearchResults ||
      [self isDisplayingBookmarkRoot] || !self.tableView.editing) {
    return NO;
  }
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != BookmarksHomeItemTypeBookmark) {
    // Can only move bookmarks.
    return NO;
  }

  return YES;
}

- (void)tableView:(UITableView*)tableView
    moveRowAtIndexPath:(NSIndexPath*)sourceIndexPath
           toIndexPath:(NSIndexPath*)destinationIndexPath {
  if (sourceIndexPath.row == destinationIndexPath.row ||
      self.mediator.currentlyShowingSearchResults) {
    return;
  }
  const BookmarkNode* node = [self nodeAtIndexPath:sourceIndexPath];
  // Calculations: Assume we have 3 nodes A B C. Node positions are A(0), B(1),
  // C(2) respectively. When we move A to after C, we are moving node at index 0
  // to 3 (position after C is 3, in terms of the existing contents). Hence add
  // 1 when moving forward. When moving backward, if C(2) is moved to Before B,
  // we move node at index 2 to index 1 (position before B is 1, in terms of the
  // existing contents), hence no change in index is necessary. It is required
  // to make these adjustments because this is how bookmark_model handles move
  // operations.
  size_t newPosition = sourceIndexPath.row < destinationIndexPath.row
                           ? destinationIndexPath.row + 1
                           : destinationIndexPath.row;
  [self handleMoveNode:node toPosition:newPosition];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarksHomeSectionIdentifier sectionIdentifier =
      (BookmarksHomeSectionIdentifier)([self.tableViewModel
          sectionIdentifierForSectionIndex:indexPath.section]);
  if (IsABookmarkNodeSectionForIdentifier(sectionIdentifier)) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    // If table is in edit mode, record all the nodes added to edit set.
    if (self.mediator.currentlyInEditMode) {
      if ([self isNodeEditableByUser:node]) {
        // Only add nodes that are editable to the edit set.
        self.mediator.selectedNodesForEditMode.insert(node);
        [self handleSelectEditNodes:self.mediator.selectedNodesForEditMode];
        return;
      }
      // If the selected row is not editable, do not add it to the edit set.
      // Simply deselect the row.
      [tableView deselectRowAtIndexPath:indexPath animated:YES];
      return;
    }
    [self.editingFolderCell stopEdit];
    if (node->is_folder()) {
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerOpenFolder"));
      [self handleSelectFolderForNavigation:node];
    } else {
      if (self.mediator.currentlyShowingSearchResults) {
        // Set the searchController active property to NO or the SearchBar will
        // cause the navigation controller to linger for a second  when
        // dismissing.
        self.searchController.active = NO;
      }
      // Open URL. Pass this to the delegate.
      [self handleSelectUrlForNavigation:node->url()];
    }
  } else if (sectionIdentifier == BookmarksBatchUploadSectionIdentifier) {
    // Open batch upload alert dialog if batch upload button clicked.
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    if (static_cast<BookmarksHomeItemType>(item.type) ==
        BookmarksHomeItemTypeBatchUploadButton) {
      base::RecordAction(base::UserMetricsAction(
          "MobileBookmarksManagerBulkSaveBookmarksToAccountButtonClicked"));
      CGRect targetRect = [tableView rectForRowAtIndexPath:indexPath];
      [self showBatchUploadDialog:targetRect];
    }
  }
  // Deselect row.
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  BookmarksHomeSectionIdentifier sectionIdentifier =
      (BookmarksHomeSectionIdentifier)[self.tableViewModel
          sectionIdentifierForSectionIndex:indexPath.section];
  if (sectionIdentifier == BookmarksHomeSectionIdentifierBookmarks &&
      self.mediator.currentlyInEditMode) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    self.mediator.selectedNodesForEditMode.erase(node);
    [self handleSelectEditNodes:self.mediator.selectedNodesForEditMode];
  }
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (self.mediator.currentlyInEditMode) {
    // Don't show the context menu when currently in editing mode.
    return nil;
  }
  if (![self canShowContextMenuFor:indexPath]) {
    return nil;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];

  // Disable the edit and move menu options if the node is not editable by user,
  // or if editing bookmarks is not allowed.
  BOOL canEditNode =
      [self isEditBookmarksEnabled] && [self isNodeEditableByUser:node];
  UIContextMenuActionProvider actionProvider;

  __weak BookmarksHomeViewController* weakSelf = self;
  if (node->is_url()) {
    actionProvider = ^(NSArray<UIMenuElement*>* suggestedActions) {
      return [weakSelf bookmarkNodeContextualMenuWithIndexPath:indexPath
                                                   canEditNode:canEditNode];
    };
  } else if (node->is_folder()) {
    actionProvider = ^(NSArray<UIMenuElement*>* suggestedActions) {
      return [weakSelf folderNodeContextualMenuWithIndexPath:indexPath
                                                 canEditNode:canEditNode];
    };
  }
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return [self.tableViewModel numberOfItemsInSection:section] == 0
             ? 0
             : UITableViewAutomaticDimension;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  // Add space between profile and account sections only if both are not empty,
  // to avoid useless space at the end of the account section content.
  if ([self.tableViewModel sectionIdentifierForSectionIndex:section] ==
          BookmarksHomeSectionIdentifierRootAccount &&
      [self hasItemsInSectionIdentifier:
                BookmarksHomeSectionIdentifierRootLocalOrSyncable] &&
      [self hasItemsInSectionIdentifier:
                BookmarksHomeSectionIdentifierRootAccount]) {
    return kSpaceBetweenAccountAndProfileSections;
  } else {
    return 0;
  }
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section !=
      [self.tableViewModel sectionForSectionIdentifier:
                               BookmarksHomeSectionIdentifierBookmarks]) {
    return nil;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  if (!node || node->is_folder()) {
    return nil;
  }
  return [[URLInfo alloc]
      initWithURL:node->url()
            title:bookmark_utils_ios::TitleForBookmarkNode(node)];
}

#pragma mark - TableViewURLDropDelegate

- (BOOL)canHandleURLDropInTableView:(UITableView*)tableView {
  return !self.mediator.currentlyShowingSearchResults &&
         !self.tableView.hasActiveDrag && ![self isDisplayingBookmarkRoot];
}

- (void)tableView:(UITableView*)tableView
       didDropURL:(const GURL&)URL
      atIndexPath:(NSIndexPath*)indexPath {
  NSUInteger index = base::checked_cast<NSUInteger>(indexPath.item);

  [self.snackbarCommandsHandler
      showSnackbarMessage:
          bookmark_utils_ios::CreateBookmarkAtPositionWithUndoToast(
              base::SysUTF8ToNSString(URL.spec()), URL,
              self.displayedFolderNode, index, _bookmarkModel.get(),
              self.profile)];
}

@end

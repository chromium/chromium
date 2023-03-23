// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/numerics/safe_conversions.h"
#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/drag_and_drop/table_view_url_drag_drop_handler.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_home_node_item.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell_title_edit_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/cells/table_view_bookmarks_folder_item.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_shared_state.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/elements/home_waiting_view.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;
using l10n_util::GetNSString;

// Used to store a pair of NSIntegers when storing a NSIndexPath in C++
// collections.
using IntegerPair = std::pair<NSInteger, NSInteger>;

namespace {
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
const CGFloat kEstimatedRowHeight = 65.0;

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

}  // namespace

@interface BookmarksHomeViewController () <
    BookmarksCoordinatorDelegate,
    BookmarksFolderChooserCoordinatorDelegate,
    BookmarksHomeConsumer,
    BookmarksHomeSharedStateObserver,
    BookmarkModelBridgeObserver,
    BookmarkTableCellTitleEditDelegate,
    TableViewURLDragDataSource,
    TableViewURLDropDelegate,
    UIGestureRecognizerDelegate,
    UISearchControllerDelegate,
    UISearchResultsUpdating,
    UITableViewDataSource,
    UITableViewDelegate>

// Shared state between BookmarksHome classes.  Used as a temporary refactoring
// aid.
@property(nonatomic, strong) BookmarksHomeSharedState* sharedState;

// The profile bookmark model used.
@property(nonatomic, assign) bookmarks::BookmarkModel* profileBookmarks;

// The Browser in which bookmarks are presented
// TODO(crbug.com/1423926): Need to convert this property into:
// base::WeakPtr<Browser> _browser.
@property(nonatomic, assign) Browser* browser;

// The user's browser state model used.
@property(nonatomic, assign) ChromeBrowserState* browserState;

// The mediator that provides data for this view controller.
@property(nonatomic, strong) BookmarksHomeMediator* mediator;

// TODO(crbug.com/1402758): Move this to BookmarksHomeCoordinator.
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
  // Bridge to register for bookmark changes in the profile model.
  std::unique_ptr<BookmarkModelBridge> _profileBridge;
  // The bookmark node that was choosen by an entity outside of the Bookmarks UI
  // and is selected when the view is loaded.
  const bookmarks::BookmarkNode* _externalBookmark;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _browserState =
        _browser->GetBrowserState()->GetOriginalChromeBrowserState();
    _webStateList = _browser->GetWebStateList();

    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForBrowserState(_browserState);

    _profileBookmarks =
        ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
            _browserState);

    _profileBridge.reset(new BookmarkModelBridge(self, _profileBookmarks));
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [_bookmarksCoordinator shutdown];
  _bookmarksCoordinator = nil;

  [self.mediator disconnect];
  _sharedState.tableView.dataSource = nil;
  _sharedState.tableView.delegate = nil;
  self.browser = nullptr;
  self.browserState = nullptr;
  _profileBridge.reset();
}

- (void)setExternalBookmark:(const bookmarks::BookmarkNode*)node {
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
  DCHECK(self.profileBookmarks->loaded());
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
          getBookmarkTopMostRowCacheWithPrefService:self.browserState
                                                        ->GetPrefs()
                                              model:self.profileBookmarks
                                           folderId:&cachedFolderID
                                         topMostRow:&cachedIndexPathRow] ||
      cachedFolderID == self.profileBookmarks->root_node()->id()) {
    return stack;
  }

  NSArray<NSNumber*>* path = bookmark_utils_ios::CreateBookmarkPath(
      self.profileBookmarks, cachedFolderID);
  if (!path) {
    return stack;
  }

  DCHECK_EQ(self.profileBookmarks->root_node()->id(),
            [[path firstObject] longLongValue]);
  for (NSUInteger ii = 1; ii < [path count]; ii++) {
    int64_t nodeID = [[path objectAtIndex:ii] longLongValue];
    const BookmarkNode* node =
        bookmark_utils_ios::FindFolderById(self.profileBookmarks, nodeID);
    DCHECK(node);
    // if node is an empty permanent node, stop.
    if (node->children().empty() &&
        IsPrimaryPermanentNode(node, self.profileBookmarks)) {
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

- (void)willDismissBySwipeDown {
  if (self.searchController.active) {
    // Dismiss the keyboard if trying to dismiss the VC so the keyboard doesn't
    // linger until the VC dismissal has completed.
    [self.searchController.searchBar endEditing:YES];
  }
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

  if (self.profileBookmarks->loaded()) {
    [self loadBookmarkViews];
  } else {
    [self showLoadingSpinnerBackground];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  // Set the delegate here to make sure it is working when navigating in the
  // ViewController hierarchy (as each view controller is setting itself as
  // delegate).
  self.navigationController.interactivePopGestureRecognizer.delegate = self;

  // Hide the toolbar if we're displaying the root node.
  if (self.profileBookmarks->loaded() &&
      (![self isDisplayingBookmarkRoot] ||
       self.sharedState.currentlyShowingSearchResults)) {
    self.navigationController.toolbarHidden = NO;
  } else {
    self.navigationController.toolbarHidden = YES;
  }

  // If we navigate back to the root level, we need to make sure the root level
  // folders are created or deleted if needed.
  if ([self isDisplayingBookmarkRoot]) {
    [self refreshContents];
  }
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
  [self.sharedState.editingFolderCell stopEdit];
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

  self.sharedState = [[BookmarksHomeSharedState alloc]
      initWithProfileBookmarkModel:_profileBookmarks
                 displayedRootNode:self.displayedFolderNode];
  self.sharedState.tableViewModel = self.tableViewModel;
  self.sharedState.tableView = self.tableView;
  self.sharedState.observer = self;
  self.sharedState.currentlyShowingSearchResults = NO;

  self.dragDropHandler = [[TableViewURLDragDropHandler alloc] init];
  self.dragDropHandler.origin = WindowActivityBookmarksOrigin;
  self.dragDropHandler.dragDataSource = self;
  self.dragDropHandler.dropDelegate = self;
  self.tableView.dragDelegate = self.dragDropHandler;
  self.tableView.dropDelegate = self.dragDropHandler;
  self.tableView.dragInteractionEnabled = true;

  // Configure the table view.
  self.sharedState.tableView.accessibilityIdentifier =
      kBookmarksHomeTableViewIdentifier;
  self.sharedState.tableView.estimatedRowHeight = kEstimatedRowHeight;
  self.tableView.sectionHeaderHeight = 0;
  // Setting a sectionFooterHeight of 0 will be the same as not having a
  // footerView, which shows a cell separator for the last cell. Removing this
  // line will also create a default footer of height 30.
  self.tableView.sectionFooterHeight = 1;
  self.sharedState.tableView.allowsMultipleSelectionDuringEditing = YES;

  // Create the mediator and hook up the table view.
  self.mediator = [[BookmarksHomeMediator alloc]
      initWithSharedState:self.sharedState
                  browser:_browser
       baseViewController:self.navigationController];
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

  DCHECK(self.profileBookmarks->loaded());
  DCHECK([self isViewLoaded]);
}

- (void)cacheIndexPathRow {
  // Cache IndexPathRow for BookmarkTableView.
  int topMostVisibleIndexPathRow = [self topMostVisibleIndexPathRow];
  if (self.displayedFolderNode) {
    [BookmarkPathCache
        cacheBookmarkTopMostRowWithPrefService:self.browserState->GetPrefs()
                                      folderId:self.displayedFolderNode->id()
                                    topMostRow:topMostVisibleIndexPathRow];
  } else {
    // TODO(crbug.com/1061882):Remove DCHECK once we know the root cause of the
    // bug, for now this will cause a crash on Dev/Canary and we should get
    // breadcrumbs.
    DCHECK(NO);
  }
}

#pragma mark - BookmarksHomeConsumer

- (void)setTableViewEditing:(BOOL)editing {
  self.sharedState.currentlyInEditMode = editing;
  [self setContextBarState:editing ? BookmarksContextBarBeginSelection
                                   : BookmarksContextBarDefault];
  self.searchController.searchBar.userInteractionEnabled = !editing;
  self.searchController.searchBar.alpha =
      editing ? kTableViewNavigationAlphaForDisabledSearchBar : 1.0;

  self.tableView.dragInteractionEnabled = !editing;
}

- (void)refreshContents {
  if (self.sharedState.currentlyShowingSearchResults) {
    NSString* noResults = GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self.mediator computeBookmarkTableViewDataMatching:self.searchTerm
                             orShowMessageWhenNoResults:noResults];
  } else {
    [self.mediator computeBookmarkTableViewData];
  }
  [self handleRefreshContextBar];
  [self.sharedState.editingFolderCell stopEdit];
  [self.sharedState.tableView reloadData];
  if (self.sharedState.currentlyInEditMode &&
      !self.sharedState.editNodes.empty()) {
    [self restoreRowSelection];
  }
}

- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
        fallbackToGoogleServer:(BOOL)fallbackToGoogleServer {
  UITableViewCell* cell =
      [self.sharedState.tableView cellForRowAtIndexPath:indexPath];
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
  const bookmarks::BookmarkNode* node = [self nodeAtIndexPath:indexPath];
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
        base::mac::ObjCCastStrict<TableViewURLCell>(cell);
    [URLCell.faviconView configureWithAttributes:attributes];
  };

  CGFloat desiredFaviconSizeInPoints =
      [BookmarksHomeSharedState desiredFaviconSizePt];
  CGFloat minFaviconSizeInPoints = [BookmarksHomeSharedState minFaviconSizePt];

  self.faviconLoader->FaviconForPageUrl(
      blockURL, desiredFaviconSizeInPoints, minFaviconSizeInPoints,
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
      base::mac::ObjCCast<TableViewSigninPromoItem>(
          [self.sharedState.tableViewModel itemAtIndexPath:indexPath]);
  if (!signinPromoItem) {
    return;
  }

  signinPromoItem.configurator = configurator;
  [self reloadCellsForItems:@[ signinPromoItem ]
           withRowAnimation:UITableViewRowAnimationNone];
}

#pragma mark - Action sheet callbacks

// Opens the folder move editor for the given node.
- (void)moveNodes:(const std::set<const BookmarkNode*>&)nodes {
  DCHECK(!_folderChooserCoordinator);
  DCHECK(nodes.size() > 0);
  const BookmarkNode* editedNode = *(nodes.begin());
  const BookmarkNode* selectedFolder = editedNode->parent();
  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:_browser
                     hiddenNodes:nodes];
  [_folderChooserCoordinator setSelectedFolder:selectedFolder];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

// Deletes the current node.
- (void)deleteNodes:(const std::set<const BookmarkNode*>&)nodes {
  DCHECK_GE(nodes.size(), 1u);
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::DeleteBookmarksWithUndoToast(
                              nodes, self.profileBookmarks, self.browserState)];
  [self setTableViewEditing:NO];
}

// Ensures bookmarkInteractionController is set.
- (void)ensureBookmarksCoordinator {
  if (!self.bookmarksCoordinator) {
    self.bookmarksCoordinator =
        [[BookmarksCoordinator alloc] initWithBrowser:self.browser];
    self.bookmarksCoordinator.baseViewController = self;
    self.bookmarksCoordinator.delegate = self;
  }
}

// Opens the editor on the given URL node.
- (void)editNodeURL:(const BookmarkNode*)node {
  DCHECK(node);
  DCHECK_EQ(node->type(), BookmarkNode::URL);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEditBookmark"));
  [self ensureBookmarksCoordinator];
  [self.bookmarksCoordinator presentEditorForURLNode:node];
}

// Opens the editor on the given Folder node.
- (void)editNodeFolder:(const BookmarkNode*)node {
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEditFolder"));
  DCHECK(node);
  DCHECK_EQ(node->type(), BookmarkNode::FOLDER);
  [self ensureBookmarksCoordinator];
  [self.bookmarksCoordinator presentEditorForFolderNode:node];
}

- (void)openAllURLs:(std::vector<GURL>)urls
        inIncognito:(BOOL)inIncognito
             newTab:(BOOL)newTab {
  if (inIncognito) {
    IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
        agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                           ->GetSceneState()];
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

#pragma mark - Navigation Bar Callbacks

- (void)navigationBarCancel:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileBookmarkManagerClose"));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  [self navigateAway];
  [self dismissWithURL:GURL()];
}

#pragma mark - More Private Methods

- (void)handleSelectUrlForNavigation:(const GURL&)url {
  [self dismissWithURL:url];
}

- (void)handleSelectFolderForNavigation:(const bookmarks::BookmarkNode*)folder {
  if (self.sharedState.currentlyShowingSearchResults) {
    // Clear bookmark path cache.
    int64_t unusedFolderId;
    int unusedIndexPathRow;
    while ([BookmarkPathCache
        getBookmarkTopMostRowCacheWithPrefService:self.browserState->GetPrefs()
                                            model:self.profileBookmarks
                                         folderId:&unusedFolderId
                                       topMostRow:&unusedIndexPathRow]) {
      [BookmarkPathCache
          clearBookmarkTopMostRowCacheWithPrefService:self.browserState
                                                          ->GetPrefs()];
    }

    // Rebuild folder controller list, going back up the tree.
    NSMutableArray<BookmarksHomeViewController*>* stack =
        [NSMutableArray array];
    std::vector<const bookmarks::BookmarkNode*> nodes;
    const bookmarks::BookmarkNode* cursor = folder;
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
    for (const bookmarks::BookmarkNode* node : nodes) {
      [BookmarkPathCache
          cacheBookmarkTopMostRowWithPrefService:self.browserState->GetPrefs()
                                        folderId:node->id()
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
      [weakSelf.navigationController setViewControllers:stack animated:YES];
    };

    [self.searchController dismissViewControllerAnimated:YES
                                              completion:completion];
    return;
  }
  BookmarksHomeViewController* controller =
      [self createControllerWithDisplayedFolderNode:folder];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)handleSelectNodesForDeletion:
    (const std::set<const bookmarks::BookmarkNode*>&)nodes {
  [self deleteNodes:nodes];
}

- (void)handleSelectEditNodes:
    (const std::set<const bookmarks::BookmarkNode*>&)nodes {
  // Early return if bookmarks table is not in edit mode.
  if (!self.sharedState.currentlyInEditMode) {
    return;
  }

  if (nodes.size() == 0) {
    // if nothing to select, exit edit mode.
    if (![self hasBookmarksOrFolders]) {
      [self setTableViewEditing:NO];
      return;
    }
    [self setContextBarState:BookmarksContextBarBeginSelection];
    return;
  }
  if (nodes.size() == 1) {
    const bookmarks::BookmarkNode* node = *nodes.begin();
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

  NOTREACHED();
}

- (void)handleMoveNode:(const bookmarks::BookmarkNode*)node
            toPosition:(size_t)position {
  [self.snackbarCommandsHandler
      showSnackbarMessage:
          bookmark_utils_ios::UpdateBookmarkPositionWithUndoToast(
              node, self.displayedFolderNode, position, self.profileBookmarks,
              self.browserState)];
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
  DCHECK(self.sharedState.editingFolderNode);
  self.sharedState.addingNewFolder = NO;
  if (newName.length > 0) {
    self.sharedState.profileBookmarkModel->SetTitle(
        self.sharedState.editingFolderNode, base::SysNSStringToUTF16(newName),
        bookmarks::metrics::BookmarkEditSource::kUser);
  }
  self.sharedState.editingFolderNode = nullptr;
  self.sharedState.editingFolderCell = nil;
  [self refreshContents];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);

  // Copy the list of edited nodes from BookmarksFolderChooserCoordinator
  // as the reference may become invalid when `_folderChooserCoordinator`
  // is set to nil (if `self` holds the last reference to the object).
  std::set<const bookmarks::BookmarkNode*> editedNodes =
      _folderChooserCoordinator.editedNodes;

  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;

  DCHECK(!folder->is_url());
  DCHECK_GE(editedNodes.size(), 1u);

  [self setTableViewEditing:NO];
  [self.snackbarCommandsHandler
      showSnackbarMessage:bookmark_utils_ios::MoveBookmarksWithUndoToast(
                              std::move(editedNodes), self.profileBookmarks,
                              folder, self.browserState)];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
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
  self.displayedFolderNode = self.profileBookmarks->root_node();

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
          getBookmarkTopMostRowCacheWithPrefService:self.browserState
                                                        ->GetPrefs()
                                              model:self.profileBookmarks
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
          if (innerStrongSelf.sharedState.tableView.backgroundView ==
              innerStrongSelf.spinnerView) {
            innerStrongSelf.sharedState.tableView.backgroundView = nil;
          }
          innerStrongSelf.spinnerView = nil;
        }];
    [strongSelf loadBookmarkViews];
    [strongSelf.sharedState.tableView reloadData];
  }];
}

- (void)bookmarkNodeChanged:(const BookmarkNode*)node {
  // No-op here.  Bookmarks might be refreshed in BookmarksHomeMediator.
}

- (void)bookmarkNodeChildrenChanged:(const BookmarkNode*)bookmarkNode {
  // No-op here.  Bookmarks might be refreshed in BookmarksHomeMediator.
}

- (void)bookmarkNode:(const BookmarkNode*)bookmarkNode
     movedFromParent:(const BookmarkNode*)oldParent
            toParent:(const BookmarkNode*)newParent {
  // No-op here.  Bookmarks might be refreshed in BookmarksHomeMediator.
}

- (void)bookmarkNodeDeleted:(const BookmarkNode*)node
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
  [self dismissWithURL:GURL()];
  return YES;
}

#pragma mark - private

- (BOOL)isDisplayingBookmarkRoot {
  return self.displayedFolderNode == self.profileBookmarks->root_node();
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
  if (![self isDisplayingBookmarkRoot] ||
      self.sharedState.currentlyShowingSearchResults) {
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
                                        (const bookmarks::BookmarkNode*)node {
  viewController.navigationItem.leftBarButtonItem.action = @selector(back);
  // Disable large titles on every VC but the root controller.
  if (node != self.profileBookmarks->root_node()) {
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

  new_tab_page_uma::RecordAction(self.browserState->IsOffTheRecord(),
                                 self.webStateList->GetActiveWebState(),
                                 new_tab_page_uma::ACTION_OPENED_BOOKMARK);
  base::RecordAction(
      base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)addNewFolder {
  [self.sharedState.editingFolderCell stopEdit];
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return;
  }
  self.sharedState.addingNewFolder = YES;
  std::u16string folderTitle =
      l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_NEW_GROUP_DEFAULT_NAME);
  self.sharedState.editingFolderNode =
      self.sharedState.profileBookmarkModel->AddFolder(
          self.sharedState.tableViewDisplayedRootNode,
          self.sharedState.tableViewDisplayedRootNode->children().size(),
          folderTitle);

  BookmarksHomeNodeItem* nodeItem = [[BookmarksHomeNodeItem alloc]
      initWithType:BookmarksHomeItemTypeBookmark
      bookmarkNode:self.sharedState.editingFolderNode];
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  nodeItem.shouldDisplayCloudSlashIcon =
      bookmark_utils_ios::ShouldDisplayCloudSlashIcon(syncSetupService);
  [self.sharedState.tableViewModel
                      addItem:nodeItem
      toSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];

  // Insert the new folder cell at the end of the table.
  NSIndexPath* newRowIndexPath =
      [self.sharedState.tableViewModel indexPathForItem:nodeItem];
  NSMutableArray* newRowIndexPaths =
      [[NSMutableArray alloc] initWithObjects:newRowIndexPath, nil];
  [self.sharedState.tableView beginUpdates];
  [self.sharedState.tableView
      insertRowsAtIndexPaths:newRowIndexPaths
            withRowAnimation:UITableViewRowAnimationNone];
  [self.sharedState.tableView endUpdates];

  // Scroll to the end of the table
  [self.sharedState.tableView
      scrollToRowAtIndexPath:newRowIndexPath
            atScrollPosition:UITableViewScrollPositionBottom
                    animated:YES];
}

- (BookmarksHomeViewController*)createControllerWithDisplayedFolderNode:
    (const bookmarks::BookmarkNode*)displayedFolderNode {
  BookmarksHomeViewController* controller =
      [[BookmarksHomeViewController alloc] initWithBrowser:self.browser];
  controller.displayedFolderNode = displayedFolderNode;
  controller.homeDelegate = self.homeDelegate;
  controller.applicationCommandsHandler = self.applicationCommandsHandler;
  controller.snackbarCommandsHandler = self.snackbarCommandsHandler;

  return controller;
}

// Row selection of the tableView will be cleared after reloadData.  This
// function is used to restore the row selection.  It also updates editNodes in
// case some selected nodes are removed.
- (void)restoreRowSelection {
  // Create a new editNodes set to check if some selected nodes are removed.
  std::set<const bookmarks::BookmarkNode*> newEditNodes;

  // Add selected nodes to editNodes only if they are not removed (still exist
  // in the table).
  NSArray<TableViewItem*>* items = [self.sharedState.tableViewModel
      itemsInSectionWithIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  for (TableViewItem* item in items) {
    BookmarksHomeNodeItem* nodeItem =
        base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    const BookmarkNode* node = nodeItem.bookmarkNode;
    if (self.sharedState.editNodes.find(node) !=
        self.sharedState.editNodes.end()) {
      newEditNodes.insert(node);
      // Reselect the row of this node.
      NSIndexPath* itemPath =
          [self.sharedState.tableViewModel indexPathForItem:nodeItem];
      [self.sharedState.tableView
          selectRowAtIndexPath:itemPath
                      animated:NO
                scrollPosition:UITableViewScrollPositionNone];
    }
  }

  // if editNodes is changed, update it.
  if (self.sharedState.editNodes.size() != newEditNodes.size()) {
    self.sharedState.editNodes = newEditNodes;
    [self handleSelectEditNodes:self.sharedState.editNodes];
  }
}

- (BOOL)allowsNewFolder {
  // When the current root node has been removed remotely (becomes NULL),
  // or when displaying search results, creating new folder is forbidden.
  // The root folder displayed by the table view must also be editable to allow
  // creation of new folders. Note that Bookmarks Bar, Mobile Bookmarks, and
  // Other Bookmarks return as "editable" since the user can edit the contents
  // of those folders. Editing bookmarks must also be allowed.
  return self.sharedState.tableViewDisplayedRootNode != NULL &&
         !self.sharedState.currentlyShowingSearchResults &&
         [self isEditBookmarksEnabled] &&
         [self
             isNodeEditableByUser:self.sharedState.tableViewDisplayedRootNode];
}

- (int)topMostVisibleIndexPathRow {
  // If on root node screen, return 0.
  if (self.sharedState.profileBookmarkModel &&
      [self isDisplayingBookmarkRoot]) {
    return 0;
  }

  // If no rows in table, return 0.
  NSArray* visibleIndexPaths = [self.tableView indexPathsForVisibleRows];
  if (!visibleIndexPaths.count) {
    return 0;
  }

  // If the first row is still visible, return 0.
  NSIndexPath* topMostIndexPath = [visibleIndexPaths objectAtIndex:0];
  if (topMostIndexPath.row == 0) {
    return 0;
  }

  // Return the first visible row.
  topMostIndexPath = [visibleIndexPaths objectAtIndex:0];
  return topMostIndexPath.row;
}

- (void)navigateAway {
  [self.sharedState.editingFolderCell stopEdit];
}

// Returns YES if the given node is a url or folder node.
- (BOOL)isUrlOrFolder:(const BookmarkNode*)node {
  return node->type() == BookmarkNode::URL ||
         node->type() == BookmarkNode::FOLDER;
}

// Returns YES if the given node can be edited by user.
- (BOOL)isNodeEditableByUser:(const BookmarkNode*)node {
  // Note that CanBeEditedByUser() below returns true for Bookmarks Bar, Mobile
  // Bookmarks, and Other Bookmarks since the user can add, delete, and edit
  // items within those folders. CanBeEditedByUser() returns false for the
  // managed_node and all nodes that are descendants of managed_node.
  bookmarks::ManagedBookmarkService* managedBookmarkService =
      ManagedBookmarkServiceFactory::GetForBrowserState(self.browserState);
  return managedBookmarkService
             ? managedBookmarkService->CanBeEditedByUser(node)
             : YES;
}

// Returns YES if user is allowed to edit any bookmarks.
- (BOOL)isEditBookmarksEnabled {
  return self.browserState->GetPrefs()->GetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled);
}

// Returns the bookmark node associated with `indexPath`.
- (const BookmarkNode*)nodeAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];

  if (item.type == BookmarksHomeItemTypeBookmark) {
    BookmarksHomeNodeItem* nodeItem =
        base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    return nodeItem.bookmarkNode;
  }

  NOTREACHED() << "Unexpected item type " << item.type;
  return nullptr;
}

- (BOOL)hasItemAtIndexPath:(NSIndexPath*)indexPath {
  return [self.sharedState.tableViewModel hasItemAtIndexPath:indexPath];
}

- (BOOL)hasBookmarksOrFolders {
  if (!self.sharedState.tableViewDisplayedRootNode) {
    return NO;
  }
  if (self.sharedState.currentlyShowingSearchResults) {
    return [self
        hasItemsInSectionIdentifier:BookmarksHomeSectionIdentifierBookmarks];
  } else {
    return !self.sharedState.tableViewDisplayedRootNode->children().empty();
  }
}

- (BOOL)hasItemsInSectionIdentifier:(NSInteger)sectionIdentifier {
  BOOL hasSection = [self.sharedState.tableViewModel
      hasSectionForSectionIdentifier:sectionIdentifier];
  if (!hasSection) {
    return NO;
  }
  NSInteger section = [self.sharedState.tableViewModel
      sectionForSectionIdentifier:sectionIdentifier];
  return [self.sharedState.tableViewModel numberOfItemsInSection:section] > 0;
}

- (std::vector<const bookmarks::BookmarkNode*>)editNodes {
  std::vector<const bookmarks::BookmarkNode*> nodes;
  if (self.sharedState.currentlyShowingSearchResults) {
    // Create a vector of edit nodes in the same order as the selected nodes.
    base::ranges::copy(self.sharedState.editNodes, std::back_inserter(nodes));
  } else {
    // Create a vector of edit nodes in the same order as the nodes in folder.
    for (const auto& child :
         self.sharedState.tableViewDisplayedRootNode->children()) {
      if (self.sharedState.editNodes.find(child.get()) !=
          self.sharedState.editNodes.end()) {
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

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// `indexPath` for the cell representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
       indexPath:(NSIndexPath*)indexPath {
  UIView* cellView = [self.tableView cellForRowAtIndexPath:indexPath];
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::BookmarkEntry];
  self.sharingCoordinator =
      [[SharingCoordinator alloc] initWithBaseViewController:self
                                                     browser:self.browser
                                                      params:params
                                                  originView:cellView];
  [self.sharingCoordinator start];
}

// Returns whether the incognito mode is forced.
- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(self.browser->GetBrowserState()->GetPrefs());
}

// Returns whether the incognito mode is available.
- (BOOL)isIncognitoAvailable {
  return !IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs());
}

#pragma mark - Loading and Empty States

// Shows loading spinner background view.
- (void)showLoadingSpinnerBackground {
  if (!self.spinnerView) {
    self.spinnerView =
        [[HomeWaitingView alloc] initWithFrame:self.sharedState.tableView.bounds
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
            if (strongSelf.sharedState.tableView.backgroundView ==
                strongSelf.spinnerView) {
              strongSelf.sharedState.tableView.backgroundView = nil;
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
        initWithFrame:self.sharedState.tableView.bounds
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
    [self.sharedState.tableView reloadData];

    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    if (self.sharedState.promoVisible &&
        self.sharedState.tableView.visibleCells.count) {
      CGFloat signinPromoHeight = self.sharedState.tableView.visibleCells
                                      .firstObject.bounds.size.height;
      self.emptyViewBackground.scrollViewContentInsets =
          UIEdgeInsetsMake(signinPromoHeight, 0.0, 0.0, 0.0);
    } else {
      self.emptyViewBackground.scrollViewContentInsets =
          self.view.safeAreaInsets;
    }
  }

  self.sharedState.tableView.backgroundView = self.emptyViewBackground;
  self.navigationItem.searchController = nil;
}

- (void)hideEmptyBackground {
  if (self.sharedState.tableView.backgroundView == self.emptyViewBackground) {
    self.sharedState.tableView.backgroundView = nil;
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
  const std::set<const bookmarks::BookmarkNode*> nodes =
      self.sharedState.editNodes;
  switch (self.contextBarState) {
    case BookmarksContextBarDefault:
      // New Folder clicked.
      [self addNewFolder];
      break;
    case BookmarksContextBarBeginSelection:
      // This must never happen, as the leading button is disabled at this
      // point.
      NOTREACHED();
      break;
    case BookmarksContextBarSingleURLSelection:
    case BookmarksContextBarMultipleURLSelection:
    case BookmarksContextBarSingleFolderSelection:
    case BookmarksContextBarMultipleFolderSelection:
    case BookmarksContextBarMixedSelection:
      // Delete clicked.
      [self deleteNodes:nodes];
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerRemoveSelected"));
      break;
    case BookmarksContextBarNone:
    default:
      NOTREACHED();
  }
}

// Called when the center button is clicked.
- (void)centerButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  const std::set<const bookmarks::BookmarkNode*> nodes =
      self.sharedState.editNodes;
  // Center button is shown and is clickable only when at least
  // one node is selected.
  DCHECK(nodes.size() > 0);

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
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
      NOTREACHED();
      break;
  }

  [self.actionSheetCoordinator start];
}

// Called when the trailing button, "Select" or "Cancel" is clicked.
- (void)trailingButtonClicked {
  // Ignore the button tap if any of our controller is presenting.
  if ([self isAnyControllerPresenting]) {
    return;
  }
  // Toggle edit mode.
  [self setTableViewEditing:!self.sharedState.currentlyInEditMode];
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
        base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(
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
  editButton.enabled =
      [self isEditBookmarksEnabled] && [self hasBookmarksOrFolders] &&
      [self isNodeEditableByUser:self.sharedState.tableViewDisplayedRootNode];

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
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  if ([strongSelf isIncognitoForced]) {
                    return;
                  }
                  std::vector<const bookmarks::BookmarkNode*> editNodes =
                      [strongSelf editNodes];
                  [strongSelf openAllURLs:GetUrlsToOpen(editNodes)
                              inIncognito:NO
                                   newTab:NO];
                }
                 style:UIAlertActionStyleDefault
               enabled:![self isIncognitoForced]];

  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  if (![strongSelf isIncognitoAvailable]) {
                    return;
                  }
                  std::vector<const bookmarks::BookmarkNode*> editNodes =
                      [strongSelf editNodes];
                  [strongSelf openAllURLs:GetUrlsToOpen(editNodes)
                              inIncognito:YES
                                   newTab:NO];
                }
                 style:UIAlertActionStyleDefault
               enabled:[self isIncognitoAvailable]];

  std::set<int64_t> nodeIds;
  for (const BookmarkNode* node : nodes) {
    nodeIds.insert(node->id());
  }

  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }

                  absl::optional<std::set<const BookmarkNode*>> nodesFromIds =
                      bookmark_utils_ios::FindNodesByIds(
                          strongSelf.profileBookmarks, nodeIds);
                  if (nodesFromIds) {
                    base::RecordAction(base::UserMetricsAction(
                        "MobileBookmarkManagerMoveToFolderBulk"));
                    [strongSelf moveNodes:*nodesFromIds];
                  }
                }
                 style:UIAlertActionStyleDefault];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
        forSingleBookmarkURL:(const BookmarkNode*)node {
  __weak BookmarksHomeViewController* weakSelf = self;
  std::string urlString = node->url().possibly_invalid_spec();
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;

  int64_t nodeId = node->id();
  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT);
  // Disable the edit menu option if the node is not editable by user, or if
  // editing bookmarks is not allowed.
  BOOL editEnabled =
      [self isEditBookmarksEnabled] && [self isNodeEditableByUser:node];

  [coordinator addItemWithTitle:titleString
                         action:^{
                           BookmarksHomeViewController* strongSelf = weakSelf;
                           if (!strongSelf) {
                             return;
                           }
                           const bookmarks::BookmarkNode* nodeFromId =
                               bookmark_utils_ios::FindNodeById(
                                   strongSelf.profileBookmarks, nodeId);
                           if (nodeFromId) {
                             [strongSelf editNodeURL:nodeFromId];
                           }
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];

  GURL nodeURL = node->url();
  titleString = GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [coordinator addItemWithTitle:titleString
                         action:^{
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
                  // Use strongSelf even though the object is only used once
                  // because we do not want to change the global pasteboard
                  // if the view has been deallocated.
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
                  pasteboard.string = base::SysUTF8ToNSString(urlString);
                  [strongSelf setTableViewEditing:NO];
                }
                 style:UIAlertActionStyleDefault];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
     forSingleBookmarkFolder:(const BookmarkNode*)node {
  __weak BookmarksHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;

  int64_t nodeId = node->id();
  NSString* titleString =
      GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER);
  // Disable the edit and move menu options if the folder is not editable by
  // user, or if editing bookmarks is not allowed.
  BOOL editEnabled =
      [self isEditBookmarksEnabled] && [self isNodeEditableByUser:node];

  [coordinator addItemWithTitle:titleString
                         action:^{
                           BookmarksHomeViewController* strongSelf = weakSelf;
                           if (!strongSelf) {
                             return;
                           }
                           const bookmarks::BookmarkNode* nodeFromId =
                               bookmark_utils_ios::FindNodeById(
                                   strongSelf.profileBookmarks, nodeId);
                           if (nodeFromId) {
                             [strongSelf editNodeFolder:nodeFromId];
                           }
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];

  titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);
  [coordinator addItemWithTitle:titleString
                         action:^{
                           BookmarksHomeViewController* strongSelf = weakSelf;
                           if (!strongSelf) {
                             return;
                           }
                           const bookmarks::BookmarkNode* nodeFromId =
                               bookmark_utils_ios::FindNodeById(
                                   strongSelf.profileBookmarks, nodeId);
                           if (nodeFromId) {
                             base::RecordAction(base::UserMetricsAction(
                                 "MobileBookmarkManagerMoveToFolder"));
                             std::set<const BookmarkNode*> nodes{nodeFromId};
                             [strongSelf moveNodes:nodes];
                           }
                         }
                          style:UIAlertActionStyleDefault
                        enabled:editEnabled];
}

- (void)configureCoordinator:(AlertCoordinator*)coordinator
    forMixedAndMultiFolderSelection:
        (const std::set<const bookmarks::BookmarkNode*>)nodes {
  __weak BookmarksHomeViewController* weakSelf = self;
  coordinator.alertController.view.accessibilityIdentifier =
      kBookmarksHomeContextMenuIdentifier;

  std::set<int64_t> nodeIds;
  for (const bookmarks::BookmarkNode* node : nodes) {
    nodeIds.insert(node->id());
  }

  NSString* titleString = GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE);
  [coordinator
      addItemWithTitle:titleString
                action:^{
                  BookmarksHomeViewController* strongSelf = weakSelf;
                  if (!strongSelf) {
                    return;
                  }
                  absl::optional<std::set<const bookmarks::BookmarkNode*>>
                      nodesFromIds = bookmark_utils_ios::FindNodesByIds(
                          strongSelf.profileBookmarks, nodeIds);
                  if (nodesFromIds) {
                    base::RecordAction(base::UserMetricsAction(
                        "MobileBookmarkManagerMoveToFolderBulk"));
                    [strongSelf moveNodes:*nodesFromIds];
                  }
                }
                 style:UIAlertActionStyleDefault];
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
  if (self.sharedState.currentlyInEditMode || [self scrimIsVisible]) {
    return NO;
  }
  return YES;
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gestureRecognizer {
  if (self.sharedState.currentlyInEditMode ||
      gestureRecognizer.state != UIGestureRecognizerStateBegan) {
    return;
  }
  CGPoint touchPoint =
      [gestureRecognizer locationInView:self.sharedState.tableView];
  NSIndexPath* indexPath =
      [self.sharedState.tableView indexPathForRowAtPoint:touchPoint];

  if (![self canShowContextMenuFor:indexPath]) {
    return;
  }

  const BookmarkNode* node = [self nodeAtIndexPath:indexPath];

  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
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
    NOTREACHED();
    return;
  }

  [self.actionSheetCoordinator start];
}

- (BOOL)canShowContextMenuFor:(NSIndexPath*)indexPath {
  if (indexPath == nil ||
      [self.sharedState.tableViewModel
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
    if (self.sharedState.currentlyShowingSearchResults) {
      self.sharedState.currentlyShowingSearchResults = NO;
      // Restore current list.
      [self.mediator computeBookmarkTableViewData];
      [self.mediator computePromoTableViewData];
      [self.sharedState.tableView reloadData];
      [self showScrim];
    }
  } else {
    if (!self.sharedState.currentlyShowingSearchResults) {
      self.sharedState.currentlyShowingSearchResults = YES;
      [self.mediator computePromoTableViewData];
      [self hideScrim];
    }
    // Replace current list with search result, but doesn't change
    // the 'regular' model for this page, which we can restore when search
    // is terminated.
    NSString* noResults = GetNSString(IDS_HISTORY_NO_SEARCH_RESULTS);
    [self.mediator computeBookmarkTableViewDataMatching:text
                             orShowMessageWhenNoResults:noResults];
    [self.sharedState.tableView reloadData];
    [self setupContextBar];
  }
}

#pragma mark UISearchControllerDelegate

- (void)willPresentSearchController:(UISearchController*)searchController {
  [self showScrim];
}

- (void)willDismissSearchController:(UISearchController*)searchController {
  // Avoid scrim being put back on in updateSearchResultsForSearchController.
  self.sharedState.currentlyShowingSearchResults = NO;
  // Restore current list.
  [self.mediator computeBookmarkTableViewData];
  [self.sharedState.tableView reloadData];
}

- (void)didDismissSearchController:(UISearchController*)searchController {
  [self hideScrim];
}

#pragma mark - BookmarksHomeSharedStateObserver

- (void)sharedStateDidClearEditNodes:(BookmarksHomeSharedState*)sharedState {
  [self handleSelectEditNodes:sharedState.editNodes];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];

  cell.userInteractionEnabled = (item.type != BookmarksHomeItemTypeMessage);

  if (item.type == BookmarksHomeItemTypeBookmark) {
    BookmarksHomeNodeItem* nodeItem =
        base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    if (nodeItem.bookmarkNode->is_folder() &&
        nodeItem.bookmarkNode == self.sharedState.editingFolderNode) {
      TableViewBookmarksFolderCell* tableCell =
          base::mac::ObjCCastStrict<TableViewBookmarksFolderCell>(cell);
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
        strongSelf.sharedState.editingFolderCell = tableCell;
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
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != BookmarksHomeItemTypeBookmark) {
    // Can only edit bookmarks.
    return NO;
  }

  // If the cell at `indexPath` is being edited (which happens when creating a
  // new Folder) return NO.
  if ([tableView indexPathForCell:self.sharedState.editingFolderCell] ==
      indexPath) {
    return NO;
  }

  // Enable the swipe-to-delete gesture and reordering control for editable
  // nodes of type URL or Folder, but not the permanent ones. Only enable
  // swipe-to-delete if editing bookmarks is allowed.
  BookmarksHomeNodeItem* nodeItem =
      base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(item);
  const BookmarkNode* node = nodeItem.bookmarkNode;
  return [self isEditBookmarksEnabled] && [self isUrlOrFolder:node] &&
         [self isNodeEditableByUser:node];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];
  if (item.type != BookmarksHomeItemTypeBookmark) {
    // Can only commit edits for bookmarks.
    return;
  }

  if (editingStyle == UITableViewCellEditingStyleDelete) {
    BookmarksHomeNodeItem* nodeItem =
        base::mac::ObjCCastStrict<BookmarksHomeNodeItem>(item);
    const BookmarkNode* node = nodeItem.bookmarkNode;
    std::set<const BookmarkNode*> nodes;
    nodes.insert(node);
    [self handleSelectNodesForDeletion:nodes];
    base::RecordAction(
        base::UserMetricsAction("MobileBookmarkManagerEntryDeleted"));
  }
}

- (BOOL)tableView:(UITableView*)tableView
    canMoveRowAtIndexPath:(NSIndexPath*)indexPath {
  // No reorering with filtered results or when displaying the top-most
  // Bookmarks node.
  if (self.sharedState.currentlyShowingSearchResults ||
      [self isDisplayingBookmarkRoot] || !self.tableView.editing) {
    return NO;
  }
  TableViewItem* item =
      [self.sharedState.tableViewModel itemAtIndexPath:indexPath];
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
      self.sharedState.currentlyShowingSearchResults) {
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
    heightForHeaderInSection:(NSInteger)section {
  return ChromeTableViewHeightForHeaderInSection(section);
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  return UITableViewAutomaticDimension;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier = [self.sharedState.tableViewModel
      sectionIdentifierForSectionIndex:indexPath.section];
  if (sectionIdentifier == BookmarksHomeSectionIdentifierBookmarks) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    // If table is in edit mode, record all the nodes added to edit set.
    if (self.sharedState.currentlyInEditMode) {
      if ([self isNodeEditableByUser:node]) {
        // Only add nodes that are editable to the edit set.
        self.sharedState.editNodes.insert(node);
        [self handleSelectEditNodes:self.sharedState.editNodes];
        return;
      }
      // If the selected row is not editable, do not add it to the edit set.
      // Simply deselect the row.
      [tableView deselectRowAtIndexPath:indexPath animated:YES];
      return;
    }
    [self.sharedState.editingFolderCell stopEdit];
    if (node->is_folder()) {
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerOpenFolder"));
      [self handleSelectFolderForNavigation:node];
    } else {
      if (self.sharedState.currentlyShowingSearchResults) {
        // Set the searchController active property to NO or the SearchBar will
        // cause the navigation controller to linger for a second  when
        // dismissing.
        self.searchController.active = NO;
      }
      // Open URL. Pass this to the delegate.
      [self handleSelectUrlForNavigation:node->url()];
    }
  }
  // Deselect row.
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier = [self.sharedState.tableViewModel
      sectionIdentifierForSectionIndex:indexPath.section];
  if (sectionIdentifier == BookmarksHomeSectionIdentifierBookmarks &&
      self.sharedState.currentlyInEditMode) {
    const BookmarkNode* node = [self nodeAtIndexPath:indexPath];
    DCHECK(node);
    self.sharedState.editNodes.erase(node);
    [self handleSelectEditNodes:self.sharedState.editNodes];
  }
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  if (self.sharedState.currentlyInEditMode) {
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

  int64_t nodeId = node->id();
  __weak BookmarksHomeViewController* weakSelf = self;
  if (node->is_url()) {
    GURL nodeURL = node->url();
    actionProvider = ^(NSArray<UIMenuElement*>* suggestedActions) {
      BookmarksHomeViewController* strongSelf = weakSelf;
      if (!strongSelf) {
        return [UIMenu menuWithTitle:@"" children:@[]];
      }

      // Record that this context menu was shown to the user.
      RecordMenuShown(MenuScenarioHistogram::kBookmarkEntry);

      BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
          initWithBrowser:strongSelf.browser
                 scenario:MenuScenarioHistogram::kBookmarkEntry];

      NSMutableArray<UIMenuElement*>* menuElements =
          [[NSMutableArray alloc] init];

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

      if (base::ios::IsMultipleScenesSupported()) {
        [menuElements
            addObject:[actionFactory
                          actionToOpenInNewWindowWithURL:nodeURL
                                          activityOrigin:
                                              WindowActivityBookmarksOrigin]];
      }

      [menuElements addObject:[actionFactory actionToCopyURL:nodeURL]];

      UIAction* editAction = [actionFactory actionToEditWithBlock:^{
        BookmarksHomeViewController* innerStrongSelf = weakSelf;
        if (!innerStrongSelf) {
          return;
        }
        const bookmarks::BookmarkNode* nodeFromId =
            bookmark_utils_ios::FindNodeById(innerStrongSelf.profileBookmarks,
                                             nodeId);
        if (nodeFromId) {
          [innerStrongSelf editNodeURL:nodeFromId];
        }
      }];
      [menuElements addObject:editAction];

      [menuElements
          addObject:[actionFactory actionToShareWithBlock:^{
            BookmarksHomeViewController* innerStrongSelf = weakSelf;
            if (!innerStrongSelf) {
              return;
            }
            const bookmarks::BookmarkNode* nodeFromId =
                bookmark_utils_ios::FindNodeById(
                    innerStrongSelf.profileBookmarks, nodeId);
            if (nodeFromId) {
              [weakSelf
                   shareURL:nodeURL
                      title:bookmark_utils_ios::TitleForBookmarkNode(nodeFromId)
                  indexPath:indexPath];
            }
          }]];

      UIAction* deleteAction = [actionFactory actionToDeleteWithBlock:^{
        BookmarksHomeViewController* innerStrongSelf = weakSelf;
        if (!innerStrongSelf) {
          return;
        }
        const bookmarks::BookmarkNode* nodeFromId =
            bookmark_utils_ios::FindNodeById(innerStrongSelf.profileBookmarks,
                                             nodeId);
        if (nodeFromId) {
          std::set<const BookmarkNode*> nodes{nodeFromId};
          [innerStrongSelf handleSelectNodesForDeletion:nodes];
          base::RecordAction(
              base::UserMetricsAction("MobileBookmarkManagerEntryDeleted"));
        }
      }];
      [menuElements addObject:deleteAction];

      // Disable Edit and Delete if the node cannot be edited.
      if (!canEditNode) {
        editAction.attributes = UIMenuElementAttributesDisabled;
        deleteAction.attributes = UIMenuElementAttributesDisabled;
      }

      return [UIMenu menuWithTitle:@"" children:menuElements];
    };
  } else if (node->is_folder()) {
    actionProvider = ^(NSArray<UIMenuElement*>* suggestedActions) {
      BookmarksHomeViewController* strongSelf = weakSelf;
      if (!strongSelf) {
        return [UIMenu menuWithTitle:@"" children:@[]];
      }

      // Record that this context menu was shown to the user.
      RecordMenuShown(MenuScenarioHistogram::kBookmarkFolder);

      ActionFactory* actionFactory = [[ActionFactory alloc]
          initWithScenario:MenuScenarioHistogram::kBookmarkFolder];

      NSMutableArray<UIMenuElement*>* menuElements =
          [[NSMutableArray alloc] init];

      UIAction* editAction = [actionFactory actionToEditWithBlock:^{
        BookmarksHomeViewController* innerStrongSelf = weakSelf;
        if (!innerStrongSelf) {
          return;
        }
        const bookmarks::BookmarkNode* nodeFromId =
            bookmark_utils_ios::FindNodeById(innerStrongSelf.profileBookmarks,
                                             nodeId);
        if (nodeFromId) {
          [innerStrongSelf editNodeFolder:nodeFromId];
        }
      }];
      UIAction* moveAction = [actionFactory actionToMoveFolderWithBlock:^{
        BookmarksHomeViewController* innerStrongSelf = weakSelf;
        if (!innerStrongSelf) {
          return;
        }
        const bookmarks::BookmarkNode* nodeFromId =
            bookmark_utils_ios::FindNodeById(innerStrongSelf.profileBookmarks,
                                             nodeId);
        if (nodeFromId) {
          base::RecordAction(
              base::UserMetricsAction("MobileBookmarkManagerMoveToFolder"));
          std::set<const BookmarkNode*> nodes{nodeFromId};
          [innerStrongSelf moveNodes:nodes];
        }
      }];

      if (!canEditNode) {
        editAction.attributes = UIMenuElementAttributesDisabled;
        moveAction.attributes = UIMenuElementAttributesDisabled;
      }

      [menuElements addObject:editAction];
      [menuElements addObject:moveAction];

      return [UIMenu menuWithTitle:@"" children:menuElements];
    };
  }

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - TableViewURLDragDataSource

- (URLInfo*)tableView:(UITableView*)tableView
    URLInfoAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section !=
      [self.tableViewModel sectionForSectionIdentifier:
                               BookmarksHomeSectionIdentifierBookmarks]) {
    return nil;
  }

  const bookmarks::BookmarkNode* node = [self nodeAtIndexPath:indexPath];
  if (!node || node->is_folder()) {
    return nil;
  }
  return [[URLInfo alloc]
      initWithURL:node->url()
            title:bookmark_utils_ios::TitleForBookmarkNode(node)];
}

#pragma mark - TableViewURLDropDelegate

- (BOOL)canHandleURLDropInTableView:(UITableView*)tableView {
  return !self.sharedState.currentlyShowingSearchResults &&
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
              self.displayedFolderNode, index, self.profileBookmarks,
              self.browserState)];
}

@end

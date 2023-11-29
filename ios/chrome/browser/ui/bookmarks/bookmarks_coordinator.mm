// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"

#import <stdint.h>

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Tracks the type of UI that is currently being presented.
enum class PresentedState {
  NONE,
  BOOKMARK_BROWSER,
  BOOKMARK_EDITOR,
  FOLDER_EDITOR,
  FOLDER_SELECTION,
};

}  // namespace

@interface BookmarksCoordinator () <BookmarksEditorCoordinatorDelegate,
                                    BookmarksFolderEditorCoordinatorDelegate,
                                    BookmarksFolderChooserCoordinatorDelegate,
                                    BookmarksHomeViewControllerDelegate,
                                    ManageSyncSettingsCoordinatorDelegate,
                                    UIAdaptivePresentationControllerDelegate,
                                    UINavigationControllerDelegate>

// The type of view controller that is being presented.
@property(nonatomic, assign) PresentedState currentPresentedState;

// A reference to the potentially presented single bookmark editor. This will
// be non-nil when `currentPresentedState` is BOOKMARK_EDITOR.
@property(nonatomic, strong)
    BookmarksEditorCoordinator* bookmarkEditorCoordinator;

// The navigation controller that is being presented, if any.
// `self.bookmarkBrowser` is a child of this navigation controller.
@property(nonatomic, strong)
    UINavigationController* bookmarkNavigationController;

// A reference to the potentially presented bookmark browser. This will be
// non-nil when `currentPresentedState` is BOOKMARK_BROWSER.
@property(nonatomic, strong) BookmarksHomeViewController* bookmarkBrowser;

// A reference to the potentially presented folder editor. This will be non-nil
// when `currentPresentedState` is FOLDER_EDITOR.
@property(nonatomic, strong)
    BookmarksFolderEditorCoordinator* folderEditorCoordinator;

// A reference to the potentially presented folder chooser. This will be
// non-nil when `currentPresentedState` is FOLDER_SELECTION.
@property(nonatomic, strong)
    BookmarksFolderChooserCoordinator* folderChooserCoordinator;

// URLs to bookmark when handling BookmarksCommands.
@property(nonatomic, strong) NSArray<URLWithTitle*>* URLs;

@property(nonatomic, strong) BookmarkMediator* mediator;

// Handler for Application Commands.
@property(nonatomic, readonly, weak) id<ApplicationCommands>
    applicationCommandsHandler;

// Handler for Snackbar Commands.
@property(nonatomic, readonly, weak) id<SnackbarCommands>
    snackbarCommandsHandler;

@end

@implementation BookmarksCoordinator {
  // The browser state of the current user.
  base::WeakPtr<ChromeBrowserState> _currentBrowserState;
  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  base::WeakPtr<ChromeBrowserState> _browserState;

  // Profile bookmark model.
  base::WeakPtr<bookmarks::BookmarkModel> _localOrSyncableBookmarkModel;
  // Account bookmark model.
  base::WeakPtr<bookmarks::BookmarkModel> _accountBookmarkModel;
  // Coordinator of manage sync settings.
  ManageSyncSettingsCoordinator* _manageSyncSettingsCoordinator;
}

@synthesize applicationCommandsHandler = _applicationCommandsHandler;
@synthesize baseViewController = _baseViewController;
@synthesize snackbarCommandsHandler = _snackbarCommandsHandler;

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    // Bookmarks are always opened with the main browser state, even in
    // incognito mode.
    _currentBrowserState = browser->GetBrowserState()->AsWeakPtr();
    _browserState =
        _currentBrowserState->GetOriginalChromeBrowserState()->AsWeakPtr();
    _localOrSyncableBookmarkModel =
        ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
            _browserState.get())
            ->AsWeakPtr();
    BookmarkModel* accountBookmarkModel =
        ios::AccountBookmarkModelFactory::GetForBrowserState(
            _browserState.get());
    if (accountBookmarkModel) {
      _accountBookmarkModel = accountBookmarkModel->AsWeakPtr();
    }
    _mediator = [[BookmarkMediator alloc]
        initWithWithLocalOrSyncableBookmarkModel:_localOrSyncableBookmarkModel
                                                     .get()
                            accountBookmarkModel:_accountBookmarkModel.get()
                                           prefs:_browserState->GetPrefs()
                           authenticationService:AuthenticationServiceFactory::
                                                     GetForBrowserState(
                                                         _browserState.get())
                                     syncService:SyncServiceFactory::
                                                     GetForBrowserState(
                                                         _browserState.get())];
    _currentPresentedState = PresentedState::NONE;
    DCHECK(_localOrSyncableBookmarkModel) << [self description];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_browserState);
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  switch (self.currentPresentedState) {
    case PresentedState::BOOKMARK_BROWSER:
      [self bookmarkBrowserDismissed];
      break;
    case PresentedState::BOOKMARK_EDITOR:
      [self stopBookmarksEditorCoordinator];
      break;
    case PresentedState::FOLDER_EDITOR:
      [self stopBookmarksFolderEditorCoordinator];
      break;
    case PresentedState::FOLDER_SELECTION:
      [self stopBookmarksFolderChooserCoordinator];
      break;
    case PresentedState::NONE:
      break;
  }
  _browserState = nullptr;
  _currentBrowserState = nullptr;
  _localOrSyncableBookmarkModel = nullptr;
  _accountBookmarkModel = nullptr;
  _mediator = nil;
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.bookmarkEditorCoordinator) << [self description];
  DCHECK(!self.folderEditorCoordinator) << [self description];
  DCHECK(!self.folderChooserCoordinator) << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  [super stop];
}

- (id<ApplicationCommands>)applicationCommandsHandler {
  // Using lazy loading here to avoid potential crashes with ApplicationCommands
  // not being yet dispatched.
  if (!_applicationCommandsHandler) {
    _applicationCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
  }
  return _applicationCommandsHandler;
}

- (id<SnackbarCommands>)snackbarCommandsHandler {
  // Using lazy loading here to avoid potential crashes with SnackbarCommands
  // not being yet dispatched.
  if (!_snackbarCommandsHandler) {
    _snackbarCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SnackbarCommands);
  }
  return _snackbarCommandsHandler;
}

- (void)createBookmarkURL:(const GURL&)URL title:(NSString*)title {
  if (!AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    return;
  }

  __weak BookmarksCoordinator* weakSelf = self;
  // Copy of `URL` to be captured in block.
  GURL bookmarkedURL(URL);
  void (^editAction)() = ^{
    base::RecordAction(base::UserMetricsAction(
        "MobileBookmarkManagerOpenedBookmarkEditorFromSnackbar"));
    [weakSelf presentBookmarkEditorForURL:bookmarkedURL];
  };

  [self.snackbarCommandsHandler
      showSnackbarMessage:[self.mediator addBookmarkWithTitle:title
                                                          URL:bookmarkedURL
                                                   editAction:editAction]];
}

- (void)presentBookmarkEditorForURL:(const GURL&)URL {
  if (!AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    return;
  }

  const BookmarkNode* bookmark =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          URL, _localOrSyncableBookmarkModel.get(),
          _accountBookmarkModel.get());
  if (!bookmark) {
    return;
  }
  [self presentEditorForURLNode:bookmark];
}

- (void)presentBookmarks {
  [self presentBookmarksAtDisplayedFolderNode:_localOrSyncableBookmarkModel
                                                  ->root_node()
                            selectingBookmark:nil];
}

- (void)presentFolderChooser {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState)
      << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  [self dismissSnackbar];
  self.currentPresentedState = PresentedState::FOLDER_SELECTION;
  self.folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                     hiddenNodes:std::set<const bookmarks::BookmarkNode*>()];
  self.folderChooserCoordinator.delegate = self;
  [self.folderChooserCoordinator start];
}

- (void)presentEditorForURLNode:(const bookmarks::BookmarkNode*)node {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState)
      << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  DCHECK(node) << [self description];
  DCHECK_EQ(node->type(), BookmarkNode::URL);
  [self dismissSnackbar];
  self.currentPresentedState = PresentedState::BOOKMARK_EDITOR;
  self.bookmarkEditorCoordinator = [[BookmarksEditorCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                            node:node
         snackbarCommandsHandler:self.snackbarCommandsHandler];
  self.bookmarkEditorCoordinator.delegate = self;
  [self.bookmarkEditorCoordinator start];
}

- (void)presentEditorForFolderNode:(const bookmarks::BookmarkNode*)node {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState)
      << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  DCHECK(node) << [self description];
  DCHECK_EQ(node->type(), BookmarkNode::FOLDER) << [self description];
  [self dismissSnackbar];
  self.currentPresentedState = PresentedState::FOLDER_EDITOR;
  // `self.baseViewController` is part of a navigation view controller.
  // Therefore, the bookmark folder view needs to be presented by
  // `self.baseViewController.navigationController`.
  self.folderEditorCoordinator = [[BookmarksFolderEditorCoordinator alloc]
      initWithBaseViewController:self.baseViewController.navigationController
                         browser:self.browser
                      folderNode:node];
  self.folderEditorCoordinator.delegate = self;
  [self.folderEditorCoordinator start];
}

- (void)dismissBookmarkBrowserAnimated:(BOOL)animated
                            urlsToOpen:(const std::vector<GURL>&)urlsToOpen
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab {
  if (self.currentPresentedState != PresentedState::BOOKMARK_BROWSER) {
    return;
  }
  DCHECK(self.bookmarkNavigationController);

  // If trying to open urls with tab mode changed, we need to postpone openUrls
  // until the dismissal of Bookmarks is done.  This is to prevent the race
  // condition between the dismissal of bookmarks and switch of BVC.
  const BOOL openUrlsAfterDismissal =
      !urlsToOpen.empty() &&
      ((!!inIncognito) != _currentBrowserState->IsOffTheRecord());

  // A copy of the urls vector for the completion block.
  std::vector<GURL> urlsToOpenAfterDismissal;
  if (openUrlsAfterDismissal) {
    // open urls in the completion block after dismissal.
    urlsToOpenAfterDismissal = urlsToOpen;
  } else if (!urlsToOpen.empty()) {
    // open urls now.
    [self openUrls:urlsToOpen inIncognito:inIncognito newTab:newTab];
  }

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf bookmarkBrowserDismissed];
    if (!openUrlsAfterDismissal) {
      return;
    }
    [weakSelf openUrls:urlsToOpenAfterDismissal
           inIncognito:inIncognito
                newTab:newTab];
  };

  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else {
    completion();
  }
}

- (void)bookmarkBrowserDismissed {
  DCHECK_EQ(PresentedState::BOOKMARK_BROWSER, self.currentPresentedState)
      << [self description];
  DCHECK(self.bookmarkNavigationController) << [self description];
  for (UIViewController* controller in self.bookmarkNavigationController
           .viewControllers) {
    BookmarksHomeViewController* bookmarksHomeViewController =
        base::apple::ObjCCastStrict<BookmarksHomeViewController>(controller);
    [bookmarksHomeViewController shutdown];
  }
  // TODO(crbug.com/940856): Make sure navigaton
  // controller doesn't keep any controllers. Without
  // this there's a memory leak of (almost) every BHVC
  // the user visits.
  [self.bookmarkNavigationController setViewControllers:@[] animated:NO];
  self.bookmarkBrowser.homeDelegate = nil;
  self.bookmarkBrowser = nil;
  self.bookmarkNavigationController.presentationController.delegate = nil;
  self.bookmarkNavigationController.delegate = nil;
  self.bookmarkNavigationController = nil;
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissBookmarksEditorAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::BOOKMARK_EDITOR) {
    // TODO(crbug.com/1404250): This test should be turned into a DCHECK().
    return;
  }
  self.bookmarkEditorCoordinator.animatedDismissal = animated;
  [self stopBookmarksEditorCoordinator];
}

- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated {
  // No urls to open.  So it does not care about inIncognito and newTab.
  [self dismissBookmarkBrowserAnimated:animated
                            urlsToOpen:std::vector<GURL>()
                           inIncognito:NO
                                newTab:NO];
  [self dismissBookmarksEditorAnimated:animated];
}

- (void)dismissSnackbar {
  // Dismiss any bookmark related snackbar this controller could have presented.
  [MDCSnackbarManager.defaultManager
      dismissAndCallCompletionBlocksWithCategory:
          bookmark_utils_ios::kBookmarksSnackbarCategory];
}

- (BOOL)canDismiss {
  switch (self.currentPresentedState) {
    case PresentedState::NONE:
      return YES;
    case PresentedState::BOOKMARK_BROWSER:
      return [self.bookmarkBrowser canDismiss];
    case PresentedState::BOOKMARK_EDITOR:
      return [self.bookmarkEditorCoordinator canDismiss];
    case PresentedState::FOLDER_SELECTION:
      return [self.folderChooserCoordinator canDismiss];
    case PresentedState::FOLDER_EDITOR:
      return [self.folderEditorCoordinator canDismiss];
  }
}

- (void)showAccountSettings {
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(_browserState.get());
  CHECK(!syncService->GetAccountInfo().IsEmpty())
      << base::SysNSStringToUTF8([self description]);
  SyncSettingsAccountState accountState =
      syncService->HasSyncConsent() ? SyncSettingsAccountState::kSyncing
                                    : SyncSettingsAccountState::kSignedIn;
  _manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                    accountState:accountState];
  _manageSyncSettingsCoordinator.delegate = self;
  [_manageSyncSettingsCoordinator start];
}

#pragma mark - BookmarksEditorCoordinatorDelegate

- (void)bookmarksEditorCoordinatorShouldStop:
    (BookmarksEditorCoordinator*)coordinator {
  [self dismissBookmarksEditorAnimated:YES];
}

- (void)bookmarkEditorWillCommitTitleOrURLChange:
    (BookmarksEditorCoordinator*)coordinator {
  [self.delegate bookmarksCoordinatorWillCommitTitleOrURLChange:self];
}

#pragma mark - BookmarksFolderEditorCoordinatorDelegate

- (void)bookmarksFolderEditorCoordinator:
            (BookmarksFolderEditorCoordinator*)folderEditor
              didFinishEditingFolderNode:
                  (const bookmarks::BookmarkNode*)folder {
  DCHECK(folder) << [self description];
  [self stopBookmarksFolderEditorCoordinator];
}

- (void)bookmarksFolderEditorCoordinatorShouldStop:
    (BookmarksFolderEditorCoordinator*)coordinator {
  [self stopBookmarksFolderEditorCoordinator];
}

- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorCoordinator*)coordinator {
  [self.delegate bookmarksCoordinatorWillCommitTitleOrURLChange:self];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder {
  DCHECK(folder) << [self description];
  DCHECK(_URLs) << [self description];

  [self stopBookmarksFolderChooserCoordinator];

  bookmarks::StorageType type = bookmark_utils_ios::GetBookmarkModelType(
      folder, _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get());
  SetLastUsedBookmarkFolder(_browserState->GetPrefs(), folder, type);
  [self.snackbarCommandsHandler
      showSnackbarMessage:[self.mediator addBookmarks:_URLs toFolder:folder]];
  _URLs = nil;
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  [self stopBookmarksFolderChooserCoordinator];
}

#pragma mark - BookmarksHomeViewControllerDelegate

- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarksHomeViewController*)controller
                                navigationToUrls:
                                    (const std::vector<GURL>&)urls {
  [self bookmarkHomeViewControllerWantsDismissal:controller
                                navigationToUrls:urls
                                     inIncognito:_currentBrowserState
                                                     ->IsOffTheRecord()
                                          newTab:NO];
}

- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarksHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab {
  [self dismissBookmarkBrowserAnimated:YES
                            urlsToOpen:urls
                           inIncognito:inIncognito
                                newTab:newTab];
}

- (void)openUrls:(const std::vector<GURL>&)urls
     inIncognito:(BOOL)inIncognito
          newTab:(BOOL)newTab {
  BOOL openInForegroundTab = YES;
  WebStateList* webStateList = self.browser->GetWebStateList();
  for (const GURL& url : urls) {
    DCHECK(url.is_valid()) << [self description];
    // TODO(crbug.com/695749): Force url to open in non-incognito mode. if
    // !IsURLAllowedInIncognito(url).

    if (openInForegroundTab) {
      // Only open the first URL in foreground tab.
      openInForegroundTab = NO;

      // TODO(crbug.com/695749): See if we need different metrics for 'Open
      // all', 'Open all in incognito' and 'Open in incognito'.
      new_tab_page_uma::RecordAction(_browserState->IsOffTheRecord(),
                                     webStateList->GetActiveWebState(),
                                     new_tab_page_uma::ACTION_OPENED_BOOKMARK);
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));
      LogBookmarkUseForDefaultBrowserPromo();

      if (newTab ||
          ((!!inIncognito) != _currentBrowserState->IsOffTheRecord())) {
        // Open in new tab if it is specified or target tab mode is different
        // from current tab mode.
        [self openURLInNewTab:url inIncognito:inIncognito inBackground:NO];
      } else {
        // Open in current tab otherwise.
        [self openURLInCurrentTab:url];
      }
    } else {
      // Open other URLs (if any) in background tabs.
      [self openURLInNewTab:url inIncognito:inIncognito inBackground:YES];
    }
  }  // end for
}

#pragma mark - BookmarksCommands

- (void)bookmarkWithWebState:(web::WebState*)webState {
  GURL URL = webState->GetLastCommittedURL();
  NSString* title = tab_util::GetTabTitle(webState);
  [self createOrEditBookmarkWithURL:[[URLWithTitle alloc] initWithURL:URL
                                                                title:title]];
}

- (void)bulkCreateBookmarksWithURLs:(NSArray<NSURL*>*)URLs {
  if (!AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    return;
  }

  __weak BookmarksCoordinator* weakSelf = self;
  void (^viewAction)() = ^{
    base::RecordAction(base::UserMetricsAction(
        "IOSBookmarksAddedInBulkSnackbarViewButtonClicked"));
    [weakSelf presentBookmarks];
  };

  [self.snackbarCommandsHandler
      showSnackbarMessage:[self.mediator bulkAddBookmarksWithURLs:URLs
                                                       viewAction:viewAction]];
}

- (void)createOrEditBookmarkWithURL:(URLWithTitle*)URLWithTitle {
  DCHECK(URLWithTitle) << [self description];
  NSString* title = URLWithTitle.title;
  GURL URL = URLWithTitle.URL;
  if (!AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    return;
  }

  const BookmarkNode* existingBookmark =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          URL, _localOrSyncableBookmarkModel.get(),
          _accountBookmarkModel.get());
  if (existingBookmark) {
    [self presentBookmarkEditorForURL:URL];
  } else {
    [self createBookmarkURL:URL title:title];
  }
}

- (void)bookmarkWithFolderChooser:(NSArray<URLWithTitle*>*)URLs {
  DCHECK(URLs.count > 0) << "URLs are missing " << [self description];

  if (!AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    return;
  }

  _URLs = URLs;
  [self presentFolderChooser];
}

- (void)openToExternalBookmark:(GURL)URL {
  if (!AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    return;
  }

  const BookmarkNode* existingBookmark =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          URL, _localOrSyncableBookmarkModel.get(),
          _accountBookmarkModel.get());
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    [self presentBookmarksAtDisplayedFolderNode:_localOrSyncableBookmarkModel
                                                    ->mobile_node()
                              selectingBookmark:existingBookmark];
  } else if (existingBookmark) {
    [self presentBookmarksAtDisplayedFolderNode:existingBookmark->parent()
                              selectingBookmark:existingBookmark];
  } else {
    // Couldn't find the bookmark for the requested URL, just open mobile
    // bookmarks.
    [self presentBookmarksAtDisplayedFolderNode:_localOrSyncableBookmarkModel
                                                    ->mobile_node()
                              selectingBookmark:nil];
  }
}

#pragma mark - Private

// Stops `self.folderChooserCoordinator` and sets `currentPresentedState` to
// `NONE.
- (void)stopBookmarksFolderChooserCoordinator {
  DCHECK_EQ(PresentedState::FOLDER_SELECTION, self.currentPresentedState)
      << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  DCHECK(self.folderChooserCoordinator) << [self description];
  [self.folderChooserCoordinator stop];
  self.folderChooserCoordinator.delegate = nil;
  self.folderChooserCoordinator = nil;
  self.currentPresentedState = PresentedState::NONE;
}

// Stops `self.folderEditorCoordinator` and sets `currentPresentedState` to
// `NONE.
- (void)stopBookmarksFolderEditorCoordinator {
  DCHECK_EQ(PresentedState::FOLDER_EDITOR, self.currentPresentedState)
      << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  DCHECK(self.folderEditorCoordinator) << [self description];
  [self.folderEditorCoordinator stop];
  self.folderEditorCoordinator.delegate = nil;
  self.folderEditorCoordinator = nil;
  self.currentPresentedState = PresentedState::NONE;
}

// Stops `self.bookmarkEditorCoordinator` and sets `currentPresentedState` to
// `NONE.
- (void)stopBookmarksEditorCoordinator {
  DCHECK_EQ(PresentedState::BOOKMARK_EDITOR, self.currentPresentedState)
      << [self description];
  DCHECK(self.bookmarkEditorCoordinator) << [self description];
  DCHECK(!self.bookmarkNavigationController) << [self description];
  self.bookmarkEditorCoordinator.delegate = nil;
  [self.bookmarkEditorCoordinator stop];
  self.bookmarkEditorCoordinator = nil;
  self.currentPresentedState = PresentedState::NONE;
}

// Presents `viewController` using the appropriate presentation and styling,
// depending on whether the UIRefresh experiment is enabled or disabled. Sets
// `self.bookmarkNavigationController` to the UINavigationController subclass
// used. If `replacementViewControllers` is not nil, those controllers are
// swapped in to the UINavigationController instead of `viewController`.
- (void)presentTableViewController:
            (LegacyChromeTableViewController*)viewController
    withReplacementViewControllers:
        (NSArray<LegacyChromeTableViewController*>*)replacementViewControllers {
  TableViewNavigationController* navController =
      [[TableViewNavigationController alloc] initWithTable:viewController];
  navController.modalPresentationStyle = UIModalPresentationFormSheet;
  self.bookmarkNavigationController = navController;
  if (replacementViewControllers) {
    [navController setViewControllers:replacementViewControllers];
  }
  self.bookmarkNavigationController.delegate = self;

  navController.toolbarHidden = YES;
  navController.presentationController.delegate = self;

  [self.baseViewController presentViewController:navController
                                        animated:YES
                                      completion:nil];
}

- (void)openURLInCurrentTab:(const GURL&)url {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (url.SchemeIs(url::kJavaScriptScheme) && webStateList) {  // bookmarklet
    LoadJavaScriptURL(url, _browserState.get(),
                      webStateList->GetActiveWebState());
    return;
  }
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)openURLInNewTab:(const GURL&)url
            inIncognito:(BOOL)inIncognito
           inBackground:(BOOL)inBackground {
  // TODO(crbug.com/695749):  Open bookmarklet in new tab doesn't work.  See how
  // to deal with this later.
  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  params.SetInBackground(inBackground);
  params.in_incognito = inIncognito;
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

// Presents the bookmarks browser modally. If `selectingBookmark` is non-nil,
// then the bookmarks modal is changed to edit mode and `selectingBookmark` is
// identified in the list of bookmarks and selected.
- (void)presentBookmarksAtDisplayedFolderNode:
            (const BookmarkNode*)displayedFolderNode
                            selectingBookmark:
                                (const BookmarkNode*)bookmarkNode {
  if (self.bookmarkNavigationController) {
    // Since bookmark browser is dismissed asynchronously through
    // `-presentationControllerDidDismiss:`, it is possible for this method to
    // be called before `self.bookmarkNavigationController` is reset. In that
    // case reset `self.bookmarkNavigationController` and continue.
    DCHECK_EQ(PresentedState::BOOKMARK_BROWSER, self.currentPresentedState)
        << [self description];
    [self bookmarkBrowserDismissed];
  }
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.bookmarkNavigationController) << [self description];

  self.bookmarkBrowser =
      [[BookmarksHomeViewController alloc] initWithBrowser:self.browser];
  self.bookmarkBrowser.homeDelegate = self;
  self.bookmarkBrowser.applicationCommandsHandler =
      self.applicationCommandsHandler;
  self.bookmarkBrowser.snackbarCommandsHandler = self.snackbarCommandsHandler;

  NSArray<BookmarksHomeViewController*>* replacementViewControllers = nil;
  if (AreAllAvailableBookmarkModelsLoaded(_browserState.get())) {
    // Set the root node if the model has been loaded. If the model has not been
    // loaded yet, the root node will be set in BookmarksHomeViewController
    // after the model is finished loading.
    self.bookmarkBrowser.displayedFolderNode = displayedFolderNode;
    [self.bookmarkBrowser setExternalBookmark:bookmarkNode];
    if (displayedFolderNode == _localOrSyncableBookmarkModel->root_node()) {
      replacementViewControllers =
          [self.bookmarkBrowser cachedViewControllerStack];
    }
  }

  [self presentTableViewController:self.bookmarkBrowser
      withReplacementViewControllers:replacementViewControllers];
  self.currentPresentedState = PresentedState::BOOKMARK_BROWSER;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  DCHECK_EQ(PresentedState::BOOKMARK_BROWSER, self.currentPresentedState)
      << [self description];
  DCHECK(self.bookmarkNavigationController) << [self description];
  for (UIViewController* controller in self.bookmarkNavigationController
           .viewControllers) {
    BookmarksHomeViewController* bookmarksHomeViewController =
        base::apple::ObjCCastStrict<BookmarksHomeViewController>(controller);
    [bookmarksHomeViewController willDismissBySwipeDown];
  }
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [self canDismiss];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSBookmarkManagerCloseWithSwipe"));
  [self bookmarkBrowserDismissed];
}

#pragma mark - UINavigationControllerDelegate

- (id<UIViewControllerAnimatedTransitioning>)
               navigationController:
                   (UINavigationController*)navigationController
    animationControllerForOperation:(UINavigationControllerOperation)operation
                 fromViewController:(UIViewController*)fromVC
                   toViewController:(UIViewController*)toVC {
  if ([fromVC isKindOfClass:[ManageSyncSettingsTableViewController class]]) {
    return nil;
  }
  if (operation == UINavigationControllerOperationPop) {
    BookmarksHomeViewController* poppedHome =
        base::apple::ObjCCastStrict<BookmarksHomeViewController>(fromVC);
    // `shutdown` must wait for the next run of the main loop, so that
    // methods such as `textFieldDidEndEditing` have time to be run.
    dispatch_async(dispatch_get_main_queue(), ^{
      [poppedHome shutdown];
    });
  }
  return nil;
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(_manageSyncSettingsCoordinator, coordinator);
  [_manageSyncSettingsCoordinator stop];
  _manageSyncSettingsCoordinator = nil;
}

- (NSString*)manageSyncSettingsCoordinatorTitle {
  return l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TITLE);
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, state=%d bookmarkEditorCoordinator=%p, "
          @"bookmarkNavigationController=%p (presented: %@), "
          @"folderEditorCoordinator=%p, folderChooserCoordinator=%p "
          @"localOrSyncableBookmarkModel=%p, accountBookmarkModel=%p, "
          @"manageSyncSettingsCoordinator=%p>",
          NSStringFromClass([self class]), self,
          static_cast<int>(self.currentPresentedState),
          self.bookmarkEditorCoordinator, self.bookmarkNavigationController,
          self.bookmarkNavigationController ? @"YES" : @"NO",
          self.folderEditorCoordinator, self.folderChooserCoordinator,
          _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get(),
          _manageSyncSettingsCoordinator];
}

@end

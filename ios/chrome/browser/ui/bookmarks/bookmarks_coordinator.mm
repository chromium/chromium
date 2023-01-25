// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"

#import <stdint.h>

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@interface BookmarksCoordinator () <
    BookmarksEditorViewControllerDelegate,
    BookmarksFolderEditorViewControllerDelegate,
    BookmarksFolderChooserViewControllerDelegate,
    BookmarksHomeViewControllerDelegate> {
  // The browser bookmarks are presented in.
  Browser* _browser;  // weak

  // The browser state of the current user.
  ChromeBrowserState* _currentBrowserState;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ChromeBrowserState* _browserState;  // weak

  // The web state list currently in use.
  WebStateList* _webStateList;
}

// The type of view controller that is being presented.
@property(nonatomic, assign) PresentedState currentPresentedState;

// The navigation controller that is being presented, if any.
// `self.bookmarkBrowser`, `self.bookmarkEditor`, and `self.folderEditor` are
// children of this navigation controller.
@property(nonatomic, strong)
    UINavigationController* bookmarkNavigationController;

// The delegate provided to `self.bookmarkNavigationController`.
@property(nonatomic, strong)
    BookmarkNavigationControllerDelegate* bookmarkNavigationControllerDelegate;

// The bookmark model in use.
@property(nonatomic, assign) BookmarkModel* bookmarkModel;

// A reference to the potentially presented bookmark browser. This will be
// non-nil when `currentPresentedState` is BOOKMARK_BROWSER.
@property(nonatomic, strong) BookmarksHomeViewController* bookmarkBrowser;

// A reference to the potentially presented single bookmark editor. This will be
// non-nil when `currentPresentedState` is BOOKMARK_EDITOR.
@property(nonatomic, strong) BookmarksEditorViewController* bookmarkEditor;

// A reference to the potentially presented folder editor. This will be non-nil
// when `currentPresentedState` is FOLDER_EDITOR.
@property(nonatomic, strong) BookmarksFolderEditorViewController* folderEditor;

// A reference to the potentially presented folder selector. This will be
// non-nil when `currentPresentedState` is FOLDER_SELECTION.
@property(nonatomic, strong)
    BookmarksFolderChooserViewController* folderSelector;

@property(nonatomic, copy) void (^folderSelectionCompletionBlock)
    (const bookmarks::BookmarkNode*);

@property(nonatomic, strong) BookmarkMediator* mediator;

// Handler for Application Commands.
@property(nonatomic, readonly, weak) id<ApplicationCommands>
    applicationCommandsHandler;

// Handler for Snackbar Commands.
@property(nonatomic, readonly, weak) id<SnackbarCommands>
    snackbarCommandsHandler;

// Dismisses the bookmark browser.  If `urlsToOpen` is not empty, then the user
// has selected to navigate to those URLs with specified tab mode.
- (void)dismissBookmarkBrowserAnimated:(BOOL)animated
                            urlsToOpen:(const std::vector<GURL>&)urlsToOpen
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab;

// Dismisses the bookmark editor.
- (void)dismissBookmarkEditorAnimated:(BOOL)animated;

// Dismisses the folder editor.
- (void)dismissFolderEditorAnimated:(BOOL)animated;

@end

@implementation BookmarksCoordinator
@synthesize applicationCommandsHandler = _applicationCommandsHandler;
@synthesize snackbarCommandsHandler = _snackbarCommandsHandler;
@synthesize baseViewController = _baseViewController;
@synthesize bookmarkBrowser = _bookmarkBrowser;
@synthesize bookmarkEditor = _bookmarkEditor;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize bookmarkNavigationController = _bookmarkNavigationController;
@synthesize bookmarkNavigationControllerDelegate =
    _bookmarkNavigationControllerDelegate;
@synthesize currentPresentedState = _currentPresentedState;
@synthesize delegate = _delegate;
@synthesize folderEditor = _folderEditor;
@synthesize mediator = _mediator;

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _browser = browser;
    // Bookmarks are always opened with the main browser state, even in
    // incognito mode.
    _currentBrowserState = browser->GetBrowserState();
    _browserState = _currentBrowserState->GetOriginalChromeBrowserState();
    _webStateList = browser->GetWebStateList();
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    _mediator = [[BookmarkMediator alloc]
        initWithWithBookmarkModel:self.bookmarkModel
                            prefs:_browserState->GetPrefs()];
    _currentPresentedState = PresentedState::NONE;
    DCHECK(_bookmarkModel);
  }
  return self;
}

- (void)dealloc {
  [self shutdown];
}

- (void)shutdown {
  [self bookmarkBrowserDismissed];

  _bookmarkBrowser.homeDelegate = nil;
  [_bookmarkBrowser shutdown];
  _bookmarkBrowser = nil;

  _bookmarkEditor.delegate = nil;
  [_bookmarkEditor shutdown];
  _bookmarkEditor = nil;
}

- (id<ApplicationCommands>)applicationCommandsHandler {
  // Using lazy loading here to avoid potential crashes with ApplicationCommands
  // not being yet dispatched.
  if (!_applicationCommandsHandler) {
    _applicationCommandsHandler = HandlerForProtocol(
        _browser->GetCommandDispatcher(), ApplicationCommands);
  }
  return _applicationCommandsHandler;
}

- (id<SnackbarCommands>)snackbarCommandsHandler {
  // Using lazy loading here to avoid potential crashes with SnackbarCommands
  // not being yet dispatched.
  if (!_snackbarCommandsHandler) {
    _snackbarCommandsHandler =
        HandlerForProtocol(_browser->GetCommandDispatcher(), SnackbarCommands);
  }
  return _snackbarCommandsHandler;
}

- (void)bookmarkURL:(const GURL&)URL title:(NSString*)title {
  if (!self.bookmarkModel->loaded()) {
    return;
  }

  __weak BookmarksCoordinator* weakSelf = self;
  // Copy of `URL` to be captured in block.
  GURL bookmarkedURL(URL);
  void (^editAction)() = ^{
    [weakSelf presentBookmarkEditorForURL:bookmarkedURL];
  };

  [self.snackbarCommandsHandler
      showSnackbarMessage:[self.mediator addBookmarkWithTitle:title
                                                          URL:bookmarkedURL
                                                   editAction:editAction]];
}

- (void)presentBookmarkEditorForURL:(const GURL&)URL {
  if (!self.bookmarkModel->loaded()) {
    return;
  }

  const BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(URL);
  if (!bookmark) {
    return;
  }
  [self presentEditorForURLNode:bookmark];
}

- (void)presentBookmarks {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.bookmarkNavigationController);

  self.bookmarkBrowser =
      [[BookmarksHomeViewController alloc] initWithBrowser:_browser];
  self.bookmarkBrowser.homeDelegate = self;
  self.bookmarkBrowser.applicationCommandsHandler =
      self.applicationCommandsHandler;
  self.bookmarkBrowser.snackbarCommandsHandler = self.snackbarCommandsHandler;

  NSArray<BookmarksHomeViewController*>* replacementViewControllers = nil;
  if (self.bookmarkModel->loaded()) {
    // Set the root node if the model has been loaded. If the model has not been
    // loaded yet, the root node will be set in BookmarksHomeViewController
    // after the model is finished loading.
    [self.bookmarkBrowser setRootNode:self.bookmarkModel->root_node()];
    replacementViewControllers =
        [self.bookmarkBrowser cachedViewControllerStack];
  }

  [self presentTableViewController:self.bookmarkBrowser
      withReplacementViewControllers:replacementViewControllers];
  self.currentPresentedState = PresentedState::BOOKMARK_BROWSER;
}

- (void)presentFolderPickerWithCompletion:
    (void (^)(const bookmarks::BookmarkNode*))block {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(block);

  [self dismissSnackbar];

  self.currentPresentedState = PresentedState::FOLDER_SELECTION;
  self.folderSelectionCompletionBlock = [block copy];

  std::set<const BookmarkNode*> editedNodes;
  self.folderSelector = [[BookmarksFolderChooserViewController alloc]
      initWithBookmarkModel:self.bookmarkModel
           allowsNewFolders:YES
                editedNodes:editedNodes
               allowsCancel:YES
             selectedFolder:nil
                    browser:_browser];
  self.folderSelector.delegate = self;
  self.folderSelector.snackbarCommandsHandler = self.snackbarCommandsHandler;

  [self presentTableViewController:self.folderSelector
      withReplacementViewControllers:nil];
}

- (void)presentEditorForURLNode:(const bookmarks::BookmarkNode*)node {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(node);
  DCHECK_EQ(node->type(), BookmarkNode::URL);

  [self dismissSnackbar];

  self.currentPresentedState = PresentedState::BOOKMARK_EDITOR;
  BookmarksEditorViewController* bookmarkEditor =
      [[BookmarksEditorViewController alloc] initWithBookmark:node
                                                      browser:_browser];
  bookmarkEditor.delegate = self;
  bookmarkEditor.snackbarCommandsHandler = self.snackbarCommandsHandler;
  self.bookmarkEditor = bookmarkEditor;

  [self presentTableViewController:bookmarkEditor
      withReplacementViewControllers:nil];
}

- (void)presentEditorForFolderNode:(const bookmarks::BookmarkNode*)node {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(node);
  DCHECK_EQ(node->type(), BookmarkNode::FOLDER);

  [self dismissSnackbar];
  self.currentPresentedState = PresentedState::FOLDER_EDITOR;
  BookmarksFolderEditorViewController* folderEditor =
      [BookmarksFolderEditorViewController
          folderEditorWithBookmarkModel:self.bookmarkModel
                                 folder:node
                                browser:_browser];
  folderEditor.delegate = self;
  folderEditor.snackbarCommandsHandler = self.snackbarCommandsHandler;
  self.folderEditor = folderEditor;

  [self presentTableViewController:folderEditor
      withReplacementViewControllers:nil];
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

  ProceduralBlock completion = ^{
    [self bookmarkBrowserDismissed];

    if (!openUrlsAfterDismissal) {
      return;
    }
    [self openUrls:urlsToOpenAfterDismissal
        inIncognito:inIncognito
             newTab:newTab];
  };

  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:completion];
  } else {
    completion();
  }
  self.currentPresentedState = PresentedState::NONE;
}

- (void)bookmarkBrowserDismissed {
  // TODO(crbug.com/940856): Make sure navigaton
  // controller doesn't keep any controllers. Without
  // this there's a memory leak of (almost) every BHVC
  // the user visits.
  [self.bookmarkNavigationController setViewControllers:@[] animated:NO];

  self.bookmarkBrowser.homeDelegate = nil;
  self.bookmarkBrowser = nil;
  self.bookmarkNavigationController = nil;
  self.bookmarkNavigationControllerDelegate = nil;
}

- (void)dismissBookmarkEditorAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::BOOKMARK_EDITOR) {
    return;
  }
  DCHECK(self.bookmarkNavigationController);

  self.bookmarkEditor.delegate = nil;
  self.bookmarkEditor = nil;
  [self.bookmarkNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.bookmarkNavigationController = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissFolderEditorAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::FOLDER_EDITOR) {
    return;
  }
  DCHECK(self.bookmarkNavigationController);

  [self.bookmarkNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.folderEditor.delegate = nil;
                           self.folderEditor = nil;
                           self.bookmarkNavigationController = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissFolderSelectionAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::FOLDER_SELECTION) {
    return;
  }
  DCHECK(self.bookmarkNavigationController);

  [self.bookmarkNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.folderSelector.delegate = nil;
                           self.folderSelector = nil;
                           self.bookmarkNavigationController = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated {
  // No urls to open.  So it does not care about inIncognito and newTab.
  [self dismissBookmarkBrowserAnimated:animated
                            urlsToOpen:std::vector<GURL>()
                           inIncognito:NO
                                newTab:NO];
  [self dismissBookmarkEditorAnimated:animated];
}

- (void)dismissSnackbar {
  // Dismiss any bookmark related snackbar this controller could have presented.
  [MDCSnackbarManager.defaultManager
      dismissAndCallCompletionBlocksWithCategory:
          bookmark_utils_ios::kBookmarksSnackbarCategory];
}

#pragma mark - BookmarksEditorViewControllerDelegate

- (BOOL)bookmarkEditor:(BookmarksEditorViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const BookmarkNode*)bookmark {
  return YES;
}

- (void)bookmarkEditorWantsDismissal:
    (BookmarksEditorViewController*)controller {
  [self dismissBookmarkEditorAnimated:YES];
}

- (void)bookmarkEditorWillCommitTitleOrUrlChange:
    (BookmarksEditorViewController*)controller {
  [self.delegate bookmarksCoordinatorWillCommitTitleOrURLChange:self];
}

#pragma mark - BookmarksFolderEditorViewControllerDelegate

- (void)bookmarkFolderEditor:(BookmarksFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  [self dismissFolderEditorAnimated:YES];
}

- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarksFolderEditorViewController*)folderEditor {
  [self dismissFolderEditorAnimated:YES];
}

- (void)bookmarkFolderEditorDidCancel:
    (BookmarksFolderEditorViewController*)folderEditor {
  [self dismissFolderEditorAnimated:YES];
}

- (void)bookmarkFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorViewController*)controller {
  [self.delegate bookmarksCoordinatorWillCommitTitleOrURLChange:self];
}

#pragma mark - BookmarksFolderChooserViewControllerDelegate

- (void)folderPicker:(BookmarksFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const bookmarks::BookmarkNode*)folder {
  [self dismissFolderSelectionAnimated:YES];

  if (self.folderSelectionCompletionBlock) {
    self.folderSelectionCompletionBlock(folder);
  }
}

- (void)folderPickerDidCancel:
    (BookmarksFolderChooserViewController*)folderPicker {
  [self dismissFolderSelectionAnimated:YES];
}

- (void)folderPickerDidDismiss:
    (BookmarksFolderChooserViewController*)folderPicker {
  [self dismissFolderSelectionAnimated:YES];
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
  for (const GURL& url : urls) {
    DCHECK(url.is_valid());
    // TODO(crbug.com/695749): Force url to open in non-incognito mode. if
    // !IsURLAllowedInIncognito(url).

    if (openInForegroundTab) {
      // Only open the first URL in foreground tab.
      openInForegroundTab = NO;

      // TODO(crbug.com/695749): See if we need different metrics for 'Open
      // all', 'Open all in incognito' and 'Open in incognito'.
      new_tab_page_uma::RecordAction(_browserState,
                                     _webStateList->GetActiveWebState(),
                                     new_tab_page_uma::ACTION_OPENED_BOOKMARK);
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));
      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);

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

- (void)bookmark:(BookmarkAddCommand*)command {
  DCHECK(command.URLs.count > 0) << "URLs are missing";

  if (!self.bookmarkModel->loaded()) {
    return;
  }

  if (command.URLs.count == 1 && !command.presentFolderChooser) {
    URLWithTitle* URLWithTitle = command.URLs.firstObject;
    DCHECK(URLWithTitle);

    const BookmarkNode* existingBookmark =
        self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
            URLWithTitle.URL);

    if (existingBookmark) {
      [self presentBookmarkEditorForURL:URLWithTitle.URL];
    } else {
      [self bookmarkURL:URLWithTitle.URL title:URLWithTitle.title];
    }
    return;
  }

  __weak BookmarksCoordinator* weakSelf = self;
  [self presentFolderPickerWithCompletion:^(
            const bookmarks::BookmarkNode* folder) {
    BookmarksCoordinator* strongSelf = weakSelf;
    if (folder && strongSelf) {
      [strongSelf.snackbarCommandsHandler
          showSnackbarMessage:[strongSelf.mediator addBookmarks:command.URLs
                                                       toFolder:folder]];
    }
  }];
}

#pragma mark - Private

// Presents `viewController` using the appropriate presentation and styling,
// depending on whether the UIRefresh experiment is enabled or disabled. Sets
// `self.bookmarkNavigationController` to the UINavigationController subclass
// used, and may set `self.bookmarkNavigationControllerDelegate` depending on
// whether or not the desired transition requires those objects.  If
// `replacementViewControllers` is not nil, those controllers are swapped in to
// the UINavigationController instead of `viewController`.
- (void)presentTableViewController:
            (ChromeTableViewController<
                UIAdaptivePresentationControllerDelegate>*)viewController
    withReplacementViewControllers:
        (NSArray<ChromeTableViewController*>*)replacementViewControllers {
  TableViewNavigationController* navController =
      [[TableViewNavigationController alloc] initWithTable:viewController];
  self.bookmarkNavigationController = navController;
  if (replacementViewControllers) {
    [navController setViewControllers:replacementViewControllers];
  }

  navController.toolbarHidden = YES;
  self.bookmarkNavigationControllerDelegate =
      [[BookmarkNavigationControllerDelegate alloc] init];
  navController.delegate = self.bookmarkNavigationControllerDelegate;

  [navController setModalPresentationStyle:UIModalPresentationFormSheet];

  [self.baseViewController presentViewController:navController
                                        animated:YES
                                      completion:nil];
}

- (void)openURLInCurrentTab:(const GURL&)url {
  if (url.SchemeIs(url::kJavaScriptScheme) && _webStateList) {  // bookmarklet
    LoadJavaScriptURL(url, _browserState, _webStateList->GetActiveWebState());
    return;
  }
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  UrlLoadingBrowserAgent::FromBrowser(_browser)->Load(params);
}

- (void)openURLInNewTab:(const GURL&)url
            inIncognito:(BOOL)inIncognito
           inBackground:(BOOL)inBackground {
  // TODO(crbug.com/695749):  Open bookmarklet in new tab doesn't work.  See how
  // to deal with this later.
  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  params.SetInBackground(inBackground);
  params.in_incognito = inIncognito;
  UrlLoadingBrowserAgent::FromBrowser(_browser)->Load(params);
}

@end

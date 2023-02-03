// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator.h"

#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksEditorCoordinator () <
    BookmarksEditorViewControllerDelegate,
    BookmarksEditorMediatorDelegate,
    BookmarksFolderChooserViewControllerDelegate> {
  // BookmarkNode to edit.
  const bookmarks::BookmarkNode* _node;

  // The editor view controller owned and presented by this coordinator.
  // It is wrapped in a TableViewNavigationController.
  BookmarksEditorViewController* _viewController;

  // The editor mediator owned and presented by this coordinator.
  // It is wrapped in a TableViewNavigationController.
  BookmarksEditorMediator* _mediator;

  // Receives commands to show a snackbar once a bookmark is edited or deleted.
  id<SnackbarCommands> _snackbarCommandsHandler;

  // The navigation controller that is being presented. The bookmark editor view
  // controller is the child of this navigation controller.
  UINavigationController* _navigationController;

  // The delegate provided to `_bookmarkNavigationController`.
  BookmarkNavigationControllerDelegate* _navigationControllerDelegate;

  // The folder picker view controller.
  BookmarksFolderChooserViewController* _folderViewController;
}

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

@end

@implementation BookmarksEditorCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      node:(const bookmarks::BookmarkNode*)node
                   snackbarCommandsHandler:
                       (id<SnackbarCommands>)snackbarCommandsHandler {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _node = node;
    _snackbarCommandsHandler = snackbarCommandsHandler;
  }
  return self;
}

- (void)start {
  [super start];
  ChromeBrowserState* browserState =
      self.browser->GetBrowserState()->GetOriginalChromeBrowserState();
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);

  _viewController =
      [[BookmarksEditorViewController alloc] initWithBrowser:self.browser];
  _viewController.delegate = self;
  _viewController.snackbarCommandsHandler = _snackbarCommandsHandler;

  _mediator = [[BookmarksEditorMediator alloc]
      initWithBookmarkModel:model
                   bookmark:_node
                      prefs:browserState->GetPrefs()];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _viewController.mutator = _mediator;

  _navigationControllerDelegate =
      [[BookmarkNavigationControllerDelegate alloc] init];
  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];
  _navigationController.toolbarHidden = YES;
  _navigationController.delegate = _navigationControllerDelegate;
  [_navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  DCHECK(_navigationController);
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController.snackbarCommandsHandler = nil;
  _viewController.mutator = nil;
  _viewController = nil;
  _snackbarCommandsHandler = nil;
  _folderViewController.delegate = nil;
  _folderViewController = nil;

  // animatedDismissal should have been explicitly set before calling stop.
  [_navigationController dismissViewControllerAnimated:self.animatedDismissal
                                            completion:nil];
  _navigationController = nil;
  _navigationControllerDelegate = nil;
}

#pragma mark - BookmarksEditorViewControllerDelegate

- (void)moveBookmark {
  DCHECK([_mediator bookmarkModel]);
  DCHECK(!_folderViewController);

  std::set<const bookmarks::BookmarkNode*> editedNodes{[_mediator bookmark]};
  _folderViewController = [[BookmarksFolderChooserViewController alloc]
      initWithBookmarkModel:[_mediator bookmarkModel]
           allowsNewFolders:YES
                editedNodes:editedNodes
               allowsCancel:NO
             selectedFolder:[_mediator folder]
                    browser:self.browser];
  _folderViewController.delegate = self;
  _folderViewController.snackbarCommandsHandler = _snackbarCommandsHandler;
  _folderViewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  [_navigationController pushViewController:_folderViewController animated:YES];
}

- (void)bookmarkEditorWantsDismissal:
    (BookmarksEditorViewController*)controller {
  [self.delegate bookmarksEditorCoordinatorShouldStop:self];
}

- (void)bookmarkEditorWillCommitTitleOrURLChange:
    (BookmarksEditorViewController*)controller {
  [self.delegate bookmarkEditorWillCommitTitleOrURLChange:self];
}
#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                           title:nil
                         message:nil
                   barButtonItem:_viewController.cancelItem];

  __weak __typeof(self) weakSelf = self;
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_SAVE_CHANGES)
                action:^{
                  BookmarksEditorCoordinator* strongSelf = weakSelf;
                  if (strongSelf != nil) {
                    [strongSelf->_viewController save];
                  }
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_DISCARD_CHANGES)
                action:^{
                  BookmarksEditorCoordinator* strongSelf = weakSelf;
                  if (strongSelf != nil) {
                    [strongSelf->_viewController cancel];
                  }
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_VIEW_CONTROLLER_DISMISS_CANCEL_CHANGES)
                action:^{
                  BookmarksEditorCoordinator* strongSelf = weakSelf;
                  if (strongSelf != nil) {
                    [strongSelf->_viewController setNavigationItemsEnabled:YES];
                  }
                }
                 style:UIAlertActionStyleCancel];

  [_viewController setNavigationItemsEnabled:NO];
  [self.actionSheetCoordinator start];
}

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Resign first responder if trying to dismiss the VC so the keyboard doesn't
  // linger until the VC dismissal has completed.
  [_viewController.view endEditing:YES];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_viewController dismissBookmarkEditorView];
}

#pragma mark - BookmarksEditorMediatorDelegate

- (void)bookmarkEditorMediatorWantsDismissal:
    (BookmarksEditorMediator*)mediator {
  [self.delegate bookmarksEditorCoordinatorShouldStop:self];
}

- (void)bookmarkDidMoveToParent:(const bookmarks::BookmarkNode*)newParent {
  [_folderViewController changeSelectedFolder:newParent];
}

#pragma mark - BookmarksFolderChooserViewControllerDelegate

- (void)folderPicker:(BookmarksFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const bookmarks::BookmarkNode*)folder {
  [_mediator changeFolder:folder];
  // This delegate method can be called on two occasions:
  // - the user selected a folder in the folder picker. In that case, the folder
  // picker should be popped;
  // - the user created a new folder, in which case the navigation stack
  // contains this bookmark editor (`self`), a folder picker and a folder
  // creator. In such a case, both the folder picker and creator shoud be popped
  // to reveal this bookmark editor. Thus the call to
  // `popToViewController:animated:`.
  [_navigationController popToViewController:_viewController animated:YES];
  _folderViewController.delegate = nil;
  _folderViewController = nil;
}

- (void)folderPickerDidCancel:
    (BookmarksFolderChooserViewController*)folderPicker {
  // This delegate method can only be called from the folder picker, which is
  // the only view controller on top of this bookmark editor (`self`). Thus the
  // call to `popViewControllerAnimated:`.
  [_navigationController popViewControllerAnimated:YES];
  _folderViewController.delegate = nil;
  _folderViewController = nil;
}

- (void)folderPickerDidDismiss:
    (BookmarksFolderChooserViewController*)folderPicker {
  _folderViewController.delegate = nil;
  _folderViewController = nil;
  [_viewController.view endEditing:YES];
  [self.delegate bookmarksEditorCoordinatorShouldStop:self];
}
@end

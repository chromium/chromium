// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksFolderEditorCoordinator () <
    BookmarksFolderEditorViewControllerDelegate,
    BookmarksFolderChooserCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate> {
  // The navigation controller is `nullptr` if the folder chooser view
  // controller is pushed into the base navigation controller.
  // Otherwise, the navigation controller is presented in the base view
  // controller.
  UINavigationController* _navigationController;
  BookmarksFolderEditorViewController* _viewController;
  // Coordinator to show the folder chooser UI.
  BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
  // `_parentFolderNode` is only used when a new folder is added. The new
  // folder should be added in `_parentFolderNode`. If `_parentFolderNode` is
  // `nullptr`, then the new folder needs to be added in the default folder.
  const bookmarks::BookmarkNode* _parentFolderNode;
  // If `_folderNode` is set, the user is editing an existing folder and
  // `_parentFolderNode` should be `nullptr`. If `_folderNode` is not set, the
  // user is adding a new folder.
  const bookmarks::BookmarkNode* _folderNode;
}

@end

@implementation BookmarksFolderEditorCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                parentFolderNode:
                                    (const bookmarks::BookmarkNode*)
                                        parentFolder {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _parentFolderNode = parentFolder;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                folderNode:
                                    (const bookmarks::BookmarkNode*)folder {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _folderNode = folder;
  }
  return self;
}

- (void)start {
  [super start];
  // TODO(crbug.com/1402758): Create a mediator.
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  bookmarks::BookmarkModel* model =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          browserState);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  if (_baseNavigationController) {
    DCHECK(!_folderNode);
    _viewController = [BookmarksFolderEditorViewController
        folderCreatorWithBookmarkModel:model
                          parentFolder:_parentFolderNode
                               browser:self.browser
                      syncSetupService:syncSetupService
                           syncService:syncService];
    _viewController.delegate = self;
    _viewController.snackbarCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SnackbarCommands);
    [_baseNavigationController pushViewController:_viewController animated:YES];
  } else {
    DCHECK(!_navigationController);
    DCHECK(_folderNode);
    DCHECK(!_parentFolderNode);
    _viewController = [BookmarksFolderEditorViewController
        folderEditorWithBookmarkModel:model
                               folder:_folderNode
                              browser:self.browser
                     syncSetupService:syncSetupService
                          syncService:syncService];
    _viewController.delegate = self;
    _viewController.snackbarCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), SnackbarCommands);
    _navigationController = [[BookmarkNavigationController alloc]
        initWithRootViewController:_viewController];
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
    _navigationController.presentationController.delegate = self;

    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  // Stop child coordinator before stopping `self`.
  [self stopBookmarksFolderChooserCoordinator];

  DCHECK(_viewController);
  if (_baseNavigationController) {
    DCHECK_EQ(self.baseNavigationController.topViewController, _viewController);
    [_baseNavigationController popViewControllerAnimated:YES];
  } else if (_navigationController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController = nil;
  } else {
    // If there is no `_baseNavigationController` and `_navigationController`,
    // the view controller has been already dismissed. See
    // `presentationControllerDidDismiss:` and
    // `bookmarksFolderEditorDidDismiss:`.
    // Therefore `self.baseViewController.presentedViewController` must be
    // `nullptr`.
    DCHECK(!self.baseViewController.presentedViewController);
  }
  [_viewController disconnect];
  _viewController = nil;
}

- (BOOL)canDismiss {
  DCHECK(_viewController);
  return [_viewController canDismiss];
}

#pragma mark - BookmarksFolderEditorViewControllerDelegate

- (void)showBookmarksFolderChooserWithParentFolder:
            (const bookmarks::BookmarkNode*)parent
                                       hiddenNodes:
                                           (const std::set<
                                               const bookmarks::BookmarkNode*>&)
                                               hiddenNodes {
  DCHECK(!_folderChooserCoordinator);
  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseNavigationController:(_baseNavigationController
                                            ? _baseNavigationController
                                            : _navigationController)
                               browser:self.browser
                           hiddenNodes:hiddenNodes];
  _folderChooserCoordinator.allowsNewFolders = NO;
  [_folderChooserCoordinator setSelectedFolder:parent];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

- (void)bookmarksFolderEditor:(BookmarksFolderEditorViewController*)folderEditor
       didFinishEditingFolder:(const bookmarks::BookmarkNode*)folder {
  [_delegate bookmarksFolderEditorCoordinator:self
                   didFinishEditingFolderNode:folder];
}

- (void)bookmarksFolderEditorDidDeleteEditedFolder:
    (BookmarksFolderEditorViewController*)folderEditor {
  // Deleting the folder is only allowed when the user is editing an existing
  // folder.
  DCHECK(_folderNode);
  DCHECK(!_parentFolderNode);
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (void)bookmarksFolderEditorDidCancel:
    (BookmarksFolderEditorViewController*)folderEditor {
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (void)bookmarksFolderEditorDidDismiss:
    (BookmarksFolderEditorViewController*)folderEditor {
  DCHECK(_baseNavigationController);
  _baseNavigationController = nil;
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorViewController*)controller {
  [_delegate bookmarksFolderEditorWillCommitTitleChange:self];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);
  [self stopBookmarksFolderChooserCoordinator];

  [_viewController updateParentFolder:folder];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [self stopBookmarksFolderChooserCoordinator];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSBookmarksFolderEditorClosedWithSwipeDown"));
  [_viewController presentationControllerDidAttemptToDismiss];
}

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  // Resign first responder if trying to dismiss the VC so the keyboard doesn't
  // linger until the VC dismissal has completed.
  [_viewController.view endEditing:YES];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  DCHECK(_navigationController);
  _navigationController = nil;
  [_delegate bookmarksFolderEditorCoordinatorShouldStop:self];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [self canDismiss];
}

#pragma mark - Private

- (void)stopBookmarksFolderChooserCoordinator {
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
}

@end

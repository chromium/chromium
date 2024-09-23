// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_navigation_controller.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mediator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_mediator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_editor/bookmarks_folder_editor_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface BookmarksFolderChooserCoordinator () <
    BookmarksFolderChooserMediatorDelegate,
    BookmarksFolderChooserViewControllerPresentationDelegate,
    BookmarksFolderEditorCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation BookmarksFolderChooserCoordinator {
  BookmarksFolderChooserMediator* _mediator;
  // If folder chooser is created with a base view controller then folder
  // chooser will create and own `_navigationController` that should be deleted
  // in the end.
  // Otherwise, folder chooser is pushed into the `_baseNavigationController`
  // that it doesn't own.
  BookmarkNavigationController* _navigationController;
  BookmarksFolderChooserViewController* _viewController;
  // Coordinator to show the folder editor UI.
  BookmarksFolderEditorCoordinator* _folderEditorCoordinator;
  // List of nodes to hide when displaying folders. This is to avoid to move a
  // folder inside a child folder.
  std::set<const bookmarks::BookmarkNode*> _hiddenNodes;
  // The folder that has a blue check mark beside it in the UI.
  // This is only used for clients of this coordinator to update the UI. This
  // does not reflect the folder users chose by clicking. For that information
  // use `bookmarksFolderChooserCoordinatorDidConfirm:withSelectedFolder:`.
  raw_ptr<const bookmarks::BookmarkNode> _selectedFolder;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                         hiddenNodes:
                             (const std::set<const bookmarks::BookmarkNode*>&)
                                 hiddenNodes {
  self = [self initWithBaseViewController:navigationController
                                  browser:browser
                              hiddenNodes:hiddenNodes];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   hiddenNodes:(const std::set<const bookmarks::BookmarkNode*>&)
                                   hiddenNodes {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _hiddenNodes = hiddenNodes;
    _allowsNewFolders = YES;
  }
  return self;
}

- (BOOL)canDismiss {
  if (_folderEditorCoordinator) {
    return [_folderEditorCoordinator canDismiss];
  }
  return YES;
}

- (const std::set<const bookmarks::BookmarkNode*>&)editedNodes {
  return [_mediator editedNodes];
}

- (void)setSelectedFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(folder->is_folder());
  _selectedFolder = folder;
  _mediator.selectedFolderNode = _selectedFolder;
}

- (void)dealloc {
  DCHECK(!_viewController);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForProfile(profile);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  _mediator = [[BookmarksFolderChooserMediator alloc]
      initWithBookmarkModel:model
                editedNodes:std::move(_hiddenNodes)
      authenticationService:authenticationService
                syncService:syncService];
  _hiddenNodes.clear();
  _mediator.delegate = self;
  _mediator.selectedFolderNode = _selectedFolder;
  _viewController = [[BookmarksFolderChooserViewController alloc]
      initWithAllowsCancel:!_baseNavigationController
          allowsNewFolders:_allowsNewFolders];
  _viewController.delegate = self;
  _viewController.dataSource = _mediator;
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

  if (_baseNavigationController) {
    _viewController.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    [_baseNavigationController pushViewController:_viewController animated:YES];
  } else {
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
  [self stopBookmarksFolderEditorCoordinator];

  DCHECK(_mediator);
  DCHECK(_viewController);
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  if (_navigationController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController.presentationController.delegate = nil;
    _navigationController = nil;
  } else if (_baseNavigationController &&
             _baseNavigationController.presentingViewController) {
    // If `_baseNavigationController.presentingViewController` is `nil` then
    // the parent coordinator (who owns the `_baseNavigationController`) has
    // already been dismissed. In this case `_baseNavigationController` itself
    // is no longer being presented and this coordinator was dismissed as well.
    DCHECK_EQ(_baseNavigationController.topViewController, _viewController);
    [_baseNavigationController popViewControllerAnimated:YES];
  } else if (!_baseNavigationController) {
    // If there is no `_baseNavigationController` and `_navigationController`,
    // the view controller has been already dismissed. See
    // `presentationControllerDidDismiss:` and
    // `bookmarksFolderChooserViewControllerDidDismiss:`.
    // Therefore `self.baseViewController.presentedViewController` must be
    // `nil`.
    DCHECK(!self.baseViewController.presentedViewController);
  }
  _viewController.delegate = nil;
  _viewController.dataSource = nil;
  _viewController.mutator = nil;
  _viewController = nil;
}

#pragma mark - BookmarksFolderChooserMediatorDelegate

- (void)bookmarksFolderChooserMediatorWantsDismissal:
    (BookmarksFolderChooserMediator*)mediator {
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

#pragma mark - BookmarksFolderChooserViewControllerPresentationDelegate

- (void)showBookmarksFolderEditorWithParentFolderNode:
    (const bookmarks::BookmarkNode*)parentNode {
  DCHECK(!_folderEditorCoordinator);
  DCHECK(parentNode);
  _folderEditorCoordinator = [[BookmarksFolderEditorCoordinator alloc]
      initWithBaseNavigationController:(_baseNavigationController
                                            ? _baseNavigationController
                                            : _navigationController)
                               browser:self.browser
                      parentFolderNode:parentNode];
  _folderEditorCoordinator.delegate = self;
  [_folderEditorCoordinator start];
}

- (void)bookmarksFolderChooserViewController:
            (BookmarksFolderChooserViewController*)viewController
                         didFinishWithFolder:
                             (const bookmarks::BookmarkNode*)folder {
  [_delegate bookmarksFolderChooserCoordinatorDidConfirm:self
                                      withSelectedFolder:folder];
}

- (void)bookmarksFolderChooserViewControllerDidCancel:
    (BookmarksFolderChooserViewController*)viewController {
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

- (void)bookmarksFolderChooserViewControllerDidDismiss:
    (BookmarksFolderChooserViewController*)viewController {
  DCHECK(_baseNavigationController);
  _baseNavigationController = nil;
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

#pragma mark - BookmarksFolderEditorCoordinatorDelegate

- (void)bookmarksFolderEditorCoordinator:
            (BookmarksFolderEditorCoordinator*)folderEditor
              didFinishEditingFolderNode:
                  (const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(_folderEditorCoordinator);
  [self stopBookmarksFolderEditorCoordinator];
  [_delegate bookmarksFolderChooserCoordinatorDidConfirm:self
                                      withSelectedFolder:folder];
}

- (void)bookmarksFolderEditorCoordinatorShouldStop:
    (BookmarksFolderEditorCoordinator*)coordinator {
  DCHECK(_folderEditorCoordinator);
  [self stopBookmarksFolderEditorCoordinator];
}

- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorCoordinator*)coordinator {
  // Do nothing.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSBookmarksFolderChooserClosedWithSwipeDown"));
  DCHECK(_navigationController);
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [self canDismiss];
}

#pragma mark - Private

- (void)stopBookmarksFolderEditorCoordinator {
  _folderEditorCoordinator.delegate = nil;
  [_folderEditorCoordinator stop];
  _folderEditorCoordinator = nil;
}

@end

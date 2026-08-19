// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/folder_chooser/coordinator/bookmarks_folder_chooser_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/coordinator/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/coordinator/bookmarks_folder_chooser_mediator.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/coordinator/bookmarks_folder_chooser_mediator_delegate.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/ui/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/ui/bookmarks_folder_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/bookmarks/folder_editor/coordinator/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/bookmarks/folder_editor/coordinator/bookmarks_folder_editor_coordinator_delegate.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_navigation_controller.h"
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
  // List of id of moved nodes. This is to avoid to move a
  // folder inside a child folder. Only set between init and start.
  std::set<int64_t> _movedNodeIds;
  // The folder that has a blue check mark beside it in the UI.
  // This is only used for clients of this coordinator to update the UI. This
  // does not reflect the folder users chose by clicking. For that information
  // use `bookmarksFolderChooserCoordinatorDidConfirm:withSelectedFolder:`.
  raw_ptr<const bookmarks::BookmarkNode> _selectedFolder;
  // Whether this coordinator has been stopped.
  BOOL _stopped;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          movedNodes:
                              (const std::set<
                                  raw_ptr<const bookmarks::BookmarkNode>>&)
                                  movedNodes {
  self = [self initWithBaseViewController:navigationController
                                  browser:browser
                               movedNodes:movedNodes];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    movedNodes:(const std::set<
                                   raw_ptr<const bookmarks::BookmarkNode>>&)
                                   movedNodes {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    for (const raw_ptr<const bookmarks::BookmarkNode>& node : movedNodes) {
      _movedNodeIds.insert(node->id());
    }
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

- (std::set<raw_ptr<const bookmarks::BookmarkNode>>)movedNodes {
  return [_mediator movedNodes];
}

- (void)setSelectedFolder:(const bookmarks::BookmarkNode*)folder {
  CHECK(folder, base::NotFatalUntil::M150);
  CHECK(folder->is_folder(), base::NotFatalUntil::M150);
  _selectedFolder = folder;
  _mediator.selectedFolderNode = _selectedFolder;
}

- (void)dealloc {
  DUMP_WILL_BE_CHECK(!_viewController);
  DUMP_WILL_BE_CHECK(!_baseNavigationController);
  DUMP_WILL_BE_CHECK(!_mediator);
  DUMP_WILL_BE_CHECK(!_folderEditorCoordinator);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForProfile(profile);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  _mediator = [[BookmarksFolderChooserMediator alloc]
      initWithBookmarkModel:model
               movedNodeIds:std::move(_movedNodeIds)
      authenticationService:authenticationService
                syncService:syncService];
  _movedNodeIds.clear();
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
  if (_stopped) {
    return;
  }
  _stopped = YES;
  _viewController.coordinatorIsStopping = YES;
  [super stop];
  // Stop child coordinator before stopping `self`.
  [self stopBookmarksFolderEditorCoordinator];

  DUMP_WILL_BE_CHECK(_mediator);
  DUMP_WILL_BE_CHECK(_viewController);
  _mediator.UIDisabled = YES;
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  if (_navigationController) {
    // If the navigation controller is already being interactively dismissed by
    // UIKit (e.g. swipe-down gesture), skip programmatic dismissal to avoid
    // interrupting UIKit's transition animator and causing app hangs.
    if (!_navigationController.isBeingDismissed) {
      [_navigationController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:nil];
    }
    _navigationController.presentationController.delegate = nil;
    _navigationController = nil;
  } else if (_baseNavigationController &&
             _baseNavigationController.presentingViewController) {
    // If `_baseNavigationController.presentingViewController` is `nil` then
    // the parent coordinator (who owns the `_baseNavigationController`) has
    // already been dismissed. In this case `_baseNavigationController` itself
    // is no longer being presented and this coordinator was dismissed as well.
    //
    // Pop `_viewController` (and any child VCs on top of it) back to its
    // parent view controller. If `_viewController` was already popped
    // interactively (e.g. back button tap), `indexOfObject:` returns
    // `NSNotFound` and popping is skipped.
    if (!_baseNavigationController.isBeingDismissed) {
      NSUInteger index = [_baseNavigationController.viewControllers
          indexOfObject:_viewController];
      if (index != NSNotFound && index > 0) {
        UIViewController* previousVC =
            _baseNavigationController.viewControllers[index - 1];
        [_baseNavigationController popToViewController:previousVC animated:YES];
      }
    }
  }
  _viewController.delegate = nil;
  _viewController.dataSource = nil;
  _viewController.mutator = nil;
  _viewController = nil;
  _baseNavigationController = nil;
}

#pragma mark - BookmarksFolderChooserMediatorDelegate

- (void)bookmarksFolderChooserMediatorWantsDismissal:
    (BookmarksFolderChooserMediator*)mediator {
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

#pragma mark - BookmarksFolderChooserViewControllerPresentationDelegate

- (void)showBookmarksFolderEditorWithParentFolderNode:
    (const bookmarks::BookmarkNode*)parentNode {
  if (_folderEditorCoordinator || _mediator.UIDisabled) {
    return;
  }
  CHECK(parentNode, base::NotFatalUntil::M150);
  _folderEditorCoordinator = [[BookmarksFolderEditorCoordinator alloc]
      initWithBaseNavigationController:(_baseNavigationController
                                            ? _baseNavigationController
                                            : _navigationController)
                               browser:self.browser
                      parentFolderNode:parentNode];
  _folderEditorCoordinator.delegate = self;
  _mediator.UIDisabled = YES;
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
  _baseNavigationController = nil;
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

#pragma mark - BookmarksFolderEditorCoordinatorDelegate

- (void)bookmarksFolderEditorCoordinator:
            (BookmarksFolderEditorCoordinator*)folderEditor
              didFinishEditingFolderNode:
                  (const bookmarks::BookmarkNode*)folder {
  CHECK(folder, base::NotFatalUntil::M150);
  CHECK(_folderEditorCoordinator, base::NotFatalUntil::M150);
  [self stopBookmarksFolderEditorCoordinator];
  [_delegate bookmarksFolderChooserCoordinatorDidConfirm:self
                                      withSelectedFolder:folder];
}

- (void)bookmarksFolderEditorCoordinatorShouldStop:
    (BookmarksFolderEditorCoordinator*)coordinator {
  CHECK(_folderEditorCoordinator, base::NotFatalUntil::M150);
  [self stopBookmarksFolderEditorCoordinator];
  _mediator.UIDisabled = NO;
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksFolderChooserCoordinator () <
    BookmarksFolderChooserViewControllerPresentationDelegate,
    BookmarksFolderEditorCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate> {
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
  // The current nodes that are considered for a move.
  std::set<const bookmarks::BookmarkNode*> _editedNodes;
}

@end

@implementation BookmarksFolderChooserCoordinator

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
  }
  return self;
}

- (void)start {
  [super start];
  // TODO(crbug.com/1402758): Create a mediator.
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  _viewController = [[BookmarksFolderChooserViewController alloc]
      initWithBookmarkModel:model
           allowsNewFolders:YES
                editedNodes:_hiddenNodes
               allowsCancel:YES
             selectedFolder:_selectedFolder
                    browser:self.browser];
  _viewController.delegate = self;

  if (_baseNavigationController) {
    _viewController.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    [_baseNavigationController pushViewController:_viewController animated:YES];
  } else {
    _navigationController = [[BookmarkNavigationController alloc]
        initWithRootViewController:_viewController];
    _navigationController.presentationController.delegate = self;
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];

  DCHECK(_viewController);
  if (_baseNavigationController) {
    // Currently when folder editor is shown from folder chooser and the user
    // presses done button on the folder editor both the folder editor and
    // folder chooser is supposed to be popped at the same time. However the
    // folder chooser view controller also calls
    // delayedNotifyDelegateOfSelection in this case, so both the view
    // controllers need to be popped here at the end of that delayed
    // selection.
    // TODO(crbug.com/1405746): Revisit this logic after folder editor
    // coordinator is finished.
    if (_baseNavigationController.topViewController != _viewController) {
      [_baseNavigationController popToViewController:_viewController
                                            animated:YES];
    }
    DCHECK_EQ(_baseNavigationController.topViewController, _viewController);
    [_baseNavigationController popViewControllerAnimated:YES];
  } else if (_navigationController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController = nil;
  } else {
    // If there is no `_baseNavigationController` and `_navigationController`,
    // the view controller has been already dismissed. See
    // `presentationControllerDidDismiss:`.
    // Therefore `self.baseViewController.presentedViewController` must be
    // `nullptr`.
    DCHECK(!self.baseViewController.presentedViewController);
  }
  _viewController = nil;

  [_folderEditorCoordinator stop];
  _folderEditorCoordinator.delegate = nil;
  _folderEditorCoordinator = nil;
}

- (void)setSelectedFolder:(const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(folder->is_folder());
  _selectedFolder = folder;
  if (_viewController) {
    [_viewController changeSelectedFolder:_selectedFolder];
  }
}

- (void)changeSelectedFolder:(const bookmarks::BookmarkNode*)folder {
  [_viewController changeSelectedFolder:folder];
}

- (BOOL)canDismiss {
  if (_folderEditorCoordinator) {
    return [_folderEditorCoordinator canDismiss];
  }
  return YES;
}

#pragma mark - BookmarksFolderChooserViewControllerPresentationDelegate

- (void)showBookmarksFolderEditor {
  DCHECK(!_folderEditorCoordinator);
  _folderEditorCoordinator = [[BookmarksFolderEditorCoordinator alloc]
      initWithBaseNavigationController:(_baseNavigationController
                                            ? _baseNavigationController
                                            : _navigationController)
                               browser:self.browser
                      parentFolderNode:_selectedFolder];
  _folderEditorCoordinator.delegate = self;
  [_folderEditorCoordinator start];
}

- (void)bookmarksFolderChooserViewController:
            (BookmarksFolderChooserViewController*)viewController
                         didFinishWithFolder:
                             (const bookmarks::BookmarkNode*)folder {
  _editedNodes = _viewController.editedNodes;
  [_delegate bookmarksFolderChooserCoordinatorDidConfirm:self
                                      withSelectedFolder:folder];
}

- (void)bookmarksFolderChooserViewControllerDidCancel:
    (BookmarksFolderChooserViewController*)folderPicker {
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

#pragma mark - BookmarksFolderEditorCoordinatorDelegate

- (void)bookmarksFolderEditorCoordinator:
            (BookmarksFolderEditorCoordinator*)folderEditor
              didFinishEditingFolderNode:
                  (const bookmarks::BookmarkNode*)folder {
  DCHECK(folder);
  DCHECK(_folderEditorCoordinator);
  [_folderEditorCoordinator stop];
  _folderEditorCoordinator.delegate = nil;
  _folderEditorCoordinator = nil;

  [_viewController notifyFolderNodeAdded:folder];
}

- (void)bookmarksFolderEditorCoordinatorShouldStop:
    (BookmarksFolderEditorCoordinator*)coordinator {
  DCHECK(_folderEditorCoordinator);
  [_folderEditorCoordinator stop];
  _folderEditorCoordinator.delegate = nil;
  _folderEditorCoordinator = nil;
}

- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorCoordinator*)coordinator {
  // Do nothing.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  DCHECK(_navigationController);
  _navigationController = nil;
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [self canDismiss];
}

#pragma mark - Properties

- (const std::set<const bookmarks::BookmarkNode*>&)editedNodes {
  return _editedNodes;
}

@end

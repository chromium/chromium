// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksFolderChooserCoordinator () <
    BookmarksFolderChooserViewControllerDelegate> {
  // If folder chooser is created with a base view controller then folder
  // chooser will create and own `_navigationController` that should be deleted
  // in the end.
  // Otherwise, folder chooser is pushed into the `_baseNavigationController`
  // that it doesn't own.
  TableViewNavigationController* _navigationController;
  // Delegate for `_navigationController` if it was created inside folder
  // chooser and needs to be deleted with it.
  BookmarkNavigationControllerDelegate* _navigationControllerDelegate;
  BookmarksFolderChooserViewController* _folderChooserViewController;
  // List of nodes to hide when displaying folders. This is to avoid to move a
  // folder inside a child folder.
  std::set<const bookmarks::BookmarkNode*> _hiddenNodes;
}

@end

@implementation BookmarksFolderChooserCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithNavigationController:(UINavigationController*)navigationController
                         browser:(Browser*)browser
                     hiddenNodes:
                         (const std::set<const bookmarks::BookmarkNode*>&)
                             hiddenNodes {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _hiddenNodes = hiddenNodes;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  return [super initWithBaseViewController:viewController browser:browser];
}

- (void)start {
  [super start];
  // TODO(crbug.com/1402758): Create a mediator.
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  _folderChooserViewController = [[BookmarksFolderChooserViewController alloc]
      initWithBookmarkModel:model
           allowsNewFolders:YES
                editedNodes:_hiddenNodes
               allowsCancel:YES
             selectedFolder:nil
                    browser:self.browser];
  _folderChooserViewController.delegate = self;

  if (_baseNavigationController) {
    [_baseNavigationController pushViewController:_folderChooserViewController
                                         animated:YES];
  } else {
    _navigationController = [[TableViewNavigationController alloc]
        initWithTable:_folderChooserViewController];
    _navigationControllerDelegate =
        [[BookmarkNavigationControllerDelegate alloc] init];
    _navigationController.delegate = _navigationControllerDelegate;

    [_navigationController
        setModalPresentationStyle:UIModalPresentationFormSheet];
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  DCHECK(_folderChooserViewController);

  if (_baseNavigationController) {
    DCHECK_EQ(_baseNavigationController.topViewController,
              _folderChooserViewController);
    [_baseNavigationController popViewControllerAnimated:YES];
  } else if (_navigationController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController = nil;
    _navigationControllerDelegate = nil;
  } else {
    DCHECK(!self.baseViewController.presentedViewController);
  }
  _folderChooserViewController = nil;
}

#pragma mark - BookmarkFolderViewControllerDelegate

- (void)folderPicker:(BookmarksFolderChooserViewController*)folderPicker
    didFinishWithFolder:(const bookmarks::BookmarkNode*)folder {
  [_delegate
      bookmarksFolderChooserCoordinatorDidConfirm:self
                               withSelectedFolder:folder
                                      editedNodes:folderPicker.editedNodes];
}

- (void)folderPickerDidCancel:
    (BookmarksFolderChooserViewController*)folderPicker {
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

- (void)folderPickerDidDismiss:
    (BookmarksFolderChooserViewController*)folderPicker {
  DCHECK(_navigationController);
  _navigationController = nil;
  _navigationControllerDelegate = nil;
  [_delegate bookmarksFolderChooserCoordinatorDidCancel:self];
}

@end

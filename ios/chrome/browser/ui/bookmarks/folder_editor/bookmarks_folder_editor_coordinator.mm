// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarksFolderEditorCoordinator {
  // The navigation controller is nil if the folder chooser view controller is
  // pushed into the base navigation controller.
  // Otherwise, the navigation controller is presented in the base view
  // controller.
  UINavigationController* _navigationController;
  const bookmarks::BookmarkNode* _folderNode;
  UINavigationController* _viewController;
  UIViewController* _folderEditorViewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                folderNode:
                                    (const bookmarks::BookmarkNode*)folderNode {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _folderNode = folderNode;
  }
  return self;
}

- (void)start {
  [super start];
  // TODO(crbug.com/1402758): Create a view controller and a mediator.
  _folderEditorViewController = nil;
  if (self.baseNavigationController) {
    DCHECK(_folderNode);
    [self.baseNavigationController
        pushViewController:_folderEditorViewController
                  animated:YES];
  } else {
    DCHECK(!_navigationController);
    DCHECK(!_folderNode);
    _navigationController = [[UINavigationController alloc]
        initWithRootViewController:_folderEditorViewController];
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  DCHECK(_folderEditorViewController);
  if (self.baseNavigationController) {
    DCHECK_EQ(self.baseNavigationController.topViewController,
              _folderEditorViewController);
    [self.baseNavigationController popViewControllerAnimated:YES];
  } else {
    // Need to remove the created navigation controller.
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController = nil;
  }
  _folderEditorViewController = nil;
}

@end

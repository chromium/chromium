// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarksFolderChooserCoordinator {
  // The navigation controller is nil if the folder chooser view controller is
  // pushed into the base navigation controller.
  // Otherwise, the navigation controller is presented in the base view
  // controller.
  UINavigationController* _navigationController;
  UIViewController* _folderChooserViewController;
  // List of nodes to hide when displaying folders. This is to avoid to move a
  // folder inside a child folder.
  std::set<const bookmarks::BookmarkNode*> _hiddenNodes;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
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
  // TODO(crbug.com/1402758): Create a view controller and a mediator.
  _folderChooserViewController = nil;
  if (self.baseNavigationController) {
    [self.baseNavigationController
        pushViewController:_folderChooserViewController
                  animated:YES];
  } else {
    _navigationController = [[UINavigationController alloc]
        initWithRootViewController:_folderChooserViewController];
    [self.baseViewController presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  DCHECK(_folderChooserViewController);
  if (self.baseNavigationController) {
    DCHECK_EQ(self.baseNavigationController.topViewController,
              _folderChooserViewController);
    [self.baseNavigationController popViewControllerAnimated:YES];
  } else {
    DCHECK(_navigationController);
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
    _navigationController = nil;
  }
  _folderChooserViewController = nil;
}

@end

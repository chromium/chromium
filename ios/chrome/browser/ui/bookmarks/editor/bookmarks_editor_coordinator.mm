// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator.h"

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarksEditorCoordinator {
  // BookmarkNode to edit. If set, `_URL` is empty.
  const bookmarks::BookmarkNode* _node;
  // URL to edit. If set, `_node` is nullptr.
  GURL _URL;
  UIViewController* _viewController;
  UINavigationController* _navigationController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      node:
                                          (const bookmarks::BookmarkNode*)node {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _node = node;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(const GURL&)URL {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _URL = URL;
  }
  return self;
}

- (void)start {
  [super start];
  // TODO(crbug.com/1402758): Create a view controller and a mediator.
  _viewController = nil;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  DCHECK(_navigationController);
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _navigationController = nil;
}

@end

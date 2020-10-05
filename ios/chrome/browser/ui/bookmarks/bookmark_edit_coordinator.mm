// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_coordinator.h"

#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

@interface BookmarkEditCoordinator () <BookmarkEditViewControllerDelegate>

@property(nonatomic, assign) const BookmarkNode* bookmark;

@property(nonatomic, strong) BookmarkEditViewController* editViewController;

@property(nonatomic, strong) TableViewNavigationController* navController;

@end

@implementation BookmarkEditCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  bookmark:(const BookmarkNode*)bookmark {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    // This Coordinator only supports URL bookmarks for now.
    DCHECK(bookmark);
    DCHECK(bookmark->type() == BookmarkNode::URL);
    _bookmark = bookmark;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.editViewController =
      [[BookmarkEditViewController alloc] initWithBookmark:self.bookmark
                                                   browser:self.browser];
  self.editViewController.delegate = self;

  self.navController = [[TableViewNavigationController alloc]
      initWithTable:self.editViewController];
  self.navController.modalPresentationStyle = UIModalPresentationFormSheet;

  [self.baseViewController presentViewController:self.navController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  self.editViewController = nil;
  self.navController = nil;
}

#pragma mark - BookmarkEditViewControllerDelegate

- (BOOL)bookmarkEditor:(BookmarkEditViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const BookmarkNode*)bookmark {
  return YES;
}

- (void)bookmarkEditorWantsDismissal:(BookmarkEditViewController*)controller {
  [self.delegate bookmarkEditDismissed:self];
}

- (void)bookmarkEditorWillCommitTitleOrUrlChange:
    (BookmarkEditViewController*)controller {
  // No-op.
}

@end

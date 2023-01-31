// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator.h"

#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksEditorCoordinator () <
    BookmarksEditorViewControllerDelegate> {
  // BookmarkNode to edit.
  const bookmarks::BookmarkNode* _node;

  // The editor view controller owned and presented by this coordinator.
  // It is wrapped in a TableViewNavigationController.
  BookmarksEditorViewController* _viewController;

  // Receives commands to show a snackbar once a bookmark is edited or deleted.
  id<SnackbarCommands> _snackbarCommandsHandler;

  // The navigation controller that is being presented. The bookmark editor view
  // controller is the child of this navigation controller.
  UINavigationController* _navigationController;

  // The delegate provided to `_bookmarkNavigationController`.
  BookmarkNavigationControllerDelegate* _navigationControllerDelegate;
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
  _viewController =
      [[BookmarksEditorViewController alloc] initWithBookmark:_node
                                                      browser:self.browser];
  _viewController.delegate = self;
  _viewController.snackbarCommandsHandler = _snackbarCommandsHandler;
  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];

  _navigationController.toolbarHidden = YES;
  _navigationControllerDelegate =
      [[BookmarkNavigationControllerDelegate alloc] init];
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
  _viewController.delegate = nil;
  _viewController.snackbarCommandsHandler = nil;
  _viewController = nil;
  _snackbarCommandsHandler = nil;

  // animatedDismissal should have been explicitly set before calling stop.
  [_navigationController dismissViewControllerAnimated:self.animatedDismissal
                                            completion:nil];
  _navigationController = nil;
  _navigationControllerDelegate = nil;
}

#pragma mark - BookmarksEditorViewControllerDelegate

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
  [_viewController dismissBookmarkEditView];
}
@end

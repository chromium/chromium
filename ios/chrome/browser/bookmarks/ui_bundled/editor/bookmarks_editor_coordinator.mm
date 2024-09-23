// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_coordinator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_mediator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_mediator_delegate.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/editor/bookmarks_editor_view_controller.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_chooser/bookmarks_folder_chooser_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_editor/bookmarks_folder_editor_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface BookmarksEditorCoordinator () <
    BookmarksEditorViewControllerDelegate,
    BookmarksEditorMediatorDelegate,
    BookmarksFolderChooserCoordinatorDelegate> {
  // BookmarkNode to edit.
  raw_ptr<const bookmarks::BookmarkNode> _node;

  // The editor view controller owned and presented by this coordinator.
  // It is wrapped in a TableViewNavigationController.
  BookmarksEditorViewController* _viewController;

  // The editor mediator owned and presented by this coordinator.
  // It is wrapped in a TableViewNavigationController.
  BookmarksEditorMediator* _mediator;

  // Receives commands to show a snackbar once a bookmark is edited or deleted.
  id<SnackbarCommands> _snackbarCommandsHandler;

  // The navigation controller that is being presented. The bookmark editor view
  // controller is the child of this navigation controller.
  UINavigationController* _navigationController;

  // The folder chooser coordinator.
  BookmarksFolderChooserCoordinator* _folderChooserCoordinator;
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
  _viewController = [[BookmarksEditorViewController alloc]
      initWithName:bookmark_utils_ios::TitleForBookmarkNode(_node)
               URL:base::SysUTF8ToNSString(_node->url().spec())
        folderName:bookmark_utils_ios::TitleForBookmarkNode(_node->parent())];
  _viewController.delegate = self;
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);

  _mediator = [[BookmarksEditorMediator alloc]
      initWithBookmarkModel:bookmarkModel
               bookmarkNode:_node
                      prefs:profile->GetPrefs()
      authenticationService:AuthenticationServiceFactory::GetForProfile(profile)
                syncService:syncService
                    profile:profile];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  _mediator.snackbarCommandsHandler = _snackbarCommandsHandler;
  _viewController.mutator = _mediator;

  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.toolbarHidden = YES;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  DCHECK(_navigationController);
  [_mediator disconnect];
  [self dismissActionSheetCoordinator];
  _mediator.consumer = nil;
  _mediator.snackbarCommandsHandler = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController.mutator = nil;
  _viewController = nil;
  _snackbarCommandsHandler = nil;
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;

  // animatedDismissal should have been explicitly set before calling stop.
  [_navigationController dismissViewControllerAnimated:self.animatedDismissal
                                            completion:nil];
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
}

- (void)dealloc {
  DCHECK(!_navigationController);
}

- (BOOL)canDismiss {
  if (_viewController.edited) {
    return NO;
  }
  if (_folderChooserCoordinator && ![_folderChooserCoordinator canDismiss]) {
    return NO;
  }
  return YES;
}

#pragma mark - BookmarksEditorViewControllerDelegate

- (void)moveBookmark {
  DCHECK(!_folderChooserCoordinator);

  std::set<const bookmarks::BookmarkNode*> hiddenNodes{[_mediator bookmark]};
  _folderChooserCoordinator = [[BookmarksFolderChooserCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser
                           hiddenNodes:hiddenNodes];
  [_folderChooserCoordinator setSelectedFolder:_mediator.folder];
  _folderChooserCoordinator.delegate = self;
  [_folderChooserCoordinator start];
}

- (void)bookmarkEditorWantsDismissal:
    (BookmarksEditorViewController*)controller {
  [self.delegate bookmarksEditorCoordinatorShouldStop:self];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  if (!_viewController.canBeDismissed) {
    return;
  }

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
                  [weakSelf dismissActionSheetCoordinator];
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
                  [weakSelf dismissActionSheetCoordinator];
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
                  [weakSelf dismissActionSheetCoordinator];
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
  base::RecordAction(
      base::UserMetricsAction("IOSBookmarksEditorClosedWithSwipeDown"));
  [_viewController dismissBookmarkEditorView];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return [self canDismiss];
}

#pragma mark - BookmarksEditorMediatorDelegate

- (void)bookmarkEditorMediatorWantsDismissal:
    (BookmarksEditorMediator*)mediator {
  [self.delegate bookmarksEditorCoordinatorShouldStop:self];
}

- (void)bookmarkDidMoveToParent:(const bookmarks::BookmarkNode*)newParent {
  [_folderChooserCoordinator setSelectedFolder:newParent];
}

- (void)bookmarkEditorWillCommitTitleOrURLChange:
    (BookmarksEditorMediator*)mediator {
  [self.delegate bookmarkEditorWillCommitTitleOrURLChange:self];
}

#pragma mark - BookmarksFolderChooserCoordinatorDelegate

- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder {
  DCHECK(_folderChooserCoordinator);
  DCHECK(folder);
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;

  [_mediator manuallyChangeFolder:folder];
}

- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator {
  DCHECK(_folderChooserCoordinator);
  [_folderChooserCoordinator stop];
  _folderChooserCoordinator.delegate = nil;
  _folderChooserCoordinator = nil;
  if (!_navigationController.presentingViewController) {
    // In this case the `_navigationController` itself was dismissed.
    // TODO(crbug.com/40251259): Remove this if block when dismiss handling
    // is done in coordinators.
    [_viewController.view endEditing:YES];
    [self.delegate bookmarksEditorCoordinatorShouldStop:self];
  }
}

#pragma mark - Private

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

@end

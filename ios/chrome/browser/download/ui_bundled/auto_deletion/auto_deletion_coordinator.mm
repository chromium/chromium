// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/auto_deletion/auto_deletion_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/auto_deletion_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "ui/base/l10n/l10n_util_mac.h"

typedef void (^UIAlertActionHandler)(UIAlertAction* action);

@implementation AutoDeletionCoordinator {
  // The task that is downloading the web content onto the device.
  raw_ptr<web::DownloadTask> _downloadTask;
  // The coordinator that manages the Auto-deletion action sheet.
  ActionSheetCoordinator* _actionSheetCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)task {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _downloadTask = task;
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/390199773) Check if the user should be shown the
  // Auto-deletion feature Opt-in IPH and display it if true.

  _actionSheetCoordinator = [self createActionCoordinator];
  [_actionSheetCoordinator start];
}

- (void)stop {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nullptr;
}

#pragma mark - Private

// Creates the action sheet coordinator that manages the display of an action
// sheet when a user downloads content from the web onto the device. If the user
// clicks the primary action button, the downloaded file is scheduled for
// automatic deletion. Otherwise, the file will not be automatically deleted.
- (ActionSheetCoordinator*)createActionCoordinator {
  ActionSheetCoordinator* coordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_AUTO_DELETION_ACTION_SHEET_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_AUTO_DELETION_ACTION_SHEET_DESCRIPTION)
                            rect:self.baseViewController.view.bounds
                            view:self.baseViewController.view];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock primaryItemAction = ^{
    [weakSelf scheduleFileForDeletion];
  };
  [coordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_AUTO_DELETION_ACTION_SHEET_PRIMARY_ACTION)
                action:primaryItemAction
                 style:UIAlertActionStyleDestructive];
  ProceduralBlock cancelAction = ^{
    [weakSelf dismiss];
  };
  [coordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_AUTO_DELETION_ACTION_SHEET_CANCEL_ACTION)
                action:cancelAction
                 style:UIAlertActionStyleCancel];

  return coordinator;
}

// Schedules the downloaded file for automatic deletion when the user hits the
// action sheet's primary action button.
- (void)scheduleFileForDeletion {
  // TODO(crbug.com/390200553) Implement this function when the auto-deletion
  // models have been created and passes in `_downloadTask`.
}

// Creates a handler that conforms to the AutoDeletionCommands protocol and
// invokes its `dismissAutoDeletionActionSheet` function.
- (void)dismiss {
  id<AutoDeletionCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutoDeletionCommands);
  [handler dismissAutoDeletionActionSheet];
}

@end

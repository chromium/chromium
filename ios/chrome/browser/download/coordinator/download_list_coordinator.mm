// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/download_list_coordinator.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/coordinator/download_list_mediator.h"
#import "ios/chrome/browser/download/model/download_record_service_factory.h"
#import "ios/chrome/browser/download/ui/download_list_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/download_list_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@interface DownloadListCoordinator () {
  // Mediator for handling download list logic.
  DownloadListMediator* _mediator;
  // View controller for presenting Download List UI.
  DownloadListViewController* _downloadListViewController;
  // Navigation controller for presenting the download list.
  UINavigationController* _navigationController;
  // YES after start has been called.
  BOOL _started;
}
@end

@implementation DownloadListCoordinator

- (void)start {
  if (_started) {
    return;
  }
  [super start];

  _started = YES;

  ProfileIOS* profile = self.browser->GetProfile();
  DownloadRecordService* downloadRecordService =
      DownloadRecordServiceFactory::GetForProfile(profile);
  _mediator = [[DownloadListMediator alloc]
      initWithDownloadRecordService:downloadRecordService];
  [_mediator connect];

  [self setupAndPresentDownloadListUI];
}

- (void)stop {
  if (!_started) {
    return;
  }
  [super stop];
  _started = NO;

  // Dismiss any currently presented download list UI if we're dismissing it
  // programmatically.
  if (self.baseViewController.presentedViewController ==
      _navigationController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  _downloadListViewController = nil;
  _navigationController = nil;

  [_mediator disconnect];
  [_mediator setConsumer:nil];
  _mediator = nil;
}

#pragma mark - Private methods

// Creates, configures and presents the download list view controller.
- (void)setupAndPresentDownloadListUI {
  // Create view controller based on UI type.
  DownloadListUIType uiType = CurrentDownloadListUIType();
  DownloadListViewController* viewController = nil;
  switch (uiType) {
    case DownloadListUIType::kDefaultUI:
      viewController = [[DownloadListViewController alloc] init];
      break;
    case DownloadListUIType::kCustomUI:
      // Custom UI can be implemented here if needed.
      // For now, we will use the default implementation.
      viewController = [[DownloadListViewController alloc] init];
      break;
  }
  _downloadListViewController = viewController;
  [_mediator setConsumer:_downloadListViewController];
  _downloadListViewController.mutator = _mediator;

  id<DownloadListCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DownloadListCommands);
  _downloadListViewController.downloadListHandler = handler;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_downloadListViewController];
  _navigationController = navigationController;
  _navigationController.presentationController.delegate =
      _downloadListViewController;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

@end

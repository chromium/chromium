// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_coordinator.h"

#include "base/logging.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/send_tab_to_self_command.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_positioner.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_presentation_controller.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SendTabToSelfCoordinator () <UIViewControllerTransitioningDelegate,
                                        SendTabToSelfModalPositioner,
                                        SendTabToSelfModalDelegate>

// The TableViewController that shows the Send Tab To Self UI.
@property(nonatomic, strong)
    SendTabToSelfTableViewController* sendTabToSelfViewController;

@end

@implementation SendTabToSelfCoordinator

#pragma mark - ChromeCoordinator Methods

- (void)start {
  send_tab_to_self::SendTabToSelfSyncService* syncService =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(self.browserState);
  // This modal should not be launched in incognito mode where syncService is
  // undefined.
  DCHECK(syncService);

  self.sendTabToSelfViewController = [[SendTabToSelfTableViewController alloc]
      initWithModel:syncService->GetSendTabToSelfModel()
           delegate:self];
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.sendTabToSelfViewController];

  navigationController.transitioningDelegate = self;
  navigationController.modalPresentationStyle = UIModalPresentationCustom;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  DCHECK(self.baseViewController);
  if (self.baseViewController.presentedViewController &&
      self.baseViewController.presentedViewController ==
          self.sendTabToSelfViewController) {
    [self.sendTabToSelfViewController dismissViewControllerAnimated:NO
                                                         completion:nil];
  }
  self.sendTabToSelfViewController = nil;
}

#pragma mark-- UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  SendTabToSelfModalPresentationController* presentationController =
      [[SendTabToSelfModalPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  presentationController.modalPositioner = self;
  return presentationController;
}

#pragma mark - SendTabToSelfModalPositioner

- (CGFloat)modalHeight {
  UITableView* tableView = self.sendTabToSelfViewController.tableView;
  [tableView setNeedsLayout];
  [tableView layoutIfNeeded];

  // Since the TableView is contained in a NavigationController get the
  // navigation bar height.
  CGFloat navigationBarHeight =
      self.sendTabToSelfViewController.navigationController.navigationBar.frame
          .size.height;

  return tableView.contentSize.height + navigationBarHeight;
}

#pragma mark-- SendTabToSelfModalDelegate

- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(void (^)())completion {
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:completion];
  [self stop];
}

- (void)sendTabToTargetDeviceCacheGUID:(NSString*)cacheGUID
                      targetDeviceName:(NSString*)deviceName {
  // TODO(crbug.com/970284) log histogram of send event.
  SendTabToSelfCommand* command =
      [[SendTabToSelfCommand alloc] initWithTargetDeviceID:cacheGUID
                                          targetDeviceName:deviceName];

  [self.dispatcher sendTabToSelf:command];
  [self stop];
}

@end

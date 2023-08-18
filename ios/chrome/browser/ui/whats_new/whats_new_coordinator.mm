// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_controller.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"

namespace {

NSString* const kTableViewNavigationDismissButtonId =
    @"kWhatsNewTableViewNavigationDismissButtonId";

}  // namespace

@interface WhatsNewCoordinator () <UINavigationControllerDelegate,
                                   UIAdaptivePresentationControllerDelegate>

// The mediator to display What's New data.
@property(nonatomic, strong) WhatsNewMediator* mediator;
// The navigation controller displaying WhatsNewTableViewController.
@property(nonatomic, strong) UINavigationController* navigationController;
// The view controller used to display the What's New features and chrome tips.
@property(nonatomic, strong) WhatsNewTableViewController* tableViewController;
// The coordinator used for What's New feature.
@property(nonatomic, strong)
    WhatsNewDetailCoordinator* whatsNewDetailCoordinator;
// Browser coordinator command handler.
@property(nonatomic, readonly) id<ApplicationCommands> applicationHandler;
// The starting time of What's New.
@property(nonatomic, assign) base::TimeTicks whatsNewStartTime;
// Browser coordinator command handler.
@property(nonatomic, readonly) id<BrowserCoordinatorCommands> handler;

@end

@implementation WhatsNewCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  base::RecordAction(base::UserMetricsAction("WhatsNew.Started"));
  self.mediator = [[WhatsNewMediator alloc] init];
  self.mediator.urlLoadingAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  self.tableViewController = [[WhatsNewTableViewController alloc] init];
  self.tableViewController.navigationItem.rightBarButtonItem =
      [self dismissButton];

  self.tableViewController.delegate = self;
  self.tableViewController.actionHandler = self.mediator;
  self.mediator.consumer = self.tableViewController;
  self.mediator.handler = self.applicationHandler;

  [self.tableViewController reloadData];

  self.navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.tableViewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.navigationController.delegate = self;
  self.navigationController.presentationController.delegate = self;
  self.navigationController.navigationBar.prefersLargeTitles = YES;

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
  self.whatsNewStartTime = base::TimeTicks::Now();
  self.mediator.baseViewController = self.tableViewController;

  [super start];
}

- (void)stop {
  if (self.whatsNewDetailCoordinator) {
    [self.whatsNewDetailCoordinator stop];
    self.whatsNewDetailCoordinator = nil;
  }
  self.mediator = nil;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tableViewController = nil;
  self.navigationController = nil;

  base::RecordAction(base::UserMetricsAction("WhatsNew.Dismissed"));
  UmaHistogramMediumTimes("IOS.WhatsNew.TimeSpent",
                          base::TimeTicks::Now() - self.whatsNewStartTime);

  [self.promosUIHandler promoWasDismissed];

  if (self.shouldShowBubblePromoOnDismiss) {
    [self.handler showWhatsNewIPH];
  }

  [super stop];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  // No-op if the previous view controller is not the detail view.
  if (!self.whatsNewDetailCoordinator) {
    return;
  }

  // Stop the detail coordinator if the next view is the table view given that
  // the previous view was the detail view..
  if (viewController == self.tableViewController) {
    [self.whatsNewDetailCoordinator stop];
    self.whatsNewDetailCoordinator = nil;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismiss];
}

#pragma mark - WhatsNewTableViewDelegate

- (void)detailViewController:
            (WhatsNewTableViewController*)whatsNewTableviewController
    openDetailViewControllerForItem:(WhatsNewItem*)item {
  DCHECK_EQ(self.tableViewController, whatsNewTableviewController);

  self.whatsNewDetailCoordinator = [[WhatsNewDetailCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                                  item:item
                         actionHandler:self.mediator];
  [self.whatsNewDetailCoordinator start];
}

#pragma mark Private

- (id<ApplicationCommands>)applicationHandler {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  return handler;
}

- (UIBarButtonItem*)dismissButton {
  UIBarButtonItem* button = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismiss)];
  [button setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  return button;
}

- (void)dismiss {
  [self.handler dismissWhatsNew];
}

- (id<BrowserCoordinatorCommands>)handler {
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  DCHECK(handler);

  return handler;
}

@end

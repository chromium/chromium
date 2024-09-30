// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_controller.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

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
// The starting time of What's New.
@property(nonatomic, assign) base::TimeTicks whatsNewStartTime;
// Application command handler.
@property(nonatomic, readonly) id<ApplicationCommands> applicationHandler;
// Dispatcher for handling Lens promo actions.
@property(nonatomic, readonly) id<LensCommands> lensHandler;
// Whats New commands handler.
@property(nonatomic, readonly) id<WhatsNewCommands> whatsNewHandler;
// Settings command handler.
@property(nonatomic, readonly) id<SettingsCommands> settingsHandler;
// Number of clicked items in What's New
@property(nonatomic, assign) int clicksOnWhatsNewItemsCount;

@end

@implementation WhatsNewCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());
  DCHECK(tracker);
  tracker->NotifyEvent(feature_engagement::events::kViewedWhatsNew);

  self.clicksOnWhatsNewItemsCount = 0;
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
  self.mediator.applicationHandler = self.applicationHandler;
  self.mediator.whatsNewHandler = self.whatsNewHandler;
  self.mediator.lensHandler = self.lensHandler;
  self.mediator.settingsHandler = self.settingsHandler;

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

  [super start];
}

- (void)stop {
  [self.whatsNewDetailCoordinator stop];
  self.whatsNewDetailCoordinator = nil;

  self.mediator = nil;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tableViewController = nil;
  self.navigationController = nil;

  base::RecordAction(base::UserMetricsAction("WhatsNew.Dismissed"));
  UmaHistogramMediumTimes("IOS.WhatsNew.TimeSpent",
                          base::TimeTicks::Now() - self.whatsNewStartTime);
  base::UmaHistogramCounts10000("IOS.WhatsNew.ItemsClickedCount",
                                self.clicksOnWhatsNewItemsCount);

  [self.promosUIHandler promoWasDismissed];

  if (self.shouldShowBubblePromoOnDismiss) {
    [self.whatsNewHandler showWhatsNewIPH];
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

  ++self.clicksOnWhatsNewItemsCount;
  self.whatsNewDetailCoordinator = [[WhatsNewDetailCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                                  item:item
                         actionHandler:self.mediator
                       whatsNewHandler:self.whatsNewHandler];
  [self.whatsNewDetailCoordinator start];
}

#pragma mark Private

- (id<ApplicationCommands>)applicationHandler {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  DCHECK(handler);

  return handler;
}

- (id<WhatsNewCommands>)whatsNewHandler {
  id<WhatsNewCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), WhatsNewCommands);
  DCHECK(handler);

  return handler;
}

- (id<LensCommands>)lensHandler {
  id<LensCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  DCHECK(handler);

  return handler;
}

- (id<SettingsCommands>)settingsHandler {
  id<SettingsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  DCHECK(handler);

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
  [self.whatsNewHandler dismissWhatsNew];
}

@end

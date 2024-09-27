// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_coordinator.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/feature_list.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_mediator.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_metrics.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface DockingPromoCoordinator () <ConfirmationAlertActionHandler,
                                       UIAdaptivePresentationControllerDelegate>

// Main mediator for this coordinator.
@property(nonatomic, strong) DockingPromoMediator* mediator;

// Main view controller for this coordinator.
@property(nonatomic, strong) DockingPromoViewController* viewController;

@end

@implementation DockingPromoCoordinator {
  /// Whether the screen is being shown in the FRE.
  BOOL _firstRun;
  /// First run screen delegate.
  __weak id<FirstRunScreenDelegate> _firstRunDelegate;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _firstRun = NO;
  }
  return self;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  if ([self initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
    _firstRun = YES;
    _firstRunDelegate = delegate;
  }
  return self;
}

- (void)start {
  if (!_firstRun) {
    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(DockingPromoCommands)];
  }

  PromosManager* promosManager =
      PromosManagerFactory::GetForProfile(self.browser->GetProfile());

  AppState* appState = self.browser->GetSceneState().appState;

  std::optional<base::TimeDelta> timeSinceLastForeground =
      MinTimeSinceLastForeground(appState.foregroundScenes);

  self.mediator = [[DockingPromoMediator alloc]
        initWithPromosManager:promosManager
      timeSinceLastForeground:timeSinceLastForeground.value_or(
                                  base::TimeDelta::Min())];

  if (_firstRun) {
    self.viewController = [[DockingPromoViewController alloc] init];
    self.mediator.consumer = self.viewController;
    self.mediator.tracker = feature_engagement::TrackerFactory::GetForProfile(
        self.browser->GetProfile());
    self.viewController.actionHandler = self;
    self.viewController.presentationController.delegate = self;
    self.viewController.modalInPresentation = YES;

    [self.mediator configureConsumer];

    BOOL animated = self.baseNavigationController.topViewController != nil;
    [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                             animated:animated];
  }
}

- (void)stop {
  [super stop];

  if (!_firstRun) {
    [self.browser->GetCommandDispatcher()
        stopDispatchingForProtocol:@protocol(DockingPromoCommands)];

    [self.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  self.mediator = nil;
  self.viewController = nil;
  _baseNavigationController = nil;
  _firstRunDelegate = nil;
}

#pragma mark - DockingPromoCommands

- (void)showDockingPromo:(BOOL)forced {
  if ((!forced && ![self.mediator canShowDockingPromo]) ||
      [self.viewController isBeingPresented]) {
    return;
  }

  self.viewController = [[DockingPromoViewController alloc] init];
  self.mediator.consumer = self.viewController;
  self.mediator.tracker = feature_engagement::TrackerFactory::GetForProfile(
      self.browser->GetProfile());
  self.viewController.actionHandler = self;
  self.viewController.presentationController.delegate = self;

  [self.mediator configureConsumer];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if (_firstRun) {
    [_firstRunDelegate screenWillFinishPresenting];
  } else {
    [self hidePromo];
  }

  [self promoWasDismissed];
  RecordDockingPromoAction(IOSDockingPromoAction::kGotIt);
}

- (void)confirmationAlertSecondaryAction {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());
  tracker->NotifyEvent(feature_engagement::events::kDockingPromoRemindMeLater);

  if (_firstRun) {
    [_firstRunDelegate screenWillFinishPresenting];
  } else {
    [self hidePromo];
  }

  [self.mediator registerPromoWithPromosManager];
  [self promoWasDismissed];
  RecordDockingPromoAction(IOSDockingPromoAction::kRemindMeLater);
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self promoWasDismissed];
  RecordDockingPromoAction(IOSDockingPromoAction::kDismissViaSwipe);
}

#pragma mark - Private

// Dismisses the feature.
- (void)hidePromo {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

// Does any clean up for when the promo is fully dismissed.
- (void)promoWasDismissed {
  [self.promosUIHandler promoWasDismissed];
}

@end

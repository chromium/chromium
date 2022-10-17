// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/tos_commands.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/first_run/uma/uma_coordinator.h"
#import "ios/chrome/browser/ui/first_run/welcome/tos_coordinator.h"
#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/welcome/welcome_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WelcomeScreenCoordinator () <UMACoordinatorDelegate,
                                        WelcomeScreenViewControllerDelegate>

@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

// Welcome screen view controller.
@property(nonatomic, strong) WelcomeScreenViewController* viewController;

// Welcome screen mediator.
@property(nonatomic, strong) WelcomeScreenMediator* mediator;

// Whether the user tapped on the TOS link.
@property(nonatomic, assign) BOOL TOSLinkWasTapped;

// Whether the user tapped on the UMA link.
@property(nonatomic, assign) BOOL UMALinkWasTapped;

// Coordinator used to manage the TOS page.
@property(nonatomic, strong) TOSCoordinator* TOSCoordinator;

// Coordinator used to manage the UMA page.
@property(nonatomic, strong) UMACoordinator* UMACoordinator;

@end

@implementation WelcomeScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TOSCommands)];
  id<TOSCommands> TOSHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);

  self.viewController =
      [[WelcomeScreenViewController alloc] initWithTOSHandler:TOSHandler];
  self.viewController.delegate = self;
  self.mediator = [[WelcomeScreenMediator alloc] init];
  self.mediator.consumer = self.viewController;

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  self.viewController.modalInPresentation = YES;
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - WelcomeScreenViewControllerDelegate

- (BOOL)isCheckboxSelectedByDefault {
  return [self.mediator isCheckboxSelectedByDefault];
}

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1189815): Remember that the welcome screen has been shown in
  // NSUserDefaults.
  [self.mediator acceptToS];
  [self.mediator
      setMetricsReportingEnabled:self.mediator.UMAReportingUserChoice];
  if (self.TOSLinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreTOSLinkTapped"));
  }
  if (self.UMALinkWasTapped) {
    base::RecordAction(base::UserMetricsAction("MobileFreUMALinkTapped"));
  }

  [self.delegate screenWillFinishPresenting];
}

#pragma mark - TOSCommands

- (void)showTOSPage {
  self.TOSLinkWasTapped = YES;
  self.TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:self.viewController
                                                 browser:self.browser];
  [self.TOSCoordinator start];
}

- (void)closeTOSPage {
  [self.TOSCoordinator stop];
  self.TOSCoordinator = nil;
}

#pragma mark - UMACoordinatorDelegate

- (void)UMACoordinatorDidRemoveWithCoordinator:(UMACoordinator*)coordinator
                        UMAReportingUserChoice:(BOOL)UMAReportingUserChoice {
  DCHECK(self.UMACoordinator);
  DCHECK_EQ(self.UMACoordinator, coordinator);
  self.UMACoordinator = nil;
  DCHECK(self.mediator);
  self.mediator.UMAReportingUserChoice = UMAReportingUserChoice;
}

#pragma mark - WelcomeScreenViewControllerDelegate

- (void)showUMADialog {
  DCHECK(!self.UMACoordinator);
  self.UMALinkWasTapped = YES;
  self.UMACoordinator = [[UMACoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
               UMAReportingValue:self.mediator.UMAReportingUserChoice];
  self.UMACoordinator.delegate = self;
  [self.UMACoordinator start];
}

- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible
        withUMACheckboxVisible:(BOOL)umaCheckboxVisible {
  first_run::FirstRunScreenType screenType =
      umaCheckboxVisible
          ? first_run::FirstRunScreenType::kWelcomeScreenWithUMACheckbox
          : first_run::FirstRunScreenType::kWelcomeScreenWithoutUMACheckbox;
  RecordFirstRunScrollButtonVisibilityMetrics(screenType, scrollButtonVisible);
}

@end

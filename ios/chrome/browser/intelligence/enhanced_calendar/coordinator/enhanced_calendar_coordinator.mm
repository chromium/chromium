// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_coordinator.h"

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/ui/enhanced_calendar_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/enhanced_calendar_commands.h"
#import "ios/public/provider/chrome/browser/add_to_calendar/add_to_calendar_api.h"

@interface EnhancedCalendarCoordinator () <EnhancedCalendarMediatorDelegate>
@end

@implementation EnhancedCalendarCoordinator {
  // The config object holding everything needed to complete an Enhanced
  // Calendar request and start the UI flow.
  EnhancedCalendarConfiguration* _enhancedCalendarConfig;

  // The mediator for handling the Enhanced Calendar service and its model
  // request(s).
  EnhancedCalendarMediator* _mediator;

  // The view controller which represents the Enhanced Calendar interstitial
  // (bottom sheet).
  EnhancedCalendarViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    enhancedCalendarConfig:
                        (EnhancedCalendarConfiguration*)enhancedCalendarConfig {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _enhancedCalendarConfig = enhancedCalendarConfig;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[EnhancedCalendarViewController alloc] init];

  _mediator = [[EnhancedCalendarMediator alloc]
            initWithWebState:self.browser->GetWebStateList()
                                 ->GetActiveWebState()
      enhancedCalendarConfig:_enhancedCalendarConfig];

  _mediator.delegate = self;
  _viewController.mutator = _mediator;

  [_mediator startEnhancedCalendarRequest];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];

  _mediator = nil;
  _viewController = nil;
  _enhancedCalendarConfig = nil;
}

#pragma mark EnhancedCalendarMediatorDelegate

- (void)presentAddToCalendar:(EnhancedCalendarMediator*)mediator
                      config:(EnhancedCalendarConfiguration*)config {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  ios::provider::PresentAddToCalendar(
      self.baseViewController,
      self.browser->GetWebStateList()->GetActiveWebState(),
      _enhancedCalendarConfig);

  [self dismissEnhancedCalendarFeature];
}

- (void)cancelRequestsAndDismissViewController:
    (EnhancedCalendarMediator*)mediator {
  [self dismissEnhancedCalendarFeature];
}

#pragma mark - Private

- (void)dismissEnhancedCalendarFeature {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  __weak id<EnhancedCalendarCommands> enhancedCalendarHandler =
      HandlerForProtocol(dispatcher, EnhancedCalendarCommands);

  // Clean up the feature and stop/nil the coordinator.
  [enhancedCalendarHandler hideEnhancedCalendarBottomSheet];
}

@end

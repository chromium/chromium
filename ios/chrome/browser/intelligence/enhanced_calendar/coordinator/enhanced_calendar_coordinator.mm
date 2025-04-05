// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_coordinator.h"

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/ui/enhanced_calendar_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/public/provider/chrome/browser/add_to_calendar/add_to_calendar_api.h"

@implementation EnhancedCalendarCoordinator {
  // The integration provider for the "add to calendar" experience.
  ios::provider::AddToCalendarIntegrationProvider _integrationProvider;

  // The mediator for handling the Enhanced Calendar service and its model
  // request(s).
  EnhancedCalendarMediator* _mediator;

  // The view controller presented as the Enhanced Calendar interstitial.
  EnhancedCalendarViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       integrationProvider:
                           (ios::provider::AddToCalendarIntegrationProvider)
                               integrationProvider {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _integrationProvider = integrationProvider;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[EnhancedCalendarViewController alloc] init];

  _mediator = [[EnhancedCalendarMediator alloc]
         initWithWebState:self.browser->GetWebStateList()->GetActiveWebState()
      integrationProvider:_integrationProvider];

  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];

  _mediator = nil;
  _viewController = nil;
}

@end

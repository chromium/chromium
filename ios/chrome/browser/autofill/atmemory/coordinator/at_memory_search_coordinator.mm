// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_coordinator.h"

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/model/ios_at_memory_query_service_factory.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/web/public/web_state.h"

@implementation AtMemorySearchCoordinator {
  // Search view controller.
  AtMemorySearchViewController* _atMemorySearchViewController;
  // Mediator for the AtMemory search coordinator.
  AtMemorySearchMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _atMemorySearchViewController = [[AtMemorySearchViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _atMemorySearchViewController.searchResultHandler = self.searchResultHandler;

  autofill::AtMemoryQueryService* atMemoryQueryService =
      IOSAtMemoryQueryServiceFactory::GetForProfile(self.browser->GetProfile());
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  _mediator = [[AtMemorySearchMediator alloc]
      initWithAtMemoryQueryService:atMemoryQueryService
                          webState:webState];
  _mediator.fillHandler = self.fillHandler;

  [self.baseNavigationController
      pushViewController:_atMemorySearchViewController
                animated:YES];
}

- (void)stop {
  if (self.baseNavigationController.topViewController ==
      _atMemorySearchViewController) {
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
  _atMemorySearchViewController = nil;
  _mediator = nil;
}

@end

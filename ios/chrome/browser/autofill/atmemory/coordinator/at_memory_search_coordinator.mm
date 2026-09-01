// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_coordinator.h"

#import "base/check.h"
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/personal_context/first_run/personal_context_first_run_service.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_first_run_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
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
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  CHECK(webState);

  autofill::AutofillClientIOS* autofillClient =
      autofill::AutofillClientIOS::FromWebState(webState);
  CHECK(autofillClient);

  autofill::AtMemoryManager* atMemoryManager =
      autofillClient->GetAtMemoryManager();
  CHECK(atMemoryManager);

  autofill::BrowserAutofillManager* autofillManager =
      static_cast<autofill::BrowserAutofillManager*>(
          autofillClient->GetAutofillManagerForPrimaryMainFrame());
  CHECK(autofillManager);

  _atMemorySearchViewController = [[AtMemorySearchViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _atMemorySearchViewController.searchResultHandler = self.searchResultHandler;
  _atMemorySearchViewController.atMemoryHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  _atMemorySearchViewController.geminiHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), GeminiCommands);

  personal_context::PersonalContextFirstRunService* firstRunService =
      IOSPersonalContextFirstRunServiceFactory::GetForProfile(
          self.browser->GetProfile());
  _mediator =
      [[AtMemorySearchMediator alloc] initWithAtMemoryManager:atMemoryManager
                                              autofillManager:autofillManager
                                                     webState:webState
                                              firstRunService:firstRunService];
  _mediator.fillHandler = self.fillHandler;
  _mediator.searchResultHandler = self.searchResultHandler;
  _mediator.atMemoryHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  _mediator.consumer = _atMemorySearchViewController;
  _atMemorySearchViewController.mutator = _mediator;

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
  [_mediator disconnect];
  _mediator = nil;
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_coordinator.h"

#import "base/check.h"
#import "components/autofill/core/browser/at_memory/at_memory_manager.h"
#import "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_coordinator.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"
#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_search_coordinator.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_result_commands.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_content_injector.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

using autofill::AtMemoryManager;
using autofill::AutofillClientIOS;
using autofill::BrowserAutofillManager;
using autofill::FieldGlobalId;

@interface AtMemoryCoordinator () <AtMemorySearchResultCommands,
                                   UIAdaptivePresentationControllerDelegate>
@end

@implementation AtMemoryCoordinator {
  // NavigationController for the AtMemory flow.
  UINavigationController* _navigationController;
  // Injector for manual fill data.
  id<ManualFillContentInjector> _contentInjector;
  // Coordinator for AtMemory search.
  AtMemorySearchCoordinator* _atMemorySearchCoordinator;
  // Coordinator for AtMemory granular fill.
  AtMemoryGranularFillCoordinator* _atMemoryGranularFillCoordinator;
  // Mediator for AtMemory filling.
  AtMemoryMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           contentInjector:
                               (id<ManualFillContentInjector>)contentInjector {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _contentInjector = contentInjector;
  }
  return self;
}

- (void)start {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  CHECK(webState);

  AutofillClientIOS* autofillClient = AutofillClientIOS::FromWebState(webState);
  CHECK(autofillClient);

  AtMemoryManager* atMemoryManager = autofillClient->GetAtMemoryManager();
  CHECK(atMemoryManager);

  BrowserAutofillManager* autofillManager =
      static_cast<BrowserAutofillManager*>(
          autofillClient->GetAutofillManagerForPrimaryMainFrame());
  CHECK(autofillManager);

  // TODO(crbug.com/555810315): An empty `FieldGlobalId` is temporarily passed
  // here until the initiating field ID is propagated to the coordinator.
  _mediator =
      [[AtMemoryMediator alloc] initWithAtMemoryManager:atMemoryManager
                                        autofillManager:autofillManager
                                        contentInjector:_contentInjector
                                                fieldId:FieldGlobalId()];
  _mediator.atMemoryHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);

  _navigationController = [[UINavigationController alloc] init];
  _navigationController.presentationController.delegate = self;

  _atMemorySearchCoordinator = [[AtMemorySearchCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  _atMemorySearchCoordinator.searchResultHandler = self;
  _atMemorySearchCoordinator.fillHandler = _mediator;
  [_atMemorySearchCoordinator start];

  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* sheet =
      _navigationController.sheetPresentationController;
  if (sheet) {
    sheet.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
    sheet.prefersGrabberVisible = YES;
    sheet.prefersScrollingExpandsWhenScrolledToEdge = YES;
    sheet.prefersEdgeAttachedInCompactHeight = YES;
  }

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_atMemoryGranularFillCoordinator stop];
  _atMemoryGranularFillCoordinator = nil;

  [_atMemorySearchCoordinator stop];
  _atMemorySearchCoordinator = nil;

  [_mediator disconnect];
  _mediator = nil;

  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
}

#pragma mark - AtMemorySearchResultCommands

- (void)showAtMemoryGranularFill:(const autofill::Suggestion&)suggestion {
  [_atMemoryGranularFillCoordinator stop];

  _atMemoryGranularFillCoordinator = [[AtMemoryGranularFillCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser
                            suggestion:suggestion];
  _atMemoryGranularFillCoordinator.fillHandler = _mediator;
  [_atMemoryGranularFillCoordinator start];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<AtMemoryCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AtMemoryCommands);
  [handler dismissAtMemory];
}

@end

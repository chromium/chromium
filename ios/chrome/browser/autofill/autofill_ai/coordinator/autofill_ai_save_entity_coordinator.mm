// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mediator.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface AutofillAISaveEntityCoordinator () <
    UIAdaptivePresentationControllerDelegate>
@end

@implementation AutofillAISaveEntityCoordinator {
  // Autofill commands handler.
  __weak id<AutofillCommands> _autofillHandler;

  // Navigation controller that owns the save entity view controller.
  TableViewNavigationController* _navigationController;

  // Mediator that handles the business logic of the save entity UI.
  AutofillAISaveEntityMediator* _mediator;

  // New and old entity, and the callback to notify Autofill AI.
  std::optional<autofill::SaveEntityParams> _params;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(autofill::SaveEntityParams)params {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _params = std::move(params);
  }
  return self;
}

- (void)start {
  CHECK(_params.has_value());
  _mediator =
      [[AutofillAISaveEntityMediator alloc] initWithParams:std::move(*_params)];
  _params.reset();

  _autofillHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                        AutofillCommands);
  CHECK(_autofillHandler);

  AutofillAISaveEntityTableViewController* saveViewController =
      [[AutofillAISaveEntityTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  saveViewController.mutator = _mediator;
  saveViewController.autofillHandler = _autofillHandler;
  _mediator.consumer = saveViewController;

  _navigationController =
      [[TableViewNavigationController alloc] initWithTable:saveViewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  _navigationController.presentationController.delegate = self;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_mediator dismissSaving];
  [_autofillHandler dismissSaveEntityDialog];
}

@end

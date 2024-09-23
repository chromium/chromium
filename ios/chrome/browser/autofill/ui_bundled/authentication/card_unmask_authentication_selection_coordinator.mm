// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_coordinator.h"

#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_view_controller.h"

@interface CardUnmaskAuthenticationSelectionCoordinator () <
    CardUnmaskAuthenticationSelectionMediatorDelegate>
@end

@implementation CardUnmaskAuthenticationSelectionCoordinator {
  // A reference to the base view controller with UINavigationController type.
  __weak UINavigationController* _baseNavigationController;

  // The authentication selection view controlling displaying challenge options.
  __weak CardUnmaskAuthenticationSelectionViewController*
      _selectionViewController;

  // The controller providing the UI assets (titles, messages, authentication
  // options, etc.). In the Coordinator-Mediator-ViewController pattern this
  // controller is in the model.
  std::unique_ptr<
      autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
      _modelController;

  std::unique_ptr<CardUnmaskAuthenticationSelectionMediator> _mediator;

  id<BrowserCoordinatorCommands> _browserCoordinatorCommands;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)baseViewController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _baseNavigationController = baseViewController;
    AutofillBottomSheetTabHelper* tabHelper =
        AutofillBottomSheetTabHelper::FromWebState(
            browser->GetWebStateList()->GetActiveWebState());
    _modelController =
        tabHelper->GetCardUnmaskAuthenticationSelectionDialogController();
    _browserCoordinatorCommands = HandlerForProtocol(
        browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    CHECK(_modelController);
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/40282545) Connect the view controllers Mutator, an
  // Objective-C protocol to the Mediator (a C++ class).
  CardUnmaskAuthenticationSelectionViewController* selectionViewController =
      [[CardUnmaskAuthenticationSelectionViewController alloc] init];
  _mediator = std::make_unique<CardUnmaskAuthenticationSelectionMediator>(
      _modelController->GetWeakPtr(),
      /*consumer=*/selectionViewController);
  _mediator->set_delegate(self);
  selectionViewController.mutator = _mediator->AsMutator();
  _selectionViewController = selectionViewController;

  [_baseNavigationController pushViewController:_selectionViewController
                                       animated:NO];
}

- (void)stop {
  [_baseNavigationController popViewControllerAnimated:YES];
  _selectionViewController.mutator = nil;
}

#pragma mark - CardUnmaskAuthenticationSelectionMediatorDelegate

- (void)dismissAuthenticationSelection {
  [_browserCoordinatorCommands dismissCardUnmaskAuthentication];
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_selection_coordinator.h"

#import "components/autofill/core/browser/ui/payments/card_unmask_authentication_selection_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_selection_mediator.h"

@implementation CardUnmaskAuthenticationSelectionCoordinator {
  // A reference to the base view controller with UINavigationController type.
  __weak UINavigationController* _baseNavigationController;

  // TODO(crbug.com/40282545) Implement the authentication selection view and
  // update the type here.
  __weak UIViewController* _selectionViewController;

  // The controller providing the UI assets (titles, messages, authentication
  // options, etc.). In the Coordinator-Mediator-ViewController pattern this
  // controller is in the model.
  std::unique_ptr<
      autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
      _modelController;

  std::unique_ptr<CardUnmaskAuthenticationSelectionMediator> _mediator;
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
    CHECK(_modelController);
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/40282545): Remove placeholder view and implement card
  // unmask authentication.
  UIViewController* selectionViewController = [[UIViewController alloc] init];
  _mediator = std::make_unique<CardUnmaskAuthenticationSelectionMediator>(
      _modelController->GetWeakPtr(),
      /*consumer=*/nil);
  selectionViewController.view.backgroundColor = [UIColor redColor];
  selectionViewController.title = @"TODO";
  _selectionViewController = selectionViewController;

  [_baseNavigationController pushViewController:_selectionViewController
                                       animated:NO];
}

- (void)stop {
  [_baseNavigationController popViewControllerAnimated:YES];
}

@end

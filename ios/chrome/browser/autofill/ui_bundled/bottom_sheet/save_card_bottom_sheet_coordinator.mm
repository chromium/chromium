// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_coordinator.h"

#import <memory>
#import <utility>

#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_edit_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"

@interface SaveCardBottomSheetCoordinator () <SaveCardBottomSheetDelegate>
@end

@implementation SaveCardBottomSheetCoordinator {
  // The model providing resources and callbacks for save card bottomsheet.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;

  // The mediator for save card bottomsheet created and owned by the
  // coordinator.
  SaveCardBottomSheetMediator* _mediator;

  // The view controller to display save card bottomsheet.
  SaveCardBottomSheetViewController* _saveCardViewController;

  // The view controller for the Scan and Save flow
  PaymentsScanSaveAndFillEditViewController* _scannedCardEditViewController;

  // The coordinator for the credit card scanner
  CreditCardScannerCoordinator* _creditCardScannerCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    AutofillBottomSheetTabHelper* tabHelper =
        AutofillBottomSheetTabHelper::FromWebState(
            browser->GetWebStateList()->GetActiveWebState());
    _saveCardBottomSheetModel = tabHelper->GetSaveCardBottomSheetModel();
    CHECK(_saveCardBottomSheetModel);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[SaveCardBottomSheetMediator alloc]
              initWithUIModel:std::move(_saveCardBottomSheetModel)
      autofillCommandsHandler:HandlerForProtocol(
                                  self.browser->GetCommandDispatcher(),
                                  AutofillCommands)];
  SaveCardActionType actionType = [_mediator actionType];

  if (actionType == SaveCardActionType::kUpload ||
      actionType == SaveCardActionType::kLocal) {
    _saveCardViewController = [[SaveCardBottomSheetViewController alloc] init];
    _saveCardViewController.mutator = _mediator;
    _saveCardViewController.dataSource = _mediator;
    _saveCardViewController.delegate = self;
    _mediator.consumer = _saveCardViewController;
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController presentViewController:_saveCardViewController
                                          animated:YES
                                        completion:^{
                                          [weakSelf setInitialVoiceOverFocus];
                                        }];
  } else {
    _scannedCardEditViewController =
        [[PaymentsScanSaveAndFillEditViewController alloc] init];

    _scannedCardEditViewController.dataSource = _mediator;
    _scannedCardEditViewController.mutator = _mediator;
    _scannedCardEditViewController.delegate = self;

    _mediator.consumer = _scannedCardEditViewController;

    // Wrap the view controller in a UINavigationController.
    // `PaymentsScanSaveAndFillEditViewController` uses this to display a title
    // and cancel button.
    UINavigationController* navigationController =
        [[UINavigationController alloc]
            initWithRootViewController:_scannedCardEditViewController];

    navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

    __weak __typeof(self) weakSelf = self;
    // The ScannedCardBottomSheetViewController must already be in the view
    // hierarchy to serve as the baseViewController for the
    // CreditCardScannerCoordinator. Therefore, we present the bottom sheet
    // first and immediately start the scanner in the completion block to launch
    // the camera UI over it. Once the scan finishes and the camera dismisses,
    // the recognized data is fed back to the view controller, leaving the user
    // with a pre-populated, editable form.
    [self.baseViewController presentViewController:navigationController
                                          animated:YES
                                        completion:^{
                                          [weakSelf setInitialVoiceOverFocus];
                                          [weakSelf startCreditCardScanner];
                                        }];
  }
}

- (void)stop {
  if (_saveCardViewController) {
    [_saveCardViewController dismissViewControllerAnimated:YES completion:nil];
    _saveCardViewController = nil;
  }

  if (_creditCardScannerCoordinator) {
    [_creditCardScannerCoordinator stop];
    _creditCardScannerCoordinator = nil;
  }

  if (_scannedCardEditViewController) {
    [_scannedCardEditViewController dismissViewControllerAnimated:YES
                                                       completion:nil];
    _scannedCardEditViewController = nil;
  }

  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
}

#pragma mark - SaveCardBottomSheetDelegate

- (void)didTapLinkURL:(CrURL*)URL {
  [_mediator onBottomSheetDismissedWithLinkClicked:YES];
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:URL.gurl]];
}

- (void)onViewDisappeared {
  [_mediator onBottomSheetDismissedWithLinkClicked:NO];
  id<AutofillCommands> autofillHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutofillCommands);
  [autofillHandler dismissSaveCardBottomSheet];
}

#pragma mark - Private

- (void)setInitialVoiceOverFocus {
  if (_saveCardViewController) {
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    _saveCardViewController.titleLabel);
  } else {
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    _scannedCardEditViewController.view);
  }
}

// Helper to start the credit card scanner
- (void)startCreditCardScanner {
  _creditCardScannerCoordinator = [[CreditCardScannerCoordinator alloc]
      initWithBaseViewController:_scannedCardEditViewController
                         browser:self.browser
                        consumer:_scannedCardEditViewController];

  [_creditCardScannerCoordinator start];
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_coordinator.h"

#import "base/check.h"
#import "base/ios/block_types.h"
#import "components/autofill/core/browser/form_import/form_data_importer.h"
#import "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

@interface PaymentsScanSaveAndFillOfferBottomSheetCoordinator () <
    PaymentsScanSaveAndFillOfferBottomSheetDelegate,
    WebStateListObserving>

// Handler for Autofill Commands.
@property(nonatomic, readonly) id<AutofillCommands> autofillHandler;

@end

@implementation PaymentsScanSaveAndFillOfferBottomSheetCoordinator {
  // The view controller for the bottom sheet.
  PaymentsScanSaveAndFillOfferBottomSheetViewController* _viewController;

  // The mediator for the bottom sheet.
  PaymentsScanSaveAndFillOfferBottomSheetMediator* _mediator;

  // The parameters of the form that triggered the bottom sheet.
  std::optional<autofill::FormActivityParams> _params;

  // Bridge to observe the WebStateList.
  std::optional<WebStateListObserverBridge> _webStateListObserverBridge;
  std::optional<base::ScopedObservation<WebStateList, WebStateListObserver>>
      _observation;

  // Whether the exit reason has been logged.
  BOOL _exitReasonLogged;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                    params:
                                        (autofill::FormActivityParams)params {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _params = std::move(params);
  }
  return self;
}

- (void)start {
  [super start];

  _webStateListObserverBridge.emplace(self);
  _observation.emplace(&_webStateListObserverBridge.value());
  _observation->Observe(self.browser->GetWebStateList());

  _viewController =
      [[PaymentsScanSaveAndFillOfferBottomSheetViewController alloc] init];
  _viewController.delegate = self;
  if (_params.has_value()) {
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    _mediator = [[PaymentsScanSaveAndFillOfferBottomSheetMediator alloc]
        initWithParams:std::move(*_params)
              webState:webState];

    FormSuggestionTabHelper* tabHelper =
        FormSuggestionTabHelper::FromWebState(webState);
    if (tabHelper) {
      id<FormInputSuggestionsProvider> provider =
          tabHelper->GetAccessoryViewProvider();
      CHECK(provider);
      [_mediator setProvider:provider];
    }
    _params.reset();
  }

  _mediator.consumer = _viewController;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  // Dismiss right away if the presentation failed to avoid having a zombie
  // coordinator.
  if (!_viewController.presentingViewController) {
    [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::
                                    kCouldNotPresent];
    [self.autofillHandler dismissScanCardSaveAndFillBottomSheet];
  }
}

- (void)stop {
  _observation.reset();
  _webStateListObserverBridge.reset();
  [super stop];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // If the active WebState changes, the bottom sheet is no longer relevant.
  // Invalidate the provider associated with the old WebState and dismiss
  // the bottom sheet.
  if (status.active_web_state_change()) {
    [_mediator setProvider:nil];
    [self.autofillHandler dismissScanCardSaveAndFillBottomSheet];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  [self stop];
}

#pragma mark - PaymentsScanSaveAndFillOfferBottomSheetDelegate

- (void)paymentsBottomSheetViewDidAppear {
  [_mediator scanCardBottomSheetViewDidAppear];
}

- (void)paymentsBottomSheetDidDisappear {
  [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::kIgnore];
  [_mediator refocus];
  [_mediator disconnect];
  [self.autofillHandler dismissScanCardSaveAndFillBottomSheet];
}

- (void)didTapScanCardButton {
  [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::
                                  kAcceptSuggestion];
  // Disable user interactions on the root view of the view controller so any
  // further user action isn't allowed. Only one action is allowed on the sheet.
  _viewController.view.userInteractionEnabled = NO;

  _viewController.delegate = nil;

  [_mediator didAcceptScanCardSuggestion];
  ProceduralBlock postDismissBlock = [_mediator postDismissBlock];
  [_mediator disconnect];
  _mediator = nil;

  __weak __typeof(self) weakSelf = self;
  [_viewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           if (postDismissBlock) {
                             postDismissBlock();
                           }
                           [weakSelf.autofillHandler
                                   dismissScanCardSaveAndFillBottomSheet];
                         }];
}

- (void)didTapOnCancelButton {
  [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::
                                  kRejectSuggestion];
  _viewController.delegate = nil;
  [_mediator didCancelScanCardSuggestion];
  [_mediator disconnect];

  __weak __typeof(self) weakSelf = self;
  [_viewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.autofillHandler
                                   dismissScanCardSaveAndFillBottomSheet];
                         }];
}

#pragma mark - Private

// Logs the exit reason for the bottom sheet if it hasn't been logged already.
- (void)logExitReasonIfNeeded:
    (ScanCardSuggestionBottomSheetExitReason)exitReason {
  if (!_exitReasonLogged) {
    [_mediator logExitReason:exitReason];
    _exitReasonLogged = YES;
  }
}

// Returns the AutofillCommands handler.
- (id<AutofillCommands>)autofillHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            AutofillCommands);
}

@end

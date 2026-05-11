// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

@interface PaymentsScanSaveAndFillOfferBottomSheetCoordinator () <
    PaymentsScanSaveAndFillOfferBottomSheetDelegate,
    WebStateListObserving>

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
    _mediator = [[PaymentsScanSaveAndFillOfferBottomSheetMediator alloc]
        initWithParams:std::move(*_params)];

    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
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

  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _viewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    [_viewController preferredHeightDetent],
    [UISheetPresentationControllerDetent mediumDetent]
  ];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  // Dismiss right away if the presentation failed to avoid having a zombie
  // coordinator.
  if (!_viewController.presentingViewController) {
    [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::
                                    kCouldNotPresent];
    id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [handler dismissPaymentSuggestions];
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
    id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
    [handler dismissPaymentSuggestions];
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
  [_mediator disconnect];
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [handler dismissPaymentSuggestions];
}

- (void)didTapScanCardButton {
  [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::
                                  kAcceptSuggestion];
  // Disable user interactions on the root view of the view controller so any
  // further user action isn't allowed. Only one action is allowed on the sheet.
  _viewController.view.userInteractionEnabled = NO;
  [_mediator didAcceptScanCardSuggestion];

  _viewController.delegate = nil;
  [_mediator disconnect];

  __weak id<BrowserCoordinatorCommands> weakHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [_viewController dismissViewControllerAnimated:YES
                                      completion:^{
                                        [weakHandler dismissPaymentSuggestions];
                                      }];
}

- (void)didTapOnCancelButton {
  [self logExitReasonIfNeeded:ScanCardSuggestionBottomSheetExitReason::
                                  kRejectSuggestion];
  _viewController.delegate = nil;
  [_mediator disconnect];

  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  __weak id<BrowserCoordinatorCommands> weakHandler = handler;
  [_viewController dismissViewControllerAnimated:YES
                                      completion:^{
                                        [weakHandler dismissPaymentSuggestions];
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

@end

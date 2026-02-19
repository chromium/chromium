// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_coordinator.h"

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface PaymentsScanSaveAndFillOfferBottomSheetCoordinator () <
    PaymentsScanSaveAndFillOfferBottomSheetDelegate>

@end

@implementation PaymentsScanSaveAndFillOfferBottomSheetCoordinator {
  // The view controller for the bottom sheet.
  PaymentsScanSaveAndFillOfferBottomSheetViewController* _viewController;

  // The mediator for the bottom sheet.
  PaymentsScanSaveAndFillOfferBottomSheetMediator* _mediator;

  // The parameters of the form that triggered the bottom sheet.
  std::optional<autofill::FormActivityParams> _params;
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

  _viewController =
      [[PaymentsScanSaveAndFillOfferBottomSheetViewController alloc] init];
  _viewController.delegate = self;
  if (_params.has_value()) {
    _mediator = [[PaymentsScanSaveAndFillOfferBottomSheetMediator alloc]
        initWithWebStateList:self.browser->GetWebStateList()
                      params:std::move(*_params)];
    _params.reset();
  }
}

#pragma mark - PaymentsScanSaveAndFillOfferBottomSheetDelegate

- (void)paymentsBottomSheetViewDidAppear {
}

- (void)paymentsBottomSheetDidDisappear {
}

- (void)didTapScanCardButton {
}

- (void)didTapOnCancelButton {
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/coordinator/payments_scan_save_and_fill_offer_bottom_sheet_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

@implementation PaymentsScanSaveAndFillOfferBottomSheetMediator {
  // Information regarding the triggering form for this bottom sheet.
  autofill::FormActivityParams _params;

  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                              params:(autofill::FormActivityParams)params {
  self = [super init];
  if (self) {
    _params = std::move(params);
    _webStateList = webStateList;
  }
  return self;
}

- (void)setConsumer:
    (id<PaymentsScanSaveAndFillOfferBottomSheetConsumer>)consumer {
  _consumer = consumer;
}

@end

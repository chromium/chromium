// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/bottom_sheet/payments_suggestion_bottom_sheet_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PaymentsSuggestionBottomSheetViewController

#pragma mark - PaymentsSuggestionBottomSheetConsumer

- (void)setCreditCardData:
    (NSArray<id<PaymentsSuggestionBottomSheetData>>*)creditCardData {
  // TODO(crbug.com/1450214): Store credit card data in the view controller
}

@end

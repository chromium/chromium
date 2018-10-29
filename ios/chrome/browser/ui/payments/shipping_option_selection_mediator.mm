// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/ui/payments/shipping_option_selection_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/strings_util.h"
#include "components/payments/core/web_payment_request.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using payments::GetShippingOptionSectionString;
}  // namespace

@interface ShippingOptionSelectionMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSMutableArray<PaymentsTextItem*>* items;

// Creates and stores the selectable items to display in the collection.
- (void)loadItems;

@end

@implementation ShippingOptionSelectionMediator

@synthesize headerText = _headerText;
@synthesize state = _state;
@synthesize selectedItemIndex = _selectedItemIndex;
@synthesize paymentRequest = _paymentRequest;
@synthesize items = _items;

- (instancetype)initWithPaymentRequest:
    (payments::PaymentRequest*)paymentRequest {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _selectedItemIndex = NSUIntegerMax;
    [self loadItems];
  }
  return self;
}

#pragma mark - PaymentRequestSelectorViewControllerDataSource

- (BOOL)allowsEditMode {
  return NO;
}

- (NSString*)title {
  return base::SysUTF16ToNSString(
      GetShippingOptionSectionString(self.paymentRequest->shipping_type()));
}

- (CollectionViewItem*)headerItem {
  if (!self.headerText.length)
    return nil;

  PaymentsTextItem* headerItem = [[PaymentsTextItem alloc] init];
  headerItem.text = self.headerText;
  if (self.state == PaymentRequestSelectorStateError)
    headerItem.leadingImage = NativeImage(IDR_IOS_PAYMENTS_WARNING);
  return headerItem;
}

- (NSArray<CollectionViewItem*>*)selectableItems {
  return self.items;
}

- (CollectionViewItem*)addButtonItem {
  return nil;
}

#pragma mark - Helper methods

- (void)loadItems {
  const std::vector<payments::PaymentShippingOption*>& shippingOptions =
      _paymentRequest->shipping_options();
  _items = [NSMutableArray arrayWithCapacity:shippingOptions.size()];
  for (size_t index = 0; index < shippingOptions.size(); ++index) {
    payments::PaymentShippingOption* shippingOption = shippingOptions[index];
    DCHECK(shippingOption);
    PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
    item.text = base::SysUTF8ToNSString(shippingOption->label);
    payments::CurrencyFormatter* currencyFormatter =
        _paymentRequest->GetOrCreateCurrencyFormatter();
    item.detailText = base::SysUTF16ToNSString(
        currencyFormatter->Format(shippingOption->amount->value));
    if (_paymentRequest->selected_shipping_option() == shippingOption)
      _selectedItemIndex = index;

    [_items addObject:item];
  }
}

@end

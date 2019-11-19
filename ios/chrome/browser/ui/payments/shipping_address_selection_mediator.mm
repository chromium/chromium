// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/ui/payments/shipping_address_selection_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payments::GetShippingAddressSectionString;
using ::payments::GetShippingAddressSelectorInfoMessage;
using ::payment_request_util::GetShippingAddressSelectorErrorMessage;
using ::payment_request_util::GetAddressNotificationLabelFromAutofillProfile;
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;
}  // namespace

@interface ShippingAddressSelectionMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSMutableArray<AutofillProfileItem*>* items;

@end

@implementation ShippingAddressSelectionMediator

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

- (NSString*)getHeaderText {
  if (self.state == PaymentRequestSelectorStateError) {
    return GetShippingAddressSelectorErrorMessage(*self.paymentRequest);
  } else {
    return self.paymentRequest->shipping_options().empty()
               ? base::SysUTF16ToNSString(GetShippingAddressSelectorInfoMessage(
                     self.paymentRequest->shipping_type()))
               : nil;
  }
}

#pragma mark - PaymentRequestSelectorViewControllerDataSource

- (BOOL)allowsEditMode {
  return YES;
}

- (NSString*)title {
  return base::SysUTF16ToNSString(
      GetShippingAddressSectionString(self.paymentRequest->shipping_type()));
}

- (CollectionViewItem*)headerItem {
  if (![self getHeaderText].length)
    return nil;

  PaymentsTextItem* headerItem = [[PaymentsTextItem alloc] init];
  headerItem.text = [self getHeaderText];
  if (self.state == PaymentRequestSelectorStateError)
    headerItem.leadingImage = NativeImage(IDR_IOS_PAYMENTS_WARNING);
  return headerItem;
}

- (NSArray<CollectionViewItem*>*)selectableItems {
  return self.items;
}

- (CollectionViewItem*)addButtonItem {
  PaymentsTextItem* addButtonItem = [[PaymentsTextItem alloc] init];
  addButtonItem.text = base::SysUTF16ToNSString(
      GetAddShippingAddressButtonLabel(self.paymentRequest->shipping_type()));
  addButtonItem.trailingImage = [[UIImage imageNamed:@"ic_add"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  addButtonItem.trailingImageTintColor = [UIColor colorNamed:kGrey400Color];
  addButtonItem.cellType = PaymentsTextCellTypeCallToAction;
  return addButtonItem;
}

#pragma mark - Public methods

- (void)loadItems {
  const std::vector<autofill::AutofillProfile*>& shippingProfiles =
      _paymentRequest->shipping_profiles();
  _items = [NSMutableArray arrayWithCapacity:shippingProfiles.size()];
  for (size_t index = 0; index < shippingProfiles.size(); ++index) {
    autofill::AutofillProfile* shippingAddress = shippingProfiles[index];
    DCHECK(shippingAddress);
    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    item.name = GetNameLabelFromAutofillProfile(*shippingAddress);
    item.address = GetShippingAddressLabelFromAutofillProfile(*shippingAddress);
    item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*shippingAddress);
    item.notification = GetAddressNotificationLabelFromAutofillProfile(
        *_paymentRequest, *shippingAddress);
    item.complete = _paymentRequest->profile_comparator()->IsShippingComplete(
        shippingAddress);
    item.useScaledFont = YES;
    if (_paymentRequest->selected_shipping_profile() == shippingAddress)
      _selectedItemIndex = index;

    [_items addObject:item];
  }
}

@end

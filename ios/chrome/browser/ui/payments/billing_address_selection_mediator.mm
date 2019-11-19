// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/ui/payments/billing_address_selection_mediator.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetAddressNotificationLabelFromAutofillProfile;
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
}  // namespace

@interface BillingAddressSelectionMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The selected billing address, if any.
@property(nonatomic, assign) autofill::AutofillProfile* selectedBillingProfile;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSMutableArray<AutofillProfileItem*>* items;

@end

@implementation BillingAddressSelectionMediator

@synthesize state = _state;
@synthesize selectedItemIndex = _selectedItemIndex;
@synthesize paymentRequest = _paymentRequest;
@synthesize selectedBillingProfile = _selectedBillingProfile;
@synthesize items = _items;

- (instancetype)initWithPaymentRequest:(payments::PaymentRequest*)paymentRequest
                selectedBillingProfile:
                    (autofill::AutofillProfile*)selectedBillingProfile {
  self = [super init];
  if (self) {
    DCHECK(paymentRequest);
    _paymentRequest = paymentRequest;
    _selectedBillingProfile = selectedBillingProfile;
    _selectedItemIndex = NSUIntegerMax;
    [self loadItems];
  }
  return self;
}

#pragma mark - PaymentRequestSelectorViewControllerDataSource

- (BOOL)allowsEditMode {
  return YES;
}

- (NSString*)title {
  return l10n_util::GetNSString(IDS_PAYMENTS_BILLING_ADDRESS);
}

- (CollectionViewItem*)headerItem {
  return nil;
}

- (NSArray<CollectionViewItem*>*)selectableItems {
  return self.items;
}

- (CollectionViewItem*)addButtonItem {
  PaymentsTextItem* addButtonItem = [[PaymentsTextItem alloc] init];
  addButtonItem.text =
      l10n_util::GetNSString(IDS_PAYMENTS_ADD_BILLING_ADDRESS_LABEL);
  addButtonItem.trailingImage = [[UIImage imageNamed:@"ic_add"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  addButtonItem.trailingImageTintColor = [UIColor colorNamed:kGrey400Color];
  addButtonItem.cellType = PaymentsTextCellTypeCallToAction;
  return addButtonItem;
}

#pragma mark - Public methods

- (void)loadItems {
  const std::vector<autofill::AutofillProfile*>& billingProfiles =
      _paymentRequest->billing_profiles();

  _items = [NSMutableArray arrayWithCapacity:billingProfiles.size()];
  for (size_t index = 0; index < billingProfiles.size(); ++index) {
    autofill::AutofillProfile* billingProfile = billingProfiles[index];
    DCHECK(billingProfile);
    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    item.name = GetNameLabelFromAutofillProfile(*billingProfile);
    item.address = GetBillingAddressLabelFromAutofillProfile(*billingProfile);
    item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*billingProfile);
    item.notification = GetAddressNotificationLabelFromAutofillProfile(
        *_paymentRequest, *billingProfile);
    item.complete = _paymentRequest->profile_comparator()->IsShippingComplete(
        billingProfile);
    item.useScaledFont = YES;
    if (self.selectedBillingProfile == billingProfile)
      _selectedItemIndex = index;

    [_items addObject:item];
  }
}

@end

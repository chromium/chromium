// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import "ios/chrome/browser/ui/payments/payment_method_selection_mediator.h"

#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/ios_payment_instrument.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
using ::payment_request_util::
    GetPaymentMethodNotificationLabelFromPaymentMethod;
}  // namespace

@interface PaymentMethodSelectionMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

// The selectable items to display in the collection.
@property(nonatomic, strong) NSMutableArray<PaymentMethodItem*>* items;

@end

@implementation PaymentMethodSelectionMediator

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
  return YES;
}

- (NSString*)title {
  return l10n_util::GetNSString(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL);
}

- (CollectionViewItem*)headerItem {
  base::string16 headerText = payments::GetCardTypesAreAcceptedText(
      _paymentRequest->supported_card_types_set());
  if (headerText.empty())
    return nil;

  PaymentsTextItem* headerItem = [[PaymentsTextItem alloc] init];
  headerItem.text = base::SysUTF16ToNSString(headerText);
  return headerItem;
}

- (NSArray<CollectionViewItem*>*)selectableItems {
  return self.items;
}

- (CollectionViewItem*)addButtonItem {
  PaymentsTextItem* addButtonItem = [[PaymentsTextItem alloc] init];
  addButtonItem.text = l10n_util::GetNSString(IDS_PAYMENTS_ADD_CARD);
  addButtonItem.trailingImage = [[UIImage imageNamed:@"ic_add"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  addButtonItem.trailingImageTintColor = [UIColor colorNamed:kGrey400Color];
  addButtonItem.cellType = PaymentsTextCellTypeCallToAction;
  return addButtonItem;
}

#pragma mark - Public methods

- (void)loadItems {
  const std::vector<payments::PaymentApp*>& paymentMethods =
      _paymentRequest->payment_methods();
  _items = [NSMutableArray arrayWithCapacity:paymentMethods.size()];
  for (size_t index = 0; index < paymentMethods.size(); ++index) {
    payments::PaymentApp* paymentMethod = paymentMethods[index];
    DCHECK(paymentMethod);
    PaymentMethodItem* item = [[PaymentMethodItem alloc] init];
    item.methodID = base::SysUTF16ToNSString(paymentMethod->GetLabel());
    item.methodDetail = base::SysUTF16ToNSString(paymentMethod->GetSublabel());
    item.notification = GetPaymentMethodNotificationLabelFromPaymentMethod(
        *paymentMethod, _paymentRequest->billing_profiles());
    item.complete = paymentMethod->IsCompleteForPayment();

    switch (paymentMethod->type()) {
      case payments::PaymentApp::Type::AUTOFILL: {
        payments::AutofillPaymentApp* autofillInstrument =
            static_cast<payments::AutofillPaymentApp*>(paymentMethod);
        autofill::AutofillProfile* billingAddress =
            autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
                autofillInstrument->credit_card()->billing_address_id(),
                _paymentRequest->billing_profiles());
        if (billingAddress) {
          item.methodAddress =
              GetBillingAddressLabelFromAutofillProfile(*billingAddress);
        }
        item.methodTypeIcon = NativeImage(paymentMethod->icon_resource_id());
        break;
      }
      case payments::PaymentApp::Type::NATIVE_MOBILE_APP: {
        payments::IOSPaymentInstrument* mobileApp =
            static_cast<payments::IOSPaymentInstrument*>(paymentMethod);
        item.methodTypeIcon = mobileApp->icon_image();
        break;
      }
      case payments::PaymentApp::Type::SERVICE_WORKER_APP: {
        NOTIMPLEMENTED();
        break;
      }
    }

    item.reserveRoomForAccessoryType = YES;
    if (_paymentRequest->selected_payment_method() == paymentMethod)
      _selectedItemIndex = index;

    [_items addObject:item];
  }
}

@end

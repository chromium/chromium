// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/ui/payments/payment_request_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/payments/core/autofill_payment_app.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/payment_item.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/strings_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/ios_payment_instrument.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payment_method_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// String used as the "URL" to take the user to the settings page for card and
// address options. Needs to be URL-like; otherwise, the link will not appear
// as a link in the UI (see setLabelLinkURL: in CollectionViewFooterCell).
const char kSettingsURL[] = "settings://card-and-address";

using ::payments::GetShippingOptionSectionString;
using ::payment_request_util::GetEmailLabelFromAutofillProfile;
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetShippingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetShippingSectionTitle;
}  // namespace

@interface PaymentRequestMediator ()

// The PaymentRequest object owning an instance of payments::WebPaymentRequest
// as provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) payments::PaymentRequest* paymentRequest;

@end

@implementation PaymentRequestMediator

@synthesize totalValueChanged = _totalValueChanged;
@synthesize paymentRequest = _paymentRequest;

- (instancetype)initWithPaymentRequest:
    (payments::PaymentRequest*)paymentRequest {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
  }
  return self;
}

#pragma mark - PaymentRequestViewControllerDataSource

- (BOOL)canPay {
  return self.paymentRequest->IsAbleToPay();
}

- (BOOL)hasPaymentItems {
  return !self.paymentRequest
              ->GetDisplayItems(self.paymentRequest->selected_payment_method())
              .empty();
}

- (BOOL)requestShipping {
  return self.paymentRequest->request_shipping();
}

- (BOOL)requestContactInfo {
  return self.paymentRequest->RequestContactInfo();
}

- (CollectionViewItem*)paymentSummaryItem {
  const payments::PaymentItem& total = self.paymentRequest->GetTotal(
      self.paymentRequest->selected_payment_method());

  PriceItem* item = [[PriceItem alloc] init];
  item.item = base::SysUTF8ToNSString(total.label);
  payments::CurrencyFormatter* currencyFormatter =
      self.paymentRequest->GetOrCreateCurrencyFormatter();
  item.price = base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
      base::UTF8ToUTF16(currencyFormatter->formatted_currency_code()),
      currencyFormatter->Format(total.amount->value)));
  item.notification = self.totalValueChanged
                          ? l10n_util::GetNSString(IDS_PAYMENTS_UPDATED_LABEL)
                          : nil;
  self.totalValueChanged = NO;
  if ([self hasPaymentItems]) {
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  return item;
}

- (PaymentsTextItem*)shippingSectionHeaderItem {
  if (!self.paymentRequest->selected_shipping_profile())
    return nil;
  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text = GetShippingSectionTitle(self.paymentRequest->shipping_type());
  return item;
}

- (CollectionViewItem*)shippingAddressItem {
  const autofill::AutofillProfile* profile =
      self.paymentRequest->selected_shipping_profile();
  if (profile) {
    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    item.name = GetNameLabelFromAutofillProfile(*profile);
    item.address = GetShippingAddressLabelFromAutofillProfile(*profile);
    item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*profile);
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    item.useScaledFont = YES;
    return item;
  }

  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  if (self.paymentRequest->shipping_profiles().empty()) {
    item.text = base::SysUTF16ToNSString(
        GetAddShippingAddressButtonLabel(self.paymentRequest->shipping_type()));
    item.trailingImage = [[UIImage imageNamed:@"ic_add"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    item.trailingImageTintColor = [UIColor colorNamed:kGrey400Color];
  } else {
    item.text = base::SysUTF16ToNSString(GetChooseShippingAddressButtonLabel(
        self.paymentRequest->shipping_type()));
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  item.cellType = PaymentsTextCellTypeCallToAction;
  return item;
}

- (CollectionViewItem*)shippingOptionItem {
  if (!self.paymentRequest->selected_shipping_profile())
    return nullptr;
  const payments::PaymentShippingOption* option =
      self.paymentRequest->selected_shipping_option();
  if (option) {
    PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
    item.text = base::SysUTF8ToNSString(option->label);
    payments::CurrencyFormatter* currencyFormatter =
        self.paymentRequest->GetOrCreateCurrencyFormatter();
    item.detailText = base::SysUTF16ToNSString(
        currencyFormatter->Format(option->amount->value));
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    return item;
  }

  if (self.paymentRequest->shipping_options().empty())
    return nullptr;

  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text = base::SysUTF16ToNSString(
      GetChooseShippingOptionButtonLabel(self.paymentRequest->shipping_type()));
  item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  item.cellType = PaymentsTextCellTypeCallToAction;
  return item;
}

- (PaymentsTextItem*)paymentMethodSectionHeaderItem {
  if (!self.paymentRequest->selected_payment_method())
    return nil;
  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text =
      l10n_util::GetNSString(IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME);
  return item;
}

- (CollectionViewItem*)paymentMethodItem {
  payments::PaymentApp* paymentMethod =
      self.paymentRequest->selected_payment_method();
  if (paymentMethod) {
    PaymentMethodItem* item = [[PaymentMethodItem alloc] init];
    item.methodID = base::SysUTF16ToNSString(paymentMethod->GetLabel());
    item.methodDetail = base::SysUTF16ToNSString(paymentMethod->GetSublabel());

    switch (paymentMethod->type()) {
      case payments::PaymentApp::Type::AUTOFILL: {
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

    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    return item;
  }

  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  if (self.paymentRequest->payment_methods().empty()) {
    item.text = l10n_util::GetNSString(IDS_ADD_PAYMENT_METHOD);
    item.trailingImage = [[UIImage imageNamed:@"ic_add"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    item.trailingImageTintColor = [UIColor colorNamed:kGrey400Color];
  } else {
    item.text = l10n_util::GetNSString(IDS_CHOOSE_PAYMENT_METHOD);
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  item.cellType = PaymentsTextCellTypeCallToAction;
  return item;
}

- (PaymentsTextItem*)contactInfoSectionHeaderItem {
  if (!self.paymentRequest->selected_contact_profile())
    return nil;
  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  item.text =
      l10n_util::GetNSString(IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME);
  return item;
}

- (CollectionViewItem*)contactInfoItem {
  const autofill::AutofillProfile* profile =
      self.paymentRequest->selected_contact_profile();
  if (profile) {
    DCHECK(self.paymentRequest->request_payer_name() ||
           self.paymentRequest->request_payer_email() ||
           self.paymentRequest->request_payer_phone());

    AutofillProfileItem* item = [[AutofillProfileItem alloc] init];
    if (self.paymentRequest->request_payer_name())
      item.name = GetNameLabelFromAutofillProfile(*profile);
    if (self.paymentRequest->request_payer_phone())
      item.phoneNumber = GetPhoneNumberLabelFromAutofillProfile(*profile);
    if (self.paymentRequest->request_payer_email())
      item.email = GetEmailLabelFromAutofillProfile(*profile);
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
    item.useScaledFont = YES;
    return item;
  }

  PaymentsTextItem* item = [[PaymentsTextItem alloc] init];
  if (self.paymentRequest->contact_profiles().empty()) {
    item.text = l10n_util::GetNSString(IDS_PAYMENT_REQUEST_ADD_CONTACT_INFO);
    item.trailingImage = [[UIImage imageNamed:@"ic_add"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    item.trailingImageTintColor = [UIColor colorNamed:kGrey400Color];
  } else {
    item.text = l10n_util::GetNSString(IDS_PAYMENT_REQUEST_CHOOSE_CONTACT_INFO);
    item.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  }
  item.cellType = PaymentsTextCellTypeCallToAction;
  return item;
}

- (CollectionViewFooterItem*)footerItem {
  CollectionViewFooterItem* item = [[CollectionViewFooterItem alloc] init];
  item.useScaledFont = YES;

  // If no transaction has been completed so far, choose which string to display
  // as a function of the profile's signed in state. Otherwise, always show the
  // same string.
  const bool firstTransactionCompleted =
      _paymentRequest->GetPrefService()->GetBoolean(
          payments::kPaymentsFirstTransactionCompleted);
  if (firstTransactionCompleted) {
    item.text = l10n_util::GetNSString(IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS);
  } else {
    const std::string email = _paymentRequest->GetAuthenticatedEmail();
    if (!email.empty()) {
      const std::string formattedString = l10n_util::GetStringFUTF8(
          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_IN,
          base::UTF8ToUTF16(email));
      item.text = base::SysUTF8ToNSString(formattedString);
    } else {
      item.text = l10n_util::GetNSString(
          IDS_PAYMENTS_CARD_AND_ADDRESS_SETTINGS_SIGNED_OUT);
    }
  }
  item.linkURL = GURL(kSettingsURL);
  return item;
}

@end

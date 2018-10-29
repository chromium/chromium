// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/ios_payment_instrument_launcher.h"

#include <map>
#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_instrument.h"
#import "ios/chrome/browser/payments/payment_request_constants.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/web_state/web_state.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Parameters sent to a payment app.
static const char kMethodNames[] = "methodNames";
static const char kMethodData[] = "methodData";
static const char kMerchantName[] = "merchantName";
static const char kTopLevelOrigin[] = "topLevelOrigin";
static const char kTopLevelCertificateChain[] = "topLevelCertificateChain";
static const char kCertificate[] = "cert";
static const char kTotal[] = "total";
static const char kModifiers[] = "modifiers";

// Parameters received from a payment app.
static const char kSuccess[] = "success";
static const char kMethodName[] = "methodName";
static const char kDetails[] = "details";

}  // namespace

namespace payments {

IOSPaymentInstrumentLauncher::IOSPaymentInstrumentLauncher()
    : delegate_(nullptr), payment_request_id_("") {}

IOSPaymentInstrumentLauncher::~IOSPaymentInstrumentLauncher() {}

bool IOSPaymentInstrumentLauncher::LaunchIOSPaymentInstrument(
    payments::PaymentRequest* payment_request,
    web::WebState* active_web_state,
    GURL& universal_link,
    payments::PaymentInstrument::Delegate* delegate) {
  DCHECK(delegate);

  // Only one request can be handled at a time.
  if (delegate_ || !payment_request_id_.empty()) {
    return false;
  }

  delegate_ = delegate;

  std::unique_ptr<base::DictionaryValue> params_to_payment_app =
      std::make_unique<base::DictionaryValue>();

  // TODO(crbug.com/748556): Filter the following list to only show method names
  // that we know the payment app supports. For now, sending all the requested
  // method names i.e., 'basic-card' and 'https://alice-pay.com" to the payment
  // app works as the payment app provider can then decide what to do with that
  // information, but this is not ideal nor is this consistent with Android
  // implementation.
  std::unique_ptr<base::ListValue> method_names =
      std::make_unique<base::ListValue>();
  for (auto const& it : payment_request->stringified_method_data())
    method_names->GetList().emplace_back(it.first);
  params_to_payment_app->SetList(kMethodNames, std::move(method_names));

  params_to_payment_app->SetDictionary(
      kMethodData,
      SerializeMethodData(payment_request->stringified_method_data()));

  params_to_payment_app->SetString(
      kMerchantName, base::UTF16ToASCII(active_web_state->GetTitle()));

  params_to_payment_app->SetKey(
      kTopLevelOrigin,
      base::Value(active_web_state->GetLastCommittedURL().host()));

  params_to_payment_app->SetList(
      kTopLevelCertificateChain,
      SerializeCertificateChain(
          active_web_state->GetNavigationManager()->GetVisibleItem()));

  DCHECK(payment_request->web_payment_request().details.total);
  params_to_payment_app->SetDictionary(
      kTotal,
      PaymentCurrencyAmountToDictionaryValue(
          *(payment_request->web_payment_request().details.total->amount)));

  params_to_payment_app->SetList(
      kModifiers,
      SerializeModifiers(payment_request->web_payment_request().details));

  // JSON stringify the object so that it can be encoded in base-64.
  std::string stringified_parameters;
  base::JSONWriter::Write(*params_to_payment_app, &stringified_parameters);
  std::string base_64_params;
  base::Base64Encode(stringified_parameters, &base_64_params);

  payment_request_id_ =
      payment_request->web_payment_request().payment_request_id;

  universal_link = net::AppendQueryParameter(
      universal_link, payments::kPaymentRequestIDExternal, payment_request_id_);
  universal_link = net::AppendQueryParameter(
      universal_link, payments::kPaymentRequestDataExternal, base_64_params);
  NSURL* url = net::NSURLWithGURL(universal_link);

  [[UIApplication sharedApplication] openURL:url
      options:@{
        UIApplicationOpenURLOptionUniversalLinksOnly : @YES
      }
      completionHandler:^(BOOL success) {
        if (!success) {
          CompleteLaunchRequest("", "");
        }
      }];

  return true;
}

void IOSPaymentInstrumentLauncher::ReceiveResponseFromIOSPaymentInstrument(
    const std::string& base_64_response) {
  DCHECK(delegate_);

  std::string stringified_parameters;
  base::Base64Decode(base_64_response, &stringified_parameters);

  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(stringified_parameters);
  if (!value) {
    CompleteLaunchRequest("", "");
    return;
  }

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(value));
  if (!dict) {
    CompleteLaunchRequest("", "");
    return;
  }

  int success;
  if (!dict->GetInteger(kSuccess, &success) || success == 0) {
    CompleteLaunchRequest("", "");
    return;
  }

  std::string method_name;
  if (!dict->GetString(kMethodName, &method_name) || method_name.empty()) {
    CompleteLaunchRequest("", "");
    return;
  }

  std::string stringified_details;
  if (!dict->GetString(kDetails, &stringified_details) ||
      stringified_details.empty()) {
    CompleteLaunchRequest("", "");
    return;
  }

  CompleteLaunchRequest(method_name, stringified_details);
}

std::unique_ptr<base::DictionaryValue>
IOSPaymentInstrumentLauncher::SerializeMethodData(
    const std::map<std::string, std::set<std::string>>&
        stringified_method_data) {
  std::unique_ptr<base::DictionaryValue> method_data =
      std::make_unique<base::DictionaryValue>();

  for (auto const& map_it : stringified_method_data) {
    base::ListValue data_list;
    for (auto const& data_it : map_it.second) {
      // We insert the stringified data, not the JSON object and only if the
      // corresponding JSON object is valid.
      if (base::JSONReader().ReadToValue(data_it))
        data_list.GetList().emplace_back(data_it);
    }

    method_data->SetKey(map_it.first, std::move(data_list));
  }

  return method_data;
}

std::unique_ptr<base::ListValue>
IOSPaymentInstrumentLauncher::SerializeCertificateChain(
    web::NavigationItem* item) {
  std::unique_ptr<base::ListValue> cert_chain_list =
      std::make_unique<base::ListValue>();

  if (!item)
    return cert_chain_list;

  scoped_refptr<net::X509Certificate> cert = item->GetSSL().certificate;
  std::vector<base::StringPiece> cert_chain;

  cert_chain.reserve(1 + cert->intermediate_buffers().size());
  cert_chain.push_back(
      net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));
  for (const auto& handle : cert->intermediate_buffers()) {
    cert_chain.push_back(
        net::x509_util::CryptoBufferAsStringPiece(handle.get()));
  }

  std::unique_ptr<base::ListValue> byte_array;
  for (const auto& cert_string : cert_chain) {
    base::ListValue byte_array;
    byte_array.GetList().reserve(cert_string.size());
    for (const char byte : cert_string)
      byte_array.GetList().emplace_back(byte);

    base::DictionaryValue cert_chain_dict;
    cert_chain_dict.SetKey(kCertificate, std::move(byte_array));
    cert_chain_list->GetList().push_back(std::move(cert_chain_dict));
  }

  return cert_chain_list;
}

std::unique_ptr<base::ListValue>
IOSPaymentInstrumentLauncher::SerializeModifiers(PaymentDetails details) {
  std::unique_ptr<base::ListValue> modifiers =
      std::make_unique<base::ListValue>();
  size_t numModifiers = details.modifiers.size();
  for (size_t i = 0; i < numModifiers; ++i) {
    std::unique_ptr<base::DictionaryValue> modifier =
        details.modifiers[i].ToDictionaryValue();
    modifiers->GetList().push_back(std::move(*modifier));
  }

  return modifiers;
}

void IOSPaymentInstrumentLauncher::CompleteLaunchRequest(
    const std::string& method_name,
    const std::string& details) {
  if (!method_name.empty() && !details.empty())
    delegate_->OnInstrumentDetailsReady(method_name, details);
  else
    delegate_->OnInstrumentDetailsError();
  delegate_ = nullptr;
  payment_request_id_ = "";
}

}  // namespace payments

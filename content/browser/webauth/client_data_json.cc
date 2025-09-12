// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/client_data_json.h"

#include "base/check.h"
#include "components/webauthn/core/browser/common_utils.h"

using webauthn::ToJSONString;

namespace content {

std::string BuildClientDataJson(webauthn::ClientDataJsonParams params) {
  return webauthn::BuildClientDataJson(std::move(params), std::nullopt);
}

std::string BuildClientDataJsonWithPayment(
    webauthn::ClientDataJsonParams params,
    blink::mojom::PaymentOptionsPtr payment_options,
    std::string_view payment_rp) {
  std::string payment_json;
  payment_json.reserve(128);

  if (payment_options &&
      params.type == webauthn::ClientDataRequestType::kPaymentGet) {
    payment_json.append(R"(,"payment":{)");

    payment_json.append(R"("rpId":)");
    payment_json.append(ToJSONString(payment_rp));

    payment_json.append(R"(,"topOrigin":)");
    payment_json.append(ToJSONString(params.top_origin.Serialize()));

    if (payment_options->payee_name.has_value()) {
      payment_json.append(R"(,"payeeName":)");
      payment_json.append(ToJSONString(payment_options->payee_name.value()));
    }
    if (payment_options->payee_origin.has_value()) {
      payment_json.append(R"(,"payeeOrigin":)");
      payment_json.append(
          ToJSONString(payment_options->payee_origin->Serialize()));
    }

    if (payment_options->payment_entities_logos.has_value()) {
      const std::vector<blink::mojom::ShownPaymentEntityLogoPtr>& logos =
          *payment_options->payment_entities_logos;
      payment_json.append(R"(,"paymentEntitiesLogos":[)");
      for (auto logo_iterator = logos.begin(); logo_iterator != logos.end();
           ++logo_iterator) {
        payment_json.append(R"({"url":)");
        if ((*logo_iterator)->url.is_empty()) {
          payment_json.append(R"("")");
        } else {
          payment_json.append(ToJSONString((*logo_iterator)->url.spec()));
        }
        payment_json.append(R"(,"label":)");
        payment_json.append(ToJSONString((*logo_iterator)->label));
        payment_json.append("}");
        if ((logo_iterator + 1) != logos.end()) {
          payment_json.append(",");
        }
      }
      payment_json.append("]");
    }

    payment_json.append(R"(,"total":{)");

    payment_json.append(R"("value":)");
    payment_json.append(ToJSONString(payment_options->total->value));

    payment_json.append(R"(,"currency":)");
    payment_json.append(ToJSONString(payment_options->total->currency));

    payment_json.append(R"(},"instrument":{)");

    payment_json.append(R"("icon":)");
    payment_json.append(ToJSONString(payment_options->instrument->icon.spec()));

    payment_json.append(R"(,"displayName":)");
    payment_json.append(
        ToJSONString(payment_options->instrument->display_name));

    if (payment_options->instrument->details.has_value()) {
      // SPC calls should have been rejected if the details field was present
      // but empty.
      CHECK(!payment_options->instrument->details->empty());

      payment_json.append(R"(,"details":)");
      payment_json.append(ToJSONString(*payment_options->instrument->details));
    }

    payment_json.append("}");
    if (payment_options->browser_bound_public_key.has_value()) {
      payment_json.append(R"(,"browserBoundPublicKey":)");
      payment_json.append(ToJSONString(webauthn::Base64UrlEncodeOmitPadding(
          *payment_options->browser_bound_public_key)));
    }
    payment_json.append("}");
  } else if (payment_options &&
             payment_options->browser_bound_public_key.has_value() &&
             params.type == webauthn::ClientDataRequestType::kWebAuthnCreate) {
    payment_json.append(R"(,"payment":{"browserBoundPublicKey":)");
    payment_json.append(ToJSONString(webauthn::Base64UrlEncodeOmitPadding(
        *payment_options->browser_bound_public_key)));
    payment_json.append("}");
  }

  return webauthn::BuildClientDataJson(std::move(params), payment_json);
}

}  // namespace content

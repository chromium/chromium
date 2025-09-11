// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/client_data_json.h"

#include <string_view>

#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/webauthn/core/browser/common_utils.h"
#include "content/public/common/content_features.h"

namespace content {
namespace {

// ToJSONString encodes |in| as a JSON string, using the specific escaping rules
// required by https://github.com/w3c/webauthn/pull/1375.
std::string ToJSONString(std::string_view in) {
  std::string ret;
  ret.reserve(in.size() + 2);
  ret.push_back('"');

  base::span<const char> in_bytes = base::span(in);
  const size_t length = in.size();
  size_t offset = 0;

  while (offset < length) {
    const size_t prior_offset = offset;
    // Input strings must be valid UTF-8.
    base_icu::UChar32 codepoint;
    CHECK(base::ReadUnicodeCharacter(in_bytes.data(), length, &offset,
                                     &codepoint));
    // offset is updated by |ReadUnicodeCharacter| to index the last byte of the
    // codepoint. Increment it to index the first byte of the next codepoint for
    // the subsequent iteration.
    offset++;

    if (codepoint == 0x20 || codepoint == 0x21 ||
        (codepoint >= 0x23 && codepoint <= 0x5b) || codepoint >= 0x5d) {
      ret.append(&in_bytes[prior_offset], offset - prior_offset);
    } else if (codepoint == 0x22) {
      ret.append("\\\"");
    } else if (codepoint == 0x5c) {
      ret.append("\\\\");
    } else {
      ret.append("\\u00");
      base::AppendHexEncodedByte(static_cast<uint8_t>(codepoint), ret, false);
    }
  }

  ret.push_back('"');
  return ret;
}

// Builds the CollectedClientData[1] dictionary with the given values,
// serializes it to JSON, and returns the resulting string.
// This CHECKs if `challenge` has not been provided with a value.
// [1] https://w3c.github.io/webauthn/#dictdef-collectedclientdata
// Optionally, the payments JSON section can be provided and added to
// the proper section of the resulting JSON output.
std::string BuildClientDataJson(content::ClientDataJsonParams params,
                                std::optional<std::string_view> payment_json) {
  CHECK(params.challenge.has_value());

  std::string ret;
  ret.reserve(128);

  switch (params.type) {
    case content::ClientDataRequestType::kWebAuthnCreate:
      ret.append(R"({"type":"webauthn.create")");
      break;
    case content::ClientDataRequestType::kWebAuthnGet:
      ret.append(R"({"type":"webauthn.get")");
      break;
    case content::ClientDataRequestType::kPaymentGet:
      ret.append(R"({"type":"payment.get")");
      break;
  }

  ret.append(R"(,"challenge":)");
  ret.append(
      ToJSONString(webauthn::Base64UrlEncodeOmitPadding(*params.challenge)));

  ret.append(R"(,"origin":)");
  ret.append(ToJSONString(params.origin.Serialize()));

  if (params.is_cross_origin_iframe) {
    ret.append(R"(,"crossOrigin":true)");
    ret.append(R"(,"topOrigin":)");
    ret.append(ToJSONString(params.top_origin.Serialize()));
  } else {
    ret.append(R"(,"crossOrigin":false)");
  }

  if (payment_json.has_value()) {
    ret.append(*payment_json);
  }

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-ietf-tls-grease
    ret.append(R"(,"other_keys_can_be_added_here":")");
    ret.append(
        "do not compare clientDataJSON against a template. See "
        "https://goo.gl/yabPex\"");
  }

  ret.append("}");
  return ret;
}

}  // namespace

ClientDataJsonParams::ClientDataJsonParams(
    ClientDataRequestType type,
    url::Origin origin,
    url::Origin top_origin,
    std::optional<std::vector<uint8_t>> challenge,
    bool is_cross_origin_iframe)
    : type(type),
      origin(std::move(origin)),
      top_origin(std::move(top_origin)),
      challenge(std::move(challenge)),
      is_cross_origin_iframe(is_cross_origin_iframe) {}
ClientDataJsonParams::ClientDataJsonParams(ClientDataJsonParams&&) = default;
ClientDataJsonParams& ClientDataJsonParams::operator=(ClientDataJsonParams&&) =
    default;
ClientDataJsonParams::~ClientDataJsonParams() = default;

std::string BuildClientDataJson(ClientDataJsonParams params) {
  return BuildClientDataJson(std::move(params), std::nullopt);
}

std::string BuildClientDataJsonWithPayment(
    ClientDataJsonParams params,
    blink::mojom::PaymentOptionsPtr payment_options,
    std::string_view payment_rp) {
  std::string payment_json;
  payment_json.reserve(128);

  if (payment_options && params.type == ClientDataRequestType::kPaymentGet) {
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
             params.type == ClientDataRequestType::kWebAuthnCreate) {
    payment_json.append(R"(,"payment":{"browserBoundPublicKey":)");
    payment_json.append(ToJSONString(webauthn::Base64UrlEncodeOmitPadding(
        *payment_options->browser_bound_public_key)));
    payment_json.append("}");
  }

  return BuildClientDataJson(std::move(params), payment_json);
}

}  // namespace content

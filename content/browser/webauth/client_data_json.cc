// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/webauth/client_data_json.h"

#include <string_view>

#include "base/base64url.h"
#include "base/check.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "content/public/common/content_features.h"

namespace content {
namespace {

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(input.data()),
                       input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &ret);
  return ret;
}

// ToJSONString encodes |in| as a JSON string, using the specific escaping rules
// required by https://github.com/w3c/webauthn/pull/1375.
std::string ToJSONString(std::string_view in) {
  std::string ret;
  ret.reserve(in.size() + 2);
  ret.push_back('"');

  const char* const in_bytes = in.data();
  const size_t length = in.size();
  size_t offset = 0;

  while (offset < length) {
    const size_t prior_offset = offset;
    // Input strings must be valid UTF-8.
    base_icu::UChar32 codepoint;
    CHECK(base::ReadUnicodeCharacter(in_bytes, length, &offset, &codepoint));
    // offset is updated by |ReadUnicodeCharacter| to index the last byte of the
    // codepoint. Increment it to index the first byte of the next codepoint for
    // the subsequent iteration.
    offset++;

    if (codepoint == 0x20 || codepoint == 0x21 ||
        (codepoint >= 0x23 && codepoint <= 0x5b) || codepoint >= 0x5d) {
      ret.append(&in_bytes[prior_offset], &in_bytes[offset]);
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

}  // namespace

ClientDataJsonParams::ClientDataJsonParams(ClientDataRequestType type,
                                           url::Origin origin,
                                           url::Origin top_origin,
                                           std::vector<uint8_t> challenge,
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
  std::string ret;
  ret.reserve(128);

  switch (params.type) {
    case ClientDataRequestType::kWebAuthnCreate:
      ret.append(R"({"type":"webauthn.create")");
      break;
    case ClientDataRequestType::kWebAuthnGet:
      ret.append(R"({"type":"webauthn.get")");
      break;
    case ClientDataRequestType::kPaymentGet:
      ret.append(R"({"type":"payment.get")");
      break;
  }

  ret.append(R"(,"challenge":)");
  ret.append(ToJSONString(Base64UrlEncode(params.challenge)));

  ret.append(R"(,"origin":)");
  ret.append(ToJSONString(params.origin.Serialize()));

  std::string serialized_top_origin =
      ToJSONString(params.top_origin.Serialize());
  if (params.is_cross_origin_iframe) {
    ret.append(R"(,"crossOrigin":true)");
    ret.append(R"(,"topOrigin":)");
    ret.append(serialized_top_origin);
  } else {
    ret.append(R"(,"crossOrigin":false)");
  }

  if (params.payment_options) {
    ret.append(R"(,"payment":{)");

    ret.append(R"("rpId":)");
    ret.append(ToJSONString(params.payment_rp));

    ret.append(R"(,"topOrigin":)");
    ret.append(serialized_top_origin);

    if (params.payment_options->payee_name.has_value()) {
      ret.append(R"(,"payeeName":)");
      ret.append(ToJSONString(params.payment_options->payee_name.value()));
    }
    if (params.payment_options->payee_origin.has_value()) {
      ret.append(R"(,"payeeOrigin":)");
      ret.append(
          ToJSONString(params.payment_options->payee_origin->Serialize()));
    }

    ret.append(R"(,"total":{)");

    ret.append(R"("value":)");
    ret.append(ToJSONString(params.payment_options->total->value));

    ret.append(R"(,"currency":)");
    ret.append(ToJSONString(params.payment_options->total->currency));

    ret.append(R"(},"instrument":{)");

    ret.append(R"("icon":)");
    ret.append(ToJSONString(params.payment_options->instrument->icon.spec()));

    ret.append(R"(,"displayName":)");
    ret.append(ToJSONString(params.payment_options->instrument->display_name));

    ret.append("}}");
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

}  // namespace content

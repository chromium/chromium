// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/client_data_json.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "components/webauthn/core/browser/common_utils.h"

namespace webauthn {

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
std::string BuildClientDataJson(ClientDataJsonParams params,
                                std::optional<std::string_view> payment_json) {
  CHECK(params.challenge.has_value());

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

}  // namespace webauthn

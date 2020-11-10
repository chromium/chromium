// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/trust_token_attribute_parsing.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trust_token.h"
#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace internal {

namespace {
bool ParseType(const String& in, network::mojom::TrustTokenOperationType* out) {
  if (in == "token-request") {
    *out = network::mojom::TrustTokenOperationType::kIssuance;
    return true;
  } else if (in == "token-redemption") {
    *out = network::mojom::TrustTokenOperationType::kRedemption;
    return true;
  } else if (in == "send-redemption-record") {
    *out = network::mojom::TrustTokenOperationType::kSigning;
    return true;
  } else {
    return false;
  }
}
bool ParseRefreshPolicy(const String& in,
                        network::mojom::TrustTokenRefreshPolicy* out) {
  if (in == "none") {
    *out = network::mojom::TrustTokenRefreshPolicy::kUseCached;
    return true;
  } else if (in == "refresh") {
    *out = network::mojom::TrustTokenRefreshPolicy::kRefresh;
    return true;
  }
  return false;
}
bool ParseSignRequestData(const String& in,
                          network::mojom::TrustTokenSignRequestData* out) {
  if (in == "omit") {
    *out = network::mojom::TrustTokenSignRequestData::kOmit;
    return true;
  } else if (in == "headers-only") {
    *out = network::mojom::TrustTokenSignRequestData::kHeadersOnly;
    return true;
  } else if (in == "include") {
    *out = network::mojom::TrustTokenSignRequestData::kInclude;
    return true;
  }
  return false;
}
}  // namespace

// Given a JSON representation of a Trust Token parameters struct, constructs
// and returns the represented struct if the JSON representation is valid;
// returns nullopt otherwise.
network::mojom::blink::TrustTokenParamsPtr TrustTokenParamsFromJson(
    std::unique_ptr<JSONValue> in) {
  JSONObject* object = JSONObject::Cast(in.get());

  if (!object)
    return nullptr;

  auto ret = network::mojom::blink::TrustTokenParams::New();

  // |type| is required.
  String type;
  if (!object->GetString("type", &type))
    return nullptr;
  if (!ParseType(type, &ret->type))
    return nullptr;

  // |refreshPolicy| is optional.
  if (JSONValue* refresh_policy = object->Get("refreshPolicy")) {
    String str_policy;
    if (!refresh_policy->AsString(&str_policy))
      return nullptr;
    if (!ParseRefreshPolicy(str_policy, &ret->refresh_policy))
      return nullptr;
  }

  // |signRequestData| is optional.
  if (JSONValue* sign_request_data = object->Get("signRequestData")) {
    String str_sign_request_data;
    if (!sign_request_data->AsString(&str_sign_request_data))
      return nullptr;
    if (!ParseSignRequestData(str_sign_request_data, &ret->sign_request_data)) {
      return nullptr;
    }
  }

  // |includeTimestampHeader| is optional.
  if (JSONValue* include_timestamp_header =
          object->Get("includeTimestampHeader")) {
    if (!include_timestamp_header->AsBoolean(&ret->include_timestamp_header))
      return nullptr;
  }

  // |issuers| is optional; if it's provided, it should be nonempty and contain
  // origins that are valid, potentially trustworthy, and HTTP or HTTPS.
  if (JSONValue* issuers = object->Get("issuers")) {
    JSONArray* issuers_array = JSONArray::Cast(issuers);
    if (!issuers_array || !issuers_array->size())
      return nullptr;

    // Because of the characteristics of the Trust Tokens protocol, we expect
    // under 5 elements in this array.
    for (size_t i = 0; i < issuers_array->size(); ++i) {
      String str_issuer;
      if (!issuers_array->at(i)->AsString(&str_issuer))
        return nullptr;

      ret->issuers.push_back(SecurityOrigin::CreateFromString(str_issuer));
      const scoped_refptr<const SecurityOrigin>& issuer = ret->issuers.back();
      if (!issuer)
        return nullptr;
      if (!issuer->IsPotentiallyTrustworthy())
        return nullptr;
      if (issuer->Protocol() != "http" && issuer->Protocol() != "https")
        return nullptr;
    }
  }

  // |additionalSignedHeaders| is optional.
  if (JSONValue* additional_signed_headers =
          object->Get("additionalSignedHeaders")) {
    JSONArray* signed_headers_array =
        JSONArray::Cast(additional_signed_headers);
    if (!signed_headers_array)
      return nullptr;

    // Because of the characteristics of the Trust Tokens protocol, we expect
    // roughly 2-5 elements in this array.
    for (size_t i = 0; i < signed_headers_array->size(); ++i) {
      String next;
      if (!signed_headers_array->at(i)->AsString(&next))
        return nullptr;
      ret->additional_signed_headers.push_back(std::move(next));
    }
  }

  // |additionalSigningData| is optional.
  if (JSONValue* additional_signing_data =
          object->Get("additionalSigningData")) {
    if (!additional_signing_data->AsString(
            &ret->possibly_unsafe_additional_signing_data))
      return nullptr;
  }

  return ret;
}

}  // namespace internal
}  // namespace blink

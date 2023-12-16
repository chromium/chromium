// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/trust_token_attribute_parsing.h"
#include "base/logging.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/blink/renderer/core/fetch/trust_token_to_mojom.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink::internal {

namespace {
bool ParseOperation(const String& in,
                    network::mojom::TrustTokenOperationType* out) {
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

  // |version| is required, though unused.
  int version;
  if (!object->GetInteger("version", &version)) {
    LOG(WARNING) << "expected integer trust token version, got none";
    return nullptr;
  }
  // Although we don't use the version number internally, it's still the case
  // that we only understand version 1.
  if (version != 1) {
    LOG(WARNING) << "expected trust token version 1, got " << version;
    return nullptr;
  }

  // |operation| is required.
  String operation;
  if (!object->GetString("operation", &operation)) {
    return nullptr;
  }
  if (!ParseOperation(operation, &ret->operation)) {
    return nullptr;
  }

  // |refreshPolicy| is optional.
  if (JSONValue* refresh_policy = object->Get("refreshPolicy")) {
    String str_policy;
    if (!refresh_policy->AsString(&str_policy))
      return nullptr;
    if (!ParseRefreshPolicy(str_policy, &ret->refresh_policy))
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
    for (wtf_size_t i = 0; i < issuers_array->size(); ++i) {
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

  return ret;
}

}  // namespace blink::internal

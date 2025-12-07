// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_CLIENT_DATA_JSON_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_CLIENT_DATA_JSON_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "url/origin.h"

namespace webauthn {

// ClientDataRequestType enumerates different request types that
// CollectedClientData can be built for. See
// |BuildClientDataJson|.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ClientDataRequestType
enum class ClientDataRequestType {
  kWebAuthnCreate,
  kWebAuthnGet,
  kPaymentGet,
};

// ClientDataJsonParams is the parameter struct for `BuildClientDataJson()`.
struct ClientDataJsonParams {
  ClientDataJsonParams(ClientDataRequestType type,
                       url::Origin origin,
                       url::Origin top_origin,
                       std::optional<std::vector<uint8_t>> challenge,
                       bool is_cross_origin_iframe = false);
  ClientDataJsonParams(ClientDataJsonParams&&);
  ClientDataJsonParams& operator=(ClientDataJsonParams&&);
  ~ClientDataJsonParams();

  ClientDataRequestType type;
  url::Origin origin;
  url::Origin top_origin;
  std::optional<std::vector<uint8_t>> challenge;
  bool is_cross_origin_iframe = false;
};

// Builds the CollectedClientData[1] dictionary with the given values,
// serializes it to JSON, and returns the resulting string.
// This CHECKs if `challenge` has not been provided with a value.
// [1] https://w3c.github.io/webauthn/#dictdef-collectedclientdata
// Optionally, the payments JSON section can be provided and added to
// the proper section of the resulting JSON output.
std::string BuildClientDataJson(ClientDataJsonParams params,
                                std::optional<std::string_view> payment_json);

// ToJSONString encodes |in| as a JSON string, using the specific escaping rules
// required by https://github.com/w3c/webauthn/pull/1375.
std::string ToJSONString(std::string_view in);

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_CLIENT_DATA_JSON_H_

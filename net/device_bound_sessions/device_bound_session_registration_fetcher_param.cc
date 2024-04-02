// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/device_bound_session_registration_fetcher_param.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/schemeful_site.h"
#include "net/http/structured_headers.h"

#include <vector>

namespace {
// TODO(kristianm): See if these can be used with
// services/network/sec_header_helpers.cc
constexpr char kRegistrationHeaderName[] = "Sec-Session-Registration";
constexpr char kChallengeItemKey[] = "challenge";

constexpr char kES256[] = "es256";
constexpr char kRS256[] = "rs256";

std::optional<crypto::SignatureVerifier::SignatureAlgorithm> AlgoFromString(
    const std::string_view& algo) {
  if (base::EqualsCaseInsensitiveASCII(algo, kES256)) {
    return crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  if (base::EqualsCaseInsensitiveASCII(algo, kRS256)) {
    return crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  return std::nullopt;
}
}  // namespace

namespace net {

DeviceBoundSessionRegistrationFetcherParam::
    DeviceBoundSessionRegistrationFetcherParam(
        DeviceBoundSessionRegistrationFetcherParam&& other) = default;

DeviceBoundSessionRegistrationFetcherParam&
DeviceBoundSessionRegistrationFetcherParam::
    operator=(
        DeviceBoundSessionRegistrationFetcherParam&& other) noexcept = default;

DeviceBoundSessionRegistrationFetcherParam::
    ~DeviceBoundSessionRegistrationFetcherParam() =
        default;

DeviceBoundSessionRegistrationFetcherParam::
    DeviceBoundSessionRegistrationFetcherParam(
        GURL registration_endpoint,
        std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
            supported_algos,
        std::string challenge)
        : registration_endpoint_(std::move(registration_endpoint)),
          supported_algos_(std::move(supported_algos)),
          challenge_(std::move(challenge)) {}

std::optional<DeviceBoundSessionRegistrationFetcherParam>
DeviceBoundSessionRegistrationFetcherParam::ParseItem(
    const GURL& request_url,
    structured_headers::Item item,
    structured_headers::Parameters params) {
  if (!item.is_string()) {
    return std::nullopt;
  }

  // TODO(kristianm): Update this as same site requirements are solidified
  std::string unescaped = base::UnescapeURLComponent(
      item.GetString(),
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  GURL registration_endpoint = request_url.Resolve(unescaped);
  if (!registration_endpoint.is_valid() ||
      net::SchemefulSite(registration_endpoint) !=
          net::SchemefulSite(request_url)) {
    return std::nullopt;
  }

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  std::string challenge;
  for (const auto& [name, value] : params) {
    // The only boolean parameters are the supported algorithms
    if (value.is_boolean()) {
      if (value.GetBoolean()) {
        std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algo =
            AlgoFromString(name);
        if (algo) {
          supported_algos.push_back(*algo);
        }
      }
    }

    // The only byte sequence parameter is the challenge
    // TODO(kristianm): Add authorization parameter as well
    if (value.is_byte_sequence()) {
      if (name == kChallengeItemKey) {
        challenge = value.GetString();
      }
    }
  }

  if (challenge.empty() || supported_algos.empty()) {
    return {};
  }

  return DeviceBoundSessionRegistrationFetcherParam(
      std::move(registration_endpoint),
      std::move(supported_algos),
      std::move(challenge));
}

std::vector<DeviceBoundSessionRegistrationFetcherParam>
DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  if (!request_url.is_valid()) {
    return {};
  }

  std::string header_value;
  if (!headers ||
      !headers->GetNormalizedHeader(kRegistrationHeaderName, &header_value)) {
    return {};
  }

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(header_value);
  if (!list || list->empty()) {
    return {};
  }

  std::vector<DeviceBoundSessionRegistrationFetcherParam> params;
  for (const auto& item : *list) {
    // Header spec does not support inner lists
    if (item.member_is_inner_list) {
      continue;
    }

    // This is not obvious, the way the Google structured header parser
    // works these params will be considered to belong to the list item
    // and not to the item itself. This is to enable the more intuitive syntax:
    // Sec-Session-Registration: "path1"; challenge=:Y2hhbGxlbmdl:; es256;
    // instead of:
    // Sec-Session-Registration: ("path1"; challenge=:Y2hhbGxlbmdl:; es256;)
    std::optional<DeviceBoundSessionRegistrationFetcherParam> param =
        ParseItem(request_url, item.member[0].item, item.params);
    if (param) {
      params.push_back(std::move(*param));
    }
  }

  return params;
}

}  // namespace net

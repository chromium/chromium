// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/device_bound_session_registration_fetcher_param.h"

#include <vector>

#include "base/base64url.h"
#include "base/logging.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/schemeful_site.h"
#include "net/http/structured_headers.h"

namespace {
// TODO(kristianm): See if these can be used with
// services/network/sec_header_helpers.cc
constexpr char kRegistrationHeaderName[] = "Sec-Session-Registration";
constexpr char kChallengeParamKey[] = "challenge";
constexpr char kPathParamKey[] = "path";

constexpr char kES256[] = "ES256";
constexpr char kRS256[] = "RS256";

std::optional<crypto::SignatureVerifier::SignatureAlgorithm> AlgoFromString(
    const std::string_view& algo) {
  if (algo == kES256) {
    return crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  if (algo == kRS256) {
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
    const structured_headers::ParameterizedMember& session_registration) {
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  for (const auto& algo_token : session_registration.member) {
    if (algo_token.item.is_token()) {
      std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algo =
          AlgoFromString(algo_token.item.GetString());
      if (algo) {
        supported_algos.push_back(*algo);
      };
    }
  }
  if (supported_algos.empty()) {
    return std::nullopt;
  }

  GURL registration_endpoint;
  std::string challenge;
  for (const auto& param : session_registration.params) {
    // The keys for the parameters are unique and must be lower case.
    // Quiche (https://quiche.googlesource.com/quiche), used here,
    // will currently pick the last if there is more than one.
    // TODO(kristianm): Add authorization parameter as well
    if (param.first == kPathParamKey) {
      if (!param.second.is_string()) {
        continue;
      }
      std::string path = param.second.GetString();
      // TODO(kristianm): Update this as same site requirements are solidified
      std::string unescaped = base::UnescapeURLComponent(
          path,
          base::UnescapeRule::PATH_SEPARATORS |
              base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
      GURL candidate_endpoint = request_url.Resolve(unescaped);
      if (candidate_endpoint.is_valid() &&
          net::SchemefulSite(candidate_endpoint) ==
              net::SchemefulSite(request_url)) {
        registration_endpoint = std::move(candidate_endpoint);
      }
      continue;
    }

    if (param.first == kChallengeParamKey) {
      if (!param.second.is_string()) {
        continue;
      }
      challenge = param.second.GetString();
    }
    // Other params are ignored
  }

  if (!registration_endpoint.is_valid() || challenge.empty()) {
    return std::nullopt;
  }

  return DeviceBoundSessionRegistrationFetcherParam(
      std::move(registration_endpoint), std::move(supported_algos),
      std::move(challenge));
}

std::vector<DeviceBoundSessionRegistrationFetcherParam>
DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params;
  if (!request_url.is_valid()) {
    return params;
  }

  std::string header_value;
  if (!headers ||
      !headers->GetNormalizedHeader(kRegistrationHeaderName, &header_value)) {
    return params;
  }

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(header_value);
  if (!list || list->empty()) {
    return params;
  }

  for (const auto& item : *list) {
    if (item.member_is_inner_list) {
      std::optional<DeviceBoundSessionRegistrationFetcherParam> fetcher_param =
          ParseItem(request_url, item);
      if (fetcher_param) {
        params.push_back(std::move(*fetcher_param));
      }
    }
  }

  return params;
}

// static
DeviceBoundSessionRegistrationFetcherParam
DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::string challenge) {
  return DeviceBoundSessionRegistrationFetcherParam(
      std::move(registration_endpoint), std::move(supported_algos),
      std::move(challenge));
}

}  // namespace net

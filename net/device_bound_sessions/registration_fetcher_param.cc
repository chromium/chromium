// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher_param.h"

#include <vector>

#include "base/base64url.h"
#include "base/logging.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/http/structured_headers.h"

namespace {

constexpr char kRegistrationHeaderName[] = "Secure-Session-Registration";
constexpr char kChallengeParamKey[] = "challenge";
constexpr char kPathParamKey[] = "path";
constexpr char kAuthCodeParamKey[] = "authorization";
constexpr char kProviderKeyParamKey[] = "provider_key";
constexpr char kProviderUrlParamKey[] = "provider_url";
constexpr char kProviderSessionIdParamKey[] = "provider_session_id";

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

namespace net::device_bound_sessions {

RegistrationFetcherParam::RegistrationFetcherParam(
    RegistrationFetcherParam&& other) = default;

RegistrationFetcherParam& RegistrationFetcherParam::operator=(
    RegistrationFetcherParam&& other) noexcept = default;

RegistrationFetcherParam::~RegistrationFetcherParam() = default;

RegistrationFetcherParam::RegistrationFetcherParam(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::string challenge,
    std::optional<std::string> authorization,
    std::optional<std::string> provider_key,
    std::optional<GURL> provider_url,
    std::optional<Session::Id> provider_session_id)
    : registration_endpoint_(std::move(registration_endpoint)),
      supported_algos_(std::move(supported_algos)),
      challenge_(std::move(challenge)),
      authorization_(std::move(authorization)),
      provider_key_(std::move(provider_key)),
      provider_url_(std::move(provider_url)),
      provider_session_id_(std::move(provider_session_id)) {}

std::optional<RegistrationFetcherParam> RegistrationFetcherParam::ParseItem(
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
  std::optional<std::string> authorization;
  std::optional<std::string> provider_key;
  std::optional<GURL> provider_url;
  std::optional<Session::Id> provider_session_id;
  for (const auto& [key, value] : session_registration.params) {
    // The keys for the parameters are unique and must be lower case.
    // Quiche (https://quiche.googlesource.com/quiche), used here,
    // will currently pick the last if there is more than one.
    if (key == kPathParamKey) {
      if (!value.is_string()) {
        continue;
      }
      std::string unescaped_path = base::UnescapeURLComponent(
          value.GetString(),
          base::UnescapeRule::PATH_SEPARATORS |
              base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
      // Registration endpoint can be a full URL (samesite with request origin)
      // or a relative URL, starting with a "/" to make it origin-relative,
      // and starting with anything else making it current-path-relative to
      // request URL.
      GURL candidate_registration_endpoint =
          request_url.Resolve(unescaped_path);
      if (candidate_registration_endpoint.is_valid() &&
          IsSecure(candidate_registration_endpoint) &&
          net::SchemefulSite::IsSameSite(candidate_registration_endpoint,
                                         request_url)) {
        registration_endpoint = std::move(candidate_registration_endpoint);
      }
    } else if (key == kChallengeParamKey && value.is_string()) {
      challenge = value.GetString();
    } else if (key == kAuthCodeParamKey && value.is_string()) {
      authorization = value.GetString();
    } else if (key == kProviderKeyParamKey && value.is_string()) {
      provider_key = value.GetString();
    } else if (key == kProviderUrlParamKey && value.is_string()) {
      provider_url = GURL(value.GetString());
    } else if (key == kProviderSessionIdParamKey && value.is_string()) {
      provider_session_id = Session::Id(value.GetString());
    }

    // Other params are ignored
  }

  if (!registration_endpoint.is_valid() || challenge.empty()) {
    return std::nullopt;
  }

  if (provider_key.has_value() != provider_url.has_value() ||
      provider_key.has_value() != provider_session_id.has_value()) {
    return std::nullopt;
  }

  if (provider_url.has_value() &&
      (!provider_url->is_valid() || !IsSecure(*provider_url))) {
    return std::nullopt;
  }

  return RegistrationFetcherParam(
      std::move(registration_endpoint), std::move(supported_algos),
      std::move(challenge), std::move(authorization), std::move(provider_key),
      std::move(provider_url), std::move(provider_session_id));
}

std::vector<RegistrationFetcherParam> RegistrationFetcherParam::CreateIfValid(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  std::vector<RegistrationFetcherParam> params;
  if (!request_url.is_valid()) {
    return params;
  }

  if (!headers) {
    return params;
  }
  std::optional<std::string> header_value =
      headers->GetNormalizedHeader(kRegistrationHeaderName);
  if (!header_value) {
    return params;
  }

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(*header_value);
  if (!list || list->empty()) {
    return params;
  }

  for (const auto& item : *list) {
    if (item.member_is_inner_list) {
      std::optional<RegistrationFetcherParam> fetcher_param =
          ParseItem(request_url, item);
      if (fetcher_param) {
        params.push_back(std::move(*fetcher_param));
      }
    }
  }

  return params;
}

// static
RegistrationFetcherParam RegistrationFetcherParam::CreateInstanceForTesting(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::string challenge,
    std::optional<std::string> authorization,
    std::optional<std::string> provider_key,
    std::optional<GURL> provider_url,
    std::optional<Session::Id> provider_session_id) {
  return RegistrationFetcherParam(
      std::move(registration_endpoint), std::move(supported_algos),
      std::move(challenge), std::move(authorization), std::move(provider_key),
      std::move(provider_url), std::move(provider_session_id));
}

}  // namespace net::device_bound_sessions

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher_param.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/escape.h"
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
constexpr char kAikRequiredParamKey[] = "aik_required";

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

std::vector<crypto::SignatureVerifier::SignatureAlgorithm>
ParseSupportedAlgorithms(
    const std::vector<net::structured_headers::ParameterizedItem>& member) {
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  for (const auto& algo_token : member) {
    if (const std::string* token = algo_token.item.GetIfToken()) {
      std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algo =
          AlgoFromString(*token);
      if (algo) {
        supported_algos.push_back(*algo);
      }
    }
  }
  return supported_algos;
}

GURL ResolveRegistrationEndpoint(const GURL& request_url,
                                 const std::string& path) {
  std::string unescaped_path = base::UnescapeURLComponent(
      path, base::UnescapeRule::PATH_SEPARATORS |
                base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
  // Registration endpoint can be a full URL (samesite with request origin)
  // or a relative URL, starting with a "/" to make it origin-relative,
  // and starting with anything else making it current-path-relative to
  // request URL.
  GURL candidate_registration_endpoint = request_url.Resolve(unescaped_path);
  if (candidate_registration_endpoint.is_valid() &&
      net::device_bound_sessions::IsSecure(candidate_registration_endpoint) &&
      net::SchemefulSite::IsSameSite(candidate_registration_endpoint,
                                     request_url)) {
    return candidate_registration_endpoint;
  }
  return GURL();
}

struct ProviderRegistrationParseResult {
  bool is_valid = false;
  std::optional<net::device_bound_sessions::ProviderRegistrationParams> params;
};

ProviderRegistrationParseResult ParseProviderRegistrationParams(
    std::optional<std::string> provider_key,
    std::optional<GURL> provider_url,
    std::optional<net::device_bound_sessions::Session::Id>
        provider_session_id) {
  // `provider_key` and `provider_url` must either both be present or
  // both be absent.
  if (provider_key.has_value() != provider_url.has_value()) {
    return {.is_valid = false};
  }

  if (base::FeatureList::IsEnabled(
          net::features::kDeviceBoundSessionsForSingleSignOn)) {
    // In SSO scenarios, `provider_session_id` can be absent.
    // However, if `provider_session_id` is present, then `provider_key`
    // (and by extension `provider_url`) must also be present.
    if (provider_session_id.has_value() && !provider_key.has_value()) {
      return {.is_valid = false};
    }
  } else {
    // In non-SSO scenarios, `provider_session_id` must be present
    // if and only if `provider_key` is present.
    if (provider_session_id.has_value() != provider_key.has_value()) {
      return {.is_valid = false};
    }
  }

  if (provider_url.has_value() &&
      (!provider_url->is_valid() ||
       !net::device_bound_sessions::IsSecure(*provider_url))) {
    return {.is_valid = false};
  }

  if (!provider_key.has_value()) {
    return {.is_valid = true, .params = std::nullopt};
  }

  return {.is_valid = true,
          .params = net::device_bound_sessions::ProviderRegistrationParams{
              .provider_key = std::move(*provider_key),
              .provider_url = std::move(*provider_url),
              .provider_session_id = std::move(provider_session_id)}};
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
    url::Origin referring_origin,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization,
    std::optional<ProviderRegistrationParams> provider_params,
    AttestationMode attestation_mode)
    : registration_endpoint_(std::move(registration_endpoint)),
      referring_origin_(std::move(referring_origin)),
      supported_algos_(std::move(supported_algos)),
      challenge_(std::move(challenge)),
      authorization_(std::move(authorization)),
      provider_params_(std::move(provider_params)),
      attestation_mode_(attestation_mode) {}

std::optional<RegistrationFetcherParam> RegistrationFetcherParam::ParseItem(
    const GURL& request_url,
    const structured_headers::ParameterizedMember& session_registration) {
  const auto inner_list_and_params =
      session_registration.GetWithParamsIfInnerList();
  if (!inner_list_and_params.has_value()) {
    return std::nullopt;
  }

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos =
      ParseSupportedAlgorithms(inner_list_and_params->first);
  if (supported_algos.empty()) {
    return std::nullopt;
  }

  GURL registration_endpoint;
  std::optional<std::string> challenge;
  std::optional<std::string> authorization;
  std::optional<std::string> provider_key;
  std::optional<GURL> provider_url;
  std::optional<Session::Id> provider_session_id;
  bool aik_required = false;
  for (const auto& [key, value] : inner_list_and_params->second) {
    // The keys for the parameters are unique and must be lower case.
    // Quiche (https://quiche.googlesource.com/quiche), used here,
    // will currently pick the last if there is more than one.
    if (key == kPathParamKey) {
      const std::string* string = value.GetIfString();
      if (!string) {
        return std::nullopt;
      }
      registration_endpoint = ResolveRegistrationEndpoint(request_url, *string);
    } else if (key == kChallengeParamKey) {
      const std::string* string = value.GetIfString();
      if (!string) {
        return std::nullopt;
      }
      challenge = *string;
    } else if (key == kAuthCodeParamKey) {
      const std::string* string = value.GetIfString();
      if (!string) {
        return std::nullopt;
      }
      authorization = *string;
    } else if (key == kProviderKeyParamKey) {
      const std::string* string = value.GetIfString();
      if (!string) {
        return std::nullopt;
      }
      provider_key = *string;
    } else if (key == kProviderUrlParamKey) {
      const std::string* string = value.GetIfString();
      if (!string) {
        return std::nullopt;
      }
      provider_url = GURL(*string);
    } else if (key == kProviderSessionIdParamKey) {
      const std::string* string = value.GetIfString();
      if (!string) {
        return std::nullopt;
      }
      provider_session_id = Session::Id(*string);
    } else if (key == kAikRequiredParamKey &&
               base::FeatureList::IsEnabled(
                   features::kDeviceBoundSessionsForSingleSignOn)) {
      const bool* boolean = value.GetIfBoolean();
      if (!boolean) {
        return std::nullopt;
      }
      aik_required = *boolean;
    }

    // Other params are ignored
  }

  if (!registration_endpoint.is_valid()) {
    return std::nullopt;
  }

  if (aik_required && !challenge.has_value()) {
    return std::nullopt;
  }

  auto [is_valid, provider_params] = ParseProviderRegistrationParams(
      std::move(provider_key), std::move(provider_url),
      std::move(provider_session_id));
  if (!is_valid) {
    return std::nullopt;
  }

  return RegistrationFetcherParam(
      std::move(registration_endpoint), url::Origin::Create(request_url),
      std::move(supported_algos), std::move(challenge),
      std::move(authorization), std::move(provider_params),
      aik_required ? AttestationMode::kRequired : AttestationMode::kNone);
}

std::vector<RegistrationFetcherParam> RegistrationFetcherParam::CreateIfValid(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers,
    const std::vector<SchemefulSite>& restricted_sites) {
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

  SchemefulSite site(request_url);
  if (std::ranges::contains(restricted_sites, site) &&
      !base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionsForRestrictedSites)) {
    return params;
  }

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(*header_value);
  if (!list || list->empty()) {
    return params;
  }

  for (const auto& item : *list) {
    std::optional<RegistrationFetcherParam> fetcher_param =
        ParseItem(request_url, item);
    if (fetcher_param) {
      params.push_back(std::move(*fetcher_param));
    }
  }

  return params;
}

// static
RegistrationFetcherParam RegistrationFetcherParam::CreateInstanceForTesting(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::optional<std::string> challenge,
    std::optional<std::string> authorization,
    std::optional<ProviderRegistrationParams> provider_params,
    AttestationMode attestation_mode,
    std::optional<url::Origin> maybe_referring_origin) {
  url::Origin referring_origin =
      maybe_referring_origin ? std::move(*maybe_referring_origin)
                             : url::Origin::Create(registration_endpoint);
  return RegistrationFetcherParam(
      std::move(registration_endpoint), std::move(referring_origin),
      std::move(supported_algos), std::move(challenge),
      std::move(authorization), std::move(provider_params), attestation_mode);
}

}  // namespace net::device_bound_sessions

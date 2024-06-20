// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_provider_fetcher.h"

#include "base/check.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"

namespace content {

namespace {

// Maximum number of provider URLs in the well-known file.
// TODO(cbiesinger): Determine what the right number is.
static constexpr size_t kMaxProvidersInWellKnownFile = 1ul;

void SetError(FederatedProviderFetcher::FetchResult& fetch_result,
              blink::mojom::FederatedAuthRequestResult result,
              content::FedCmRequestIdTokenStatus token_status,
              std::optional<std::string> additional_console_error_message) {
  fetch_result.error = FederatedProviderFetcher::FetchError(
      result, token_status, additional_console_error_message);
}

}  // namespace

using blink::mojom::FederatedAuthRequestResult;
using TokenStatus = FedCmRequestIdTokenStatus;

FederatedProviderFetcher::FetchError::FetchError(const FetchError&) = default;

FederatedProviderFetcher::FetchError::FetchError(
    blink::mojom::FederatedAuthRequestResult result,
    FedCmRequestIdTokenStatus token_status,
    std::optional<std::string> additional_console_error_message)
    : result(result),
      token_status(token_status),
      additional_console_error_message(
          std::move(additional_console_error_message)) {}

FederatedProviderFetcher::FetchError::~FetchError() = default;

FederatedProviderFetcher::FetchResult::FetchResult() = default;
FederatedProviderFetcher::FetchResult::FetchResult(const FetchResult&) =
    default;
FederatedProviderFetcher::FetchResult::~FetchResult() = default;

FederatedProviderFetcher::FederatedProviderFetcher(
    RenderFrameHost& render_frame_host,
    IdpNetworkRequestManager* network_manager)
    : render_frame_host_(render_frame_host),
      network_manager_(network_manager) {}

FederatedProviderFetcher::~FederatedProviderFetcher() = default;

void FederatedProviderFetcher::Start(
    const std::set<GURL>& identity_provider_config_urls,
    blink::mojom::RpMode rp_mode,
    int icon_ideal_size,
    int icon_minimum_size,
    RequesterCallback callback) {
  callback_ = std::move(callback);

  for (const GURL& identity_provider_config_url :
       identity_provider_config_urls) {
    FetchResult fetch_result;
    fetch_result.identity_provider_config_url = identity_provider_config_url;
    fetch_results_.push_back(std::move(fetch_result));

    pending_well_known_fetches_.insert(identity_provider_config_url);
    pending_config_fetches_.insert(identity_provider_config_url);
  }

  // In a separate loop to avoid invalidating references when adding elements to
  // `fetch_results_`.
  for (FetchResult& fetch_result : fetch_results_) {
    network_manager_->FetchWellKnown(
        fetch_result.identity_provider_config_url,
        base::BindOnce(&FederatedProviderFetcher::OnWellKnownFetched,
                       weak_ptr_factory_.GetWeakPtr(), std::ref(fetch_result)));
    network_manager_->FetchConfig(
        fetch_result.identity_provider_config_url, rp_mode, icon_ideal_size,
        icon_minimum_size,
        base::BindOnce(&FederatedProviderFetcher::OnConfigFetched,
                       weak_ptr_factory_.GetWeakPtr(), std::ref(fetch_result)));
  }
}

void FederatedProviderFetcher::OnWellKnownFetched(
    FetchResult& fetch_result,
    IdpNetworkRequestManager::FetchStatus status,
    const IdpNetworkRequestManager::WellKnown& well_known) {
  pending_well_known_fetches_.erase(fetch_result.identity_provider_config_url);

  constexpr char kWellKnownFileStr[] = "well-known file";

  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess &&
      !ShouldSkipWellKnownEnforcementForIdp(
          fetch_result.identity_provider_config_url)) {
    std::optional<std::string> additional_console_error_message =
        webid::ComputeConsoleMessageForHttpResponseCode(kWellKnownFileStr,
                                                        status.response_code);

    switch (status.parse_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kWellKnownHttpNotFound,
                TokenStatus::kWellKnownHttpNotFound,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
        OnError(fetch_result, FederatedAuthRequestResult::kWellKnownNoResponse,
                TokenStatus::kWellKnownNoResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kWellKnownInvalidResponse,
                TokenStatus::kWellKnownInvalidResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
        OnError(fetch_result, FederatedAuthRequestResult::kWellKnownListEmpty,
                TokenStatus::kWellKnownListEmpty,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kWellKnownInvalidContentType,
                TokenStatus::kWellKnownInvalidContentType,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kSuccess: {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  fetch_result.wellknown = std::move(well_known);

  RunCallbackIfDone();
}

void FederatedProviderFetcher::OnConfigFetched(
    FetchResult& fetch_result,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  pending_config_fetches_.erase(fetch_result.identity_provider_config_url);

  constexpr char kConfigFileStr[] = "config file";

  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess) {
    std::optional<std::string> additional_console_error_message =
        webid::ComputeConsoleMessageForHttpResponseCode(kConfigFileStr,
                                                        status.response_code);

    switch (status.parse_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
        OnError(fetch_result, FederatedAuthRequestResult::kConfigHttpNotFound,
                TokenStatus::kConfigHttpNotFound,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
        OnError(fetch_result, FederatedAuthRequestResult::kConfigNoResponse,
                TokenStatus::kConfigNoResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kConfigInvalidResponse,
                TokenStatus::kConfigInvalidResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kConfigInvalidContentType,
                TokenStatus::kConfigInvalidContentType,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
        NOTREACHED_IN_MIGRATION()
            << "kEmptyListError is undefined for OnConfigFetched";
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kSuccess: {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }

  fetch_result.endpoints = endpoints;
  fetch_result.metadata = idp_metadata;

  RunCallbackIfDone();
}

void FederatedProviderFetcher::OnError(
    FetchResult& fetch_result,
    blink::mojom::FederatedAuthRequestResult result,
    content::FedCmRequestIdTokenStatus token_status,
    std::optional<std::string> additional_console_error_message) {
  SetError(fetch_result, result, token_status,
           additional_console_error_message);
  RunCallbackIfDone();
}

void FederatedProviderFetcher::ValidateAndMaybeSetError(FetchResult& result) {
  // This function validates fetch results, by analyzing the config file and
  // the well-known file.
  // If the validation fails, this function sets the "error" property in the
  // result.

  if (result.error) {
    // A fetch error was already recorded earlier (e.g. a network error).
    return;
  }

  // Validates the config file. A valid config file must have:
  //
  // (a) a token endpoint same-origin url
  // (b) an accounts endpoint same-origin url
  // (c) optionally, a login_url same-origin url
  //
  // If one of these conditions are not met (e.g. one of the urls are not
  // valid), we consider the config file invalid.

  bool is_token_valid = webid::IsEndpointSameOrigin(
      result.identity_provider_config_url, result.endpoints.token);
  bool is_accounts_valid = webid::IsEndpointSameOrigin(
      result.identity_provider_config_url, result.endpoints.accounts);
  url::Origin idp_origin =
      url::Origin::Create(result.identity_provider_config_url);

  bool is_login_url_valid =
      webid::GetIdpSigninStatusMode(render_frame_host_.get(), idp_origin) !=
          FedCmIdpSigninStatusMode::ENABLED ||
      (result.metadata &&
       webid::IsEndpointSameOrigin(result.identity_provider_config_url,
                                   result.metadata->idp_login_url));

  if (!is_token_valid || !is_accounts_valid || !is_login_url_valid) {
    std::string console_message =
        "Config file is missing or has an invalid URL for the following:\n";
    if (!is_token_valid) {
      console_message += "\"id_assertion_endpoint\"\n";
    }
    if (!is_accounts_valid) {
      console_message += "\"accounts_endpoint\"\n";
    }
    if (!is_login_url_valid) {
      console_message += "\"login_url\"\n";
    }

    SetError(result, FederatedAuthRequestResult::kConfigInvalidResponse,
             TokenStatus::kConfigInvalidResponse, console_message);
    return;
  }

  // Validates the well-known file.

  // A well-known is valid if:
  //
  // (a) a chrome://flag has been set to bypass the check or
  // (b) the well-known has an accounts_endpoint/login_url attribute that
  //     match the one in the config file or
  // (c) the well-known file has a providers_url list of size 1 and that
  //     contains the config url passed in the JS call

  // (a)
  if (ShouldSkipWellKnownEnforcementForIdp(
          result.identity_provider_config_url)) {
    return;
  }

  // (b)
  if (webid::IsFedCmAuthzEnabled(*render_frame_host_, idp_origin) &&
      result.wellknown.accounts.is_valid() &&
      result.wellknown.login_url.is_valid() && result.metadata &&
      result.metadata->idp_login_url.is_valid()) {
    // Behind the AuthZ flag, it is valid for IdPs to have valid configURLs
    // by announcing their accounts_endpoint and their login_urls in the
    // .well-known file. When that happens, and both of these urls match the
    // contents of their configURLs, the browser knows that they don't
    // contain any extra data embedded in them, so they can used as a valid
    // configURL without checking for its presence in the provider_urls array.
    if (result.endpoints.accounts != result.wellknown.accounts ||
        result.metadata->idp_login_url != result.wellknown.login_url) {
      SetError(result, FederatedAuthRequestResult::kConfigInvalidResponse,
               TokenStatus::kConfigInvalidResponse,
               "The well-known file contains an accounts endpoint or login_url "
               "that doesn't match the one in the configURL");
    }
    return;
  }

  // (c)
  //
  // The config url from the API call:
  // navigator.credentials.get({
  //   federated: {
  //     providers: [{
  //       configURL: "https://foo.idp.example/fedcm.json",
  //       clientId: "1234"
  //     }],
  //   }
  // });
  //
  // must match the one in the well-known file:
  //
  // {
  //   "provider_urls": [
  //     "https://foo.idp.example/fedcm.json"
  //   ]
  // }

  if (result.wellknown.provider_urls.size() > kMaxProvidersInWellKnownFile) {
    SetError(result, FederatedAuthRequestResult::kWellKnownTooBig,
             TokenStatus::kWellKnownTooBig,
             /*additional_console_error_message=*/std::nullopt);
    return;
  }

  bool provider_url_is_valid = (result.wellknown.provider_urls.count(
                                    result.identity_provider_config_url) != 0);

  if (!provider_url_is_valid) {
    SetError(result, FederatedAuthRequestResult::kConfigNotInWellKnown,
             TokenStatus::kConfigNotInWellKnown,
             /*additional_console_error_message=*/std::nullopt);
    return;
  }
}

void FederatedProviderFetcher::RunCallbackIfDone() {
  if (!pending_config_fetches_.empty() ||
      !pending_well_known_fetches_.empty()) {
    return;
  }

  for (auto& result : fetch_results_) {
    ValidateAndMaybeSetError(result);
  }

  std::move(callback_).Run(std::move(fetch_results_));
}

bool FederatedProviderFetcher::ShouldSkipWellKnownEnforcementForIdp(
    const GURL& idp_url) {
  if (IsFedCmWithoutWellKnownEnforcementEnabled()) {
    return true;
  }

  // Skip if RP and IDP are same-site.
  return webid::IsSameSite(render_frame_host_->GetLastCommittedOrigin(),
                           url::Origin::Create(idp_url));
}

}  // namespace content

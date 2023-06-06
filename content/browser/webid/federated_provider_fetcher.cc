// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_provider_fetcher.h"

#include "content/browser/webid/webid_utils.h"

namespace content {

namespace {

// Maximum number of provider URLs in the well-known file.
// TODO(cbiesinger): Determine what the right number is.
static constexpr size_t kMaxProvidersInWellKnownFile = 1ul;

}  // namespace

using blink::mojom::FederatedAuthRequestResult;
using TokenStatus = FedCmRequestIdTokenStatus;

FederatedProviderFetcher::FetchError::FetchError(const FetchError&) = default;

FederatedProviderFetcher::FetchError::FetchError(
    blink::mojom::FederatedAuthRequestResult result,
    FedCmRequestIdTokenStatus token_status,
    absl::optional<std::string> additional_console_error_message)
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
    IdpNetworkRequestManager* network_manager)
    : network_manager_(network_manager) {}

FederatedProviderFetcher::~FederatedProviderFetcher() = default;

void FederatedProviderFetcher::Start(
    const std::set<GURL>& identity_provider_config_urls,
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
        fetch_result.identity_provider_config_url, icon_ideal_size,
        icon_minimum_size,
        base::BindOnce(&FederatedProviderFetcher::OnConfigFetched,
                       weak_ptr_factory_.GetWeakPtr(), std::ref(fetch_result)));
  }
}

void FederatedProviderFetcher::OnWellKnownFetched(
    FetchResult& fetch_result,
    IdpNetworkRequestManager::FetchStatus status,
    const std::set<GURL>& urls) {
  pending_well_known_fetches_.erase(fetch_result.identity_provider_config_url);

  constexpr char kWellKnownFileStr[] = "well-known file";

  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess) {
    absl::optional<std::string> additional_console_error_message =
        webid::ComputeConsoleMessageForHttpResponseCode(kWellKnownFileStr,
                                                        status.response_code);

    switch (status.parse_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound,
                TokenStatus::kWellKnownHttpNotFound,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse,
                TokenStatus::kWellKnownNoResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
        OnError(
            fetch_result,
            FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse,
            TokenStatus::kWellKnownInvalidResponse,
            additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty,
                TokenStatus::kWellKnownListEmpty,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::
                    kErrorFetchingWellKnownInvalidContentType,
                TokenStatus::kWellKnownInvalidContentType,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kSuccess: {
        NOTREACHED();
      }
    }
  }

  if (urls.size() > kMaxProvidersInWellKnownFile) {
    OnError(fetch_result, FederatedAuthRequestResult::kErrorWellKnownTooBig,
            TokenStatus::kWellKnownTooBig,
            /*additional_console_error_message=*/absl::nullopt);
    return;
  }

  // The provider url from the API call:
  // navigator.credentials.get({
  //   federated: {
  //     providers: [{
  //       configURL: "https://foo.idp.example/fedcm.json",
  //       clientId: "1234"
  //     }],
  //   }
  // });
  // must match the one in the well-known file:
  // {
  //   "provider_urls": [
  //     "https://foo.idp.example/fedcm.json"
  //   ]
  // }
  bool provider_url_is_valid =
      (urls.count(fetch_result.identity_provider_config_url) != 0);

  if (!provider_url_is_valid) {
    OnError(fetch_result,
            FederatedAuthRequestResult::kErrorConfigNotInWellKnown,
            TokenStatus::kConfigNotInWellKnown,
            /*additional_console_error_message=*/absl::nullopt);
    return;
  }

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
    absl::optional<std::string> additional_console_error_message =
        webid::ComputeConsoleMessageForHttpResponseCode(kConfigFileStr,
                                                        status.response_code);

    switch (status.parse_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound,
                TokenStatus::kConfigHttpNotFound,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingConfigNoResponse,
                TokenStatus::kConfigNoResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse,
                TokenStatus::kConfigInvalidResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError: {
        OnError(
            fetch_result,
            FederatedAuthRequestResult::kErrorFetchingConfigInvalidContentType,
            TokenStatus::kConfigInvalidContentType,
            additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kEmptyListError: {
        NOTREACHED() << "kEmptyListError is undefined for OnConfigFetched";
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kSuccess: {
        NOTREACHED();
      }
    }
  }

  fetch_result.endpoints = endpoints;

  fetch_result.metadata = idp_metadata;

  bool is_token_valid = webid::IsEndpointSameOrigin(
      fetch_result.identity_provider_config_url, fetch_result.endpoints.token);
  bool is_accounts_valid =
      webid::IsEndpointSameOrigin(fetch_result.identity_provider_config_url,
                                  fetch_result.endpoints.accounts);
  bool is_signin_url_valid =
      idp_metadata.idp_signin_url.is_empty() ||
      webid::IsEndpointSameOrigin(fetch_result.identity_provider_config_url,
                                  idp_metadata.idp_signin_url);
  if (!is_token_valid || !is_accounts_valid || !is_signin_url_valid) {
    std::string console_message =
        "Config file is missing or has an invalid URL for the following:\n";
    if (!is_token_valid) {
      console_message += "\"id_assertion_endpoint\"\n";
    }
    if (!is_accounts_valid) {
      console_message += "\"accounts_endpoint\"\n";
    }
    if (!is_signin_url_valid) {
      console_message += "\"signin_url\"\n";
    }

    OnError(fetch_result,
            FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse,
            TokenStatus::kConfigInvalidResponse, console_message);
    return;
  }

  RunCallbackIfDone();
}

void FederatedProviderFetcher::OnError(
    FetchResult& fetch_result,
    blink::mojom::FederatedAuthRequestResult result,
    content::FedCmRequestIdTokenStatus token_status,
    absl::optional<std::string> additional_console_error_message) {
  fetch_result.error =
      FetchError(result, token_status, additional_console_error_message);
  RunCallbackIfDone();
}

void FederatedProviderFetcher::RunCallbackIfDone() {
  if (pending_config_fetches_.empty() && pending_well_known_fetches_.empty()) {
    std::move(callback_).Run(std::move(fetch_results_));
  }
}

}  // namespace content

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_manifest_requester.h"

#include "content/browser/webid/webid_utils.h"

namespace content {

namespace {

// Maximum number of provider URLs in the manifest list.
// TODO(cbiesinger): Determine what the right number is.
static constexpr size_t kMaxProvidersInWellKnownFile = 1ul;

}  // namespace

using blink::mojom::FederatedAuthRequestResult;
using TokenStatus = FedCmRequestIdTokenStatus;

FederatedManifestRequester::FetchError::FetchError(const FetchError&) = default;

FederatedManifestRequester::FetchError::FetchError(
    blink::mojom::FederatedAuthRequestResult result,
    FedCmRequestIdTokenStatus token_status,
    absl::optional<std::string> additional_console_error_message)
    : result(result),
      token_status(token_status),
      additional_console_error_message(
          std::move(additional_console_error_message)) {}

FederatedManifestRequester::FetchError::~FetchError() = default;

FederatedManifestRequester::FetchResult::FetchResult() = default;
FederatedManifestRequester::FetchResult::FetchResult(const FetchResult&) =
    default;
FederatedManifestRequester::FetchResult::~FetchResult() = default;

FederatedManifestRequester::FederatedManifestRequester(
    IdpNetworkRequestManager* network_manager)
    : network_manager_(network_manager) {}

FederatedManifestRequester::~FederatedManifestRequester() = default;

void FederatedManifestRequester::Start(
    const std::vector<GURL>& identity_provider_config_urls,
    int icon_ideal_size,
    int icon_minimum_size,
    RequesterCallback callback) {
  callback_ = std::move(callback);

  for (const GURL& identity_provider_config_url :
       identity_provider_config_urls) {
    FetchResult fetch_result;
    fetch_result.identity_provider_config_url = identity_provider_config_url;
    fetch_results_.push_back(std::move(fetch_result));

    pending_manifest_list_fetches_.insert(identity_provider_config_url);
    pending_manifest_fetches_.insert(identity_provider_config_url);
  }

  // In a separate loop to avoid invalidating references when adding elements to
  // `fetch_results_`.
  for (FetchResult& fetch_result : fetch_results_) {
    network_manager_->FetchManifestList(
        fetch_result.identity_provider_config_url,
        base::BindOnce(&FederatedManifestRequester::OnManifestListFetched,
                       weak_ptr_factory_.GetWeakPtr(), std::ref(fetch_result)));
    network_manager_->FetchManifest(
        fetch_result.identity_provider_config_url, icon_ideal_size,
        icon_minimum_size,
        base::BindOnce(&FederatedManifestRequester::OnManifestFetched,
                       weak_ptr_factory_.GetWeakPtr(), std::ref(fetch_result)));
  }
}

void FederatedManifestRequester::OnManifestListFetched(
    FetchResult& fetch_result,
    IdpNetworkRequestManager::FetchStatus status,
    const std::set<GURL>& urls) {
  pending_manifest_list_fetches_.erase(
      fetch_result.identity_provider_config_url);

  constexpr char kWellKnownFileStr[] = "well-known file";

  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess) {
    absl::optional<std::string> additional_console_error_message =
        webid::ComputeConsoleMessageForHttpResponseCode(kWellKnownFileStr,
                                                        status.response_code);

    switch (status.parse_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound,
                TokenStatus::kManifestListHttpNotFound,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse,
                TokenStatus::kManifestListNoResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
        OnError(
            fetch_result,
            FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse,
            TokenStatus::kManifestListInvalidResponse,
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
            TokenStatus::kManifestListTooBig,
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
  // must match the one in the manifest list:
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
            TokenStatus::kManifestNotInManifestList,
            /*additional_console_error_message=*/absl::nullopt);
    return;
  }

  RunCallbackIfDone();
}

void FederatedManifestRequester::OnManifestFetched(
    FetchResult& fetch_result,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::Endpoints endpoints,
    IdentityProviderMetadata idp_metadata) {
  pending_manifest_fetches_.erase(fetch_result.identity_provider_config_url);

  constexpr char kConfigFileStr[] = "config file";

  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess) {
    absl::optional<std::string> additional_console_error_message =
        webid::ComputeConsoleMessageForHttpResponseCode(kConfigFileStr,
                                                        status.response_code);

    switch (status.parse_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound,
                TokenStatus::kManifestHttpNotFound,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingConfigNoResponse,
                TokenStatus::kManifestNoResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError: {
        OnError(fetch_result,
                FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse,
                TokenStatus::kManifestInvalidResponse,
                additional_console_error_message);
        return;
      }
      case IdpNetworkRequestManager::ParseStatus::kSuccess: {
        NOTREACHED();
      }
    }
  }

  fetch_result.endpoints = endpoints;

  fetch_result.metadata = idp_metadata;

  bool is_token_valid = webid::IsEndpointUrlValid(
      fetch_result.identity_provider_config_url, fetch_result.endpoints.token);
  bool is_accounts_valid =
      webid::IsEndpointUrlValid(fetch_result.identity_provider_config_url,
                                fetch_result.endpoints.accounts);
  if (!is_token_valid || !is_accounts_valid) {
    std::string console_message =
        "Config file is missing or has an invalid URL for the following "
        "endpoints:\n";
    if (!is_token_valid) {
      console_message += "\"id_assertion_endpoint\"\n";
    }
    if (!is_accounts_valid) {
      console_message += "\"accounts_endpoint\"\n";
    }

    OnError(fetch_result,
            FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse,
            TokenStatus::kManifestInvalidResponse, console_message);
    return;
  }

  RunCallbackIfDone();
}

void FederatedManifestRequester::OnError(
    FetchResult& fetch_result,
    blink::mojom::FederatedAuthRequestResult result,
    content::FedCmRequestIdTokenStatus token_status,
    absl::optional<std::string> additional_console_error_message) {
  fetch_result.error =
      FetchError(result, token_status, additional_console_error_message);
  RunCallbackIfDone();
}

void FederatedManifestRequester::RunCallbackIfDone() {
  if (pending_manifest_fetches_.empty() &&
      pending_manifest_list_fetches_.empty()) {
    std::move(callback_).Run(std::move(fetch_results_));
  }
}

}  // namespace content

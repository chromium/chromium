// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_accounts_fetcher.h"

#include <set>

#include "base/containers/contains.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

using ::blink::mojom::FederatedAuthRequestResult;
using TokenStatus = content::FedCmRequestIdTokenStatus;

namespace {
static constexpr char kVcSdJwt[] = "vc+sd-jwt";
}  // namespace

namespace content {

FedCmAccountsFetcher::IdentityProviderGetInfo::IdentityProviderGetInfo(
    blink::mojom::IdentityProviderRequestOptionsPtr provider,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    std::optional<blink::mojom::Format> format)
    : provider(std::move(provider)),
      rp_context(rp_context),
      rp_mode(rp_mode),
      format(format) {}

FedCmAccountsFetcher::IdentityProviderGetInfo::~IdentityProviderGetInfo() =
    default;
FedCmAccountsFetcher::IdentityProviderGetInfo::IdentityProviderGetInfo(
    const IdentityProviderGetInfo& other) {
  *this = other;
}

FedCmAccountsFetcher::IdentityProviderGetInfo&
FedCmAccountsFetcher::IdentityProviderGetInfo::operator=(
    const IdentityProviderGetInfo& other) {
  provider = other.provider->Clone();
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  format = other.format;
  return *this;
}

FedCmAccountsFetcher::FedCmAccountsFetcher(
    RenderFrameHost& render_frame_host,
    IdpNetworkRequestManager* network_manager,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    RpMode rp_mode,
    FederatedAuthRequestImpl* federated_auth_request_impl)
    : render_frame_host_(render_frame_host),
      network_manager_(network_manager),
      permission_delegate_(permission_delegate),
      rp_mode_(rp_mode),
      federated_auth_request_impl_(federated_auth_request_impl) {}

FedCmAccountsFetcher::~FedCmAccountsFetcher() = default;

void FedCmAccountsFetcher::FetchEndpointsForIdps(
    const std::set<GURL>& idp_config_urls,
    int icon_ideal_size,
    int icon_minimum_size) {
  std::vector<FederatedProviderFetcher::FetchRequest> idps;
  base::flat_map<GURL, IdentityProviderGetInfo>& token_request_get_infos =
      federated_auth_request_impl_->GetTokenRequestGetInfos();
  for (const auto& idp : idp_config_urls) {
    auto idp_get = token_request_get_infos.find(idp);
    CHECK(idp_get != token_request_get_infos.end());
    idps.emplace_back(
        idp, idp_get->second.provider->config->from_idp_registration_api);
  }

  provider_fetcher_ = std::make_unique<FederatedProviderFetcher>(
      *render_frame_host_, network_manager_);
  provider_fetcher_->Start(
      idps, rp_mode_, icon_ideal_size, icon_minimum_size,
      base::BindOnce(&FedCmAccountsFetcher::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FedCmAccountsFetcher::OnAllConfigAndWellKnownFetched(
    std::vector<FederatedProviderFetcher::FetchResult> fetch_results) {
  provider_fetcher_.reset();

  base::TimeTicks well_known_and_config_fetched_time = base::TimeTicks::Now();
  federated_auth_request_impl_->SetWellKnownAndConfigFetchedTime(
      well_known_and_config_fetched_time);

  base::flat_map<GURL, IdentityProviderGetInfo>& token_request_get_infos =
      federated_auth_request_impl_->GetTokenRequestGetInfos();
  for (const FederatedProviderFetcher::FetchResult& fetch_result :
       fetch_results) {
    const GURL& identity_provider_config_url =
        fetch_result.identity_provider_config_url;
    auto get_info_it =
        token_request_get_infos.find(identity_provider_config_url);
    CHECK(get_info_it != token_request_get_infos.end());

    federated_auth_request_impl_->SetMetricsEndpoint(
        identity_provider_config_url, fetch_result.endpoints.metrics);

    std::unique_ptr<IdentityProviderInfo> idp_info =
        std::make_unique<IdentityProviderInfo>(
            get_info_it->second.provider, std::move(fetch_result.endpoints),
            fetch_result.metadata ? std::move(*fetch_result.metadata)
                                  : IdentityProviderMetadata(),
            get_info_it->second.rp_context, get_info_it->second.rp_mode,
            get_info_it->second.format);

    if (fetch_result.error) {
      const FederatedProviderFetcher::FetchError& fetch_error =
          *fetch_result.error;
      if (fetch_error.additional_console_error_message) {
        render_frame_host_->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            *fetch_error.additional_console_error_message);
      }
      federated_auth_request_impl_->OnFetchDataForIdpFailed(
          std::move(idp_info), fetch_error.result, fetch_error.token_status,
          /*should_delay_callback=*/rp_mode_ == RpMode::kPassive);
      continue;
    }

    if (IsFedCmIdPRegistrationEnabled()) {
      if (get_info_it->second.provider->config->type) {
        if (!base::Contains(fetch_result.metadata->types,
                            get_info_it->second.provider->config->type)) {
          federated_auth_request_impl_->OnFetchDataForIdpFailed(
              std::move(idp_info), FederatedAuthRequestResult::kTypeNotMatching,
              TokenStatus::kConfigNotMatchingType,
              /*should_delay_callback=*/false);
          continue;
        }
      }
    }

    if (get_info_it->second.provider->format) {
      // If a token format was specified, make sure that the configURL
      // supports it as well as the feature is enabled.
      if (!IsFedCmDelegationEnabled() ||
          !base::Contains(fetch_result.metadata->formats, kVcSdJwt)) {
        federated_auth_request_impl_->OnFetchDataForIdpFailed(
            std::move(idp_info),
            FederatedAuthRequestResult::kConfigInvalidResponse,
            TokenStatus::kConfigInvalidResponse,
            /*should_delay_callback=*/false);
        continue;
      }
    }

    // The login url should be valid unless IdP login status API is disabled.
    if (idp_info->metadata.idp_login_url.is_valid()) {
      federated_auth_request_impl_->SetIdpLoginInfo(
          idp_info->metadata.idp_login_url, idp_info->provider->login_hint,
          idp_info->provider->domain_hint);
    }

    // Make sure that we don't fetch accounts if the IDP sign-in bit is reset to
    // false during the API call. e.g. by the login/logout HEADER.
    // In the active flow we get here even if the IDP sign-in bit was false
    // originally, because we need the well-known and config files to find the
    // login URL.
    idp_info->has_failing_idp_signin_status =
        webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
            *render_frame_host_, identity_provider_config_url,
            permission_delegate_);
    if (idp_info->has_failing_idp_signin_status) {
      // If the user is logged out and we are in a active-mode, allow the user
      // to sign-in to the IdP and return early.
      if (rp_mode_ == blink::mojom::RpMode::kActive) {
        federated_auth_request_impl_->MaybeShowActiveModeModalDialog(
            identity_provider_config_url, idp_info->metadata.idp_login_url);
        return;
      }
      // Do not send metrics for IDP where the user is not signed-in in order
      // to prevent IDP from using the user IP to make a probabilistic model
      // of which websites a user visits.
      idp_info->endpoints.metrics = GURL();

      federated_auth_request_impl_->OnFetchDataForIdpFailed(
          std::move(idp_info), FederatedAuthRequestResult::kNotSignedInWithIdp,
          TokenStatus::kNotSignedInWithIdp,
          /*should_delay_callback=*/true);
      continue;
    }

    GURL accounts_endpoint = idp_info->endpoints.accounts;
    std::string client_id = idp_info->provider->config->client_id;
    const GURL& config_url = idp_info->provider->config->config_url;
    federated_auth_request_impl_->SendAccountsRequest(
        std::move(idp_info), config_url, accounts_endpoint, client_id);
  }
}

}  // namespace content

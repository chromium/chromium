// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_accounts_fetcher.h"

#include <set>

#include "base/containers/contains.h"
#include "content/browser/webid/fedcm_config_fetcher.h"
#include "content/browser/webid/fedcm_mappers.h"
#include "content/browser/webid/federated_auth_request_impl.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

using ::blink::mojom::FederatedAuthRequestResult;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;

namespace content {

namespace {
static constexpr char kVcSdJwt[] = "vc+sd-jwt";

bool IsFrameActive(RenderFrameHost* frame) {
  return frame && frame->IsActive();
}
}  // namespace

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

FedCmAccountsFetcher::FedCmFetchingParams::FedCmFetchingParams(
    blink::mojom::RpMode rp_mode,
    int icon_ideal_size,
    int icon_minimum_size,
    MediationRequirement mediation_requirement)
    : rp_mode(rp_mode),
      icon_ideal_size(icon_ideal_size),
      icon_minimum_size(icon_minimum_size),
      mediation_requirement(mediation_requirement) {}

FedCmAccountsFetcher::FedCmFetchingParams::~FedCmFetchingParams() = default;

FedCmAccountsFetcher::FedCmAccountsFetcher(
    RenderFrameHost& render_frame_host,
    IdpNetworkRequestManager* network_manager,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    FedCmFetchingParams params,
    FederatedAuthRequestImpl* federated_auth_request_impl)
    : render_frame_host_(render_frame_host),
      network_manager_(network_manager),
      api_permission_delegate_(api_permission_delegate),
      permission_delegate_(permission_delegate),
      params_(params),
      federated_auth_request_impl_(federated_auth_request_impl) {}

FedCmAccountsFetcher::~FedCmAccountsFetcher() = default;

void FedCmAccountsFetcher::FetchEndpointsForIdps(
    const std::set<GURL>& idp_config_urls) {
  std::vector<FedCmConfigFetcher::FetchRequest> idps;
  base::flat_map<GURL, IdentityProviderGetInfo>& token_request_get_infos =
      federated_auth_request_impl_->GetTokenRequestGetInfos();
  for (const auto& idp : idp_config_urls) {
    auto idp_get = token_request_get_infos.find(idp);
    CHECK(idp_get != token_request_get_infos.end());
    idps.emplace_back(
        idp, idp_get->second.provider->config->from_idp_registration_api);
  }

  config_fetcher_ = std::make_unique<FedCmConfigFetcher>(*render_frame_host_,
                                                         network_manager_);
  config_fetcher_->Start(
      idps, params_.rp_mode, params_.icon_ideal_size, params_.icon_minimum_size,
      base::BindOnce(&FedCmAccountsFetcher::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FedCmAccountsFetcher::SendAllFailedTokenRequestMetrics(
    blink::mojom::FederatedAuthRequestResult result,
    bool did_show_ui) {
  DCHECK(IsFedCmMetricsEndpointEnabled());
  for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
    SendFailedTokenRequestMetrics(metrics_endpoint_kv.second, result,
                                  did_show_ui);
  }
}

void FedCmAccountsFetcher::SendSuccessfulTokenRequestMetrics(
    const GURL& idp_config_url,
    base::TimeDelta api_call_to_show_dialog_time,
    base::TimeDelta show_dialog_to_continue_clicked_time,
    base::TimeDelta account_selected_to_token_response_time,
    base::TimeDelta api_call_to_token_response_time,
    bool did_show_ui) {
  DCHECK(IsFedCmMetricsEndpointEnabled());

  for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
    const GURL& metrics_endpoint = metrics_endpoint_kv.second;
    if (!metrics_endpoint.is_valid()) {
      continue;
    }

    if (metrics_endpoint_kv.first == idp_config_url) {
      network_manager_->SendSuccessfulTokenRequestMetrics(
          metrics_endpoint, api_call_to_show_dialog_time,
          show_dialog_to_continue_clicked_time,
          account_selected_to_token_response_time,
          api_call_to_token_response_time);
    } else {
      // Send kUserFailure so that IDP cannot tell difference between user
      // selecting a different IDP and user dismissing dialog without
      // selecting any IDP.
      network_manager_->SendFailedTokenRequestMetrics(
          metrics_endpoint, did_show_ui,
          MetricsEndpointErrorCode::kUserFailure);
    }
  }
}

void FedCmAccountsFetcher::OnAllConfigAndWellKnownFetched(
    std::vector<FedCmConfigFetcher::FetchResult> fetch_results) {
  config_fetcher_.reset();

  base::TimeTicks well_known_and_config_fetched_time = base::TimeTicks::Now();
  federated_auth_request_impl_->SetWellKnownAndConfigFetchedTime(
      well_known_and_config_fetched_time);

  base::flat_map<GURL, IdentityProviderGetInfo>& token_request_get_infos =
      federated_auth_request_impl_->GetTokenRequestGetInfos();
  for (const FedCmConfigFetcher::FetchResult& fetch_result : fetch_results) {
    const GURL& identity_provider_config_url =
        fetch_result.identity_provider_config_url;
    auto get_info_it =
        token_request_get_infos.find(identity_provider_config_url);
    CHECK(get_info_it != token_request_get_infos.end());

    metrics_endpoints_[identity_provider_config_url] =
        fetch_result.endpoints.metrics;

    std::unique_ptr<IdentityProviderInfo> idp_info =
        std::make_unique<IdentityProviderInfo>(
            get_info_it->second.provider, std::move(fetch_result.endpoints),
            fetch_result.metadata ? std::move(*fetch_result.metadata)
                                  : IdentityProviderMetadata(),
            get_info_it->second.rp_context, get_info_it->second.rp_mode,
            get_info_it->second.format);

    if (fetch_result.error) {
      const FedCmConfigFetcher::FetchError& fetch_error = *fetch_result.error;
      if (fetch_error.additional_console_error_message) {
        render_frame_host_->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            *fetch_error.additional_console_error_message);
      }
      federated_auth_request_impl_->OnFetchDataForIdpFailed(
          std::move(idp_info), fetch_error.result, fetch_error.token_status,
          /*should_delay_callback=*/params_.rp_mode == RpMode::kPassive);
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

    federated_auth_request_impl_->SetIdpLoginInfo(
        idp_info->metadata.idp_login_url, idp_info->provider->login_hint,
        idp_info->provider->domain_hint);

    // Make sure that we don't fetch accounts if the IDP sign-in bit is
    // reset to false during the API call. e.g. by the login/logout HEADER.
    // In the active flow we get here even if the IDP sign-in bit was false
    // originally, because we need the well-known and config files to find
    // the login URL.
    idp_info->has_failing_idp_signin_status =
        webid::ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
            *render_frame_host_, identity_provider_config_url,
            permission_delegate_);
    if (idp_info->has_failing_idp_signin_status) {
      // If the user is logged out and we are in a active-mode, allow the
      // user to sign-in to the IdP and return early.
      if (params_.rp_mode == blink::mojom::RpMode::kActive) {
        federated_auth_request_impl_->MaybeShowActiveModeModalDialog(
            identity_provider_config_url, idp_info->metadata.idp_login_url);
        return;
      }
      // Do not send metrics for IDP where the user is not signed-in in
      // order to prevent IDP from using the user IP to make a probabilistic
      // model of which websites a user visits.
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

    network_manager_->SendAccountsRequest(
        url::Origin::Create(config_url), accounts_endpoint, client_id,
        base::BindOnce(&FedCmAccountsFetcher::OnAccountsResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(idp_info)));
    federated_auth_request_impl_->fedcm_metrics()->RecordAccountsRequestSent(
        config_url);
  }
}

void FedCmAccountsFetcher::OnAccountsResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    std::vector<IdentityRequestAccountPtr> accounts) {
  federated_auth_request_impl_->SetAccountsFetchedTime(base::TimeTicks::Now());

  GURL idp_config_url = idp_info->provider->config->config_url;
  const std::optional<bool> old_idp_signin_status =
      permission_delegate_->GetIdpSigninStatus(
          url::Origin::Create(idp_config_url));
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      *render_frame_host_, idp_config_url, status,
      idp_info->has_failing_idp_signin_status, permission_delegate_);

  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess) {
    std::pair<FederatedAuthRequestResult, TokenStatus> resultAndTokenStatus =
        AccountParseStatusToRequestResultAndTokenStatus(status.parse_status);
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               resultAndTokenStatus.first,
                               resultAndTokenStatus.second, status);
    return;
  }
  RecordRawAccountsSize(accounts.size());
  if (!FilterAccountsWithLabel(idp_info->metadata.requested_label, accounts)) {
    // No accounts remain, so treat as account fetch failure.
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Accounts were received, but none matched the label.");
    // If there are no accounts after filtering based on the label,
    // treat this exactly the same as if we had received an empty accounts
    // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               FederatedAuthRequestResult::kAccountsListEmpty,
                               TokenStatus::kAccountsListEmpty, status);
    return;
  }
  if (!FilterAccountsWithLoginHint(idp_info->provider->login_hint, accounts)) {
    // No accounts remain, so treat as account fetch failure.
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Accounts were received, but none matched the loginHint.");
    // If there are no accounts after filtering based on the login hint,
    // treat this exactly the same as if we had received an empty accounts
    // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               FederatedAuthRequestResult::kAccountsListEmpty,
                               TokenStatus::kAccountsListEmpty, status);
    return;
  }
  if (!FilterAccountsWithDomainHint(idp_info->provider->domain_hint,
                                    accounts)) {
    // No accounts remain, so treat as account fetch failure.
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Accounts were received, but none matched the domainHint.");
    // If there are no accounts after filtering based on the domain hint,
    // treat this exactly the same as if we had received an empty accounts
    // list, i.e. IdpNetworkRequestManager::ParseStatus::kEmptyListError.
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               FederatedAuthRequestResult::kAccountsListEmpty,
                               TokenStatus::kAccountsListEmpty, status);
    return;
  }
  auto filter = [](const IdentityRequestAccountPtr& account) {
    return account->is_filtered_out;
  };
  if (!IsFedCmShowFilteredAccountsEnabled() ||
      !federated_auth_request_impl_->HasUserTriedToSignInToIdp(
          idp_config_url) ||
      federated_auth_request_impl_->login_url() !=
          idp_info->metadata.idp_login_url) {
    std::erase_if(accounts, filter);
  } else {
    // If the user is logging in to new accounts, only show filtered
    // accounts if there are no new unfiltered accounts. This includes in
    // particular the case where all accounts are filtered out.
    size_t new_unfiltered = std::count_if(
        accounts.begin(), accounts.end(),
        [&](const IdentityRequestAccountPtr& account) {
          return !account->is_filtered_out &&
                 !federated_auth_request_impl_->HadAccountIdBeforeLogin(
                     account->id);
        });
    if (new_unfiltered > 0u) {
      std::erase_if(accounts, filter);
    }
  }
  if (accounts.size() == 0u) {
    // No accounts remain, so treat as account fetch failure.
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Accounts were received, but none matched the login hint, domain "
        "hint, and/or account labels provided.");
    // If there are no accounts after filtering,treat this exactly the same
    // as if we had received an empty accounts list, i.e.
    // IdpNetworkRequestManager::ParseStatus::kEmptyListError.
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               FederatedAuthRequestResult::kAccountsListEmpty,
                               TokenStatus::kAccountsListEmpty, status);
    return;
  }
  RecordReadyToShowAccountsSize(accounts.size());
  ComputeLoginStates(idp_info->provider->config->config_url, accounts);

  OnAccountsFetchSucceeded(std::move(idp_info), status, std::move(accounts));
}

void FedCmAccountsFetcher::OnAccountsFetchSucceeded(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    IdpNetworkRequestManager::FetchStatus status,
    std::vector<IdentityRequestAccountPtr> accounts) {
  bool need_client_metadata = false;
  if (!idp_info->provider->config->from_idp_registration_api &&
      !GetDisclosureFields(idp_info->provider->fields).empty()) {
    for (const auto& account : accounts) {
      // ComputeLoginStates() should have populated the login_state.
      DCHECK(account->login_state);
      if (*account->login_state == LoginState::kSignUp) {
        need_client_metadata = true;
        break;
      }
    }
  }

  if (need_client_metadata &&
      webid::IsEndpointSameOrigin(idp_info->provider->config->config_url,
                                  idp_info->endpoints.client_metadata)) {
    // Copy OnClientMetadataResponseReceived() parameters because `idp_info`
    // is moved.
    GURL client_metadata_endpoint = idp_info->endpoints.client_metadata;
    std::string client_id = idp_info->provider->config->client_id;
    network_manager_->FetchClientMetadata(
        client_metadata_endpoint, client_id, params_.icon_ideal_size,
        params_.icon_minimum_size,
        base::BindOnce(&FedCmAccountsFetcher::OnClientMetadataResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                       std::move(accounts)));
  } else {
    GURL idp_brand_icon_url = idp_info->metadata.brand_icon_url;
    network_manager_->FetchAccountPicturesAndBrandIcons(
        std::move(accounts), std::move(idp_info),
        /*rp_brand_icon_url=*/GURL(),
        base::BindOnce(&FedCmAccountsFetcher::OnFetchDataForIdpSucceeded,
                       weak_ptr_factory_.GetWeakPtr(),
                       IdpNetworkRequestManager::ClientMetadata()));
  }
}

void FedCmAccountsFetcher::OnClientMetadataResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::vector<IdentityRequestAccountPtr>&& accounts,
    IdpNetworkRequestManager::FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata client_metadata) {
  federated_auth_request_impl_->SetClientMetadataFetchedTime(
      base::TimeTicks::Now());

  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.

  GURL rp_brand_icon_url = client_metadata.brand_icon_url;
  network_manager_->FetchAccountPicturesAndBrandIcons(
      std::move(accounts), std::move(idp_info), rp_brand_icon_url,
      base::BindOnce(&FedCmAccountsFetcher::OnFetchDataForIdpSucceeded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(client_metadata)));
}

void FedCmAccountsFetcher::OnFetchDataForIdpSucceeded(
    const IdpNetworkRequestManager::ClientMetadata& client_metadata,
    std::vector<IdentityRequestAccountPtr> accounts,
    std::unique_ptr<IdentityProviderInfo> idp_info,
    const gfx::Image& rp_brand_icon) {
  const GURL& idp_config_url = idp_info->provider->config->config_url;

  std::vector<IdentityRequestDialogDisclosureField> disclosure_fields =
      GetDisclosureFields(idp_info->provider->fields);

  const std::string idp_for_display =
      webid::FormatUrlForDisplay(idp_config_url);
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      idp_for_display, idp_info->metadata,
      ClientMetadata{client_metadata.terms_of_service_url,
                     client_metadata.privacy_policy_url,
                     client_metadata.brand_icon_url, rp_brand_icon},
      idp_info->rp_context, idp_info->format, disclosure_fields,
      /*has_login_status_mismatch=*/false);
  idp_info->client_matches_top_frame_origin =
      client_metadata.client_matches_top_frame_origin;
  for (auto& account : accounts) {
    account->identity_provider = idp_info->data;
  }

  federated_auth_request_impl_->OnFetchDataForIdpSucceeded(std::move(accounts),
                                                           std::move(idp_info));
}

bool FedCmAccountsFetcher::FilterAccountsWithLabel(
    const std::string& label,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (label.empty()) {
    return true;
  }

  // Filter out all accounts whose labels do not match the requested label.
  // Note that it is technically possible for us to end up with more than
  // one account afterwards, in which case the multiple account chooser
  // would be shown.
  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (!base::Contains(account->labels, label)) {
      account->is_filtered_out = true;
    } else {
      ++accounts_remaining;
    }
  }
  federated_auth_request_impl_->fedcm_metrics()->RecordNumMatchingAccounts(
      accounts_remaining, "AccountLabel");
  return IsFedCmShowFilteredAccountsEnabled() || accounts_remaining > 0u;
}

bool FedCmAccountsFetcher::FilterAccountsWithLoginHint(
    const std::string& login_hint,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (login_hint.empty()) {
    return true;
  }

  // Filter out all accounts whose ID and whose email do not match the login
  // hint. Note that it is technically possible for us to end up with more
  // than one account afterwards, in which case the multiple account chooser
  // would be shown.
  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (account->is_filtered_out) {
      continue;
    }
    if (!base::Contains(account->login_hints, login_hint)) {
      account->is_filtered_out = true;
    } else {
      ++accounts_remaining;
    }
  }
  federated_auth_request_impl_->fedcm_metrics()->RecordNumMatchingAccounts(
      accounts_remaining, "LoginHint");
  return IsFedCmShowFilteredAccountsEnabled() || accounts_remaining > 0u;
}

bool FedCmAccountsFetcher::FilterAccountsWithDomainHint(
    const std::string& domain_hint,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (domain_hint.empty()) {
    return true;
  }

  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (account->is_filtered_out) {
      continue;
    }
    if (domain_hint == FederatedAuthRequestImpl::kWildcardDomainHint) {
      if (account->domain_hints.empty()) {
        account->is_filtered_out = true;
        continue;
      }
    } else if (!base::Contains(account->domain_hints, domain_hint)) {
      account->is_filtered_out = true;
      continue;
    }
    ++accounts_remaining;
  }
  federated_auth_request_impl_->fedcm_metrics()->RecordNumMatchingAccounts(
      accounts_remaining, "DomainHint");
  return IsFedCmShowFilteredAccountsEnabled() || accounts_remaining > 0u;
}

void FedCmAccountsFetcher::ComputeLoginStates(
    const GURL& idp_config_url,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  url::Origin idp_origin = url::Origin::Create(idp_config_url);
  // Populate the accounts login state.
  for (auto& account : accounts) {
    // Record when IDP and browser have different user sign-in states.
    bool idp_claimed_sign_in = account->login_state == LoginState::kSignIn;
    account->last_used_timestamp = permission_delegate_->GetLastUsedTimestamp(
        render_frame_host_->GetLastCommittedOrigin(),
        federated_auth_request_impl_->GetEmbeddingOrigin(), idp_origin,
        account->id);

    if (idp_claimed_sign_in == account->last_used_timestamp.has_value()) {
      federated_auth_request_impl_->fedcm_metrics()
          ->RecordSignInStateMatchStatus(idp_config_url,
                                         SignInStateMatchStatus::kMatch);
    } else if (idp_claimed_sign_in) {
      federated_auth_request_impl_->fedcm_metrics()
          ->RecordSignInStateMatchStatus(
              idp_config_url, SignInStateMatchStatus::kIdpClaimedSignIn);
    } else {
      federated_auth_request_impl_->fedcm_metrics()
          ->RecordSignInStateMatchStatus(
              idp_config_url, SignInStateMatchStatus::kBrowserObservedSignIn);
    }

    // We set the login state based on the IDP response if it sends
    // back an approved_clients list. If it does not, we need to set
    // it here based on browser state.
    if (!account->login_state) {
      // Consider this a sign-in if we have seen a successful sign-up for
      // this account before.
      account->login_state = account->last_used_timestamp.has_value()
                                 ? LoginState::kSignIn
                                 : LoginState::kSignUp;
    }

    if (webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
            *render_frame_host_, /*provider_url=*/idp_config_url,
            federated_auth_request_impl_->GetEmbeddingOrigin(),
            render_frame_host_->GetLastCommittedOrigin(), account->id,
            permission_delegate_, api_permission_delegate_)) {
      // At this moment we can trust login_state even though it's controlled
      // by IdP. If it's kSignUp, it could mean that the browser's sharing
      // permission is obsolete.
      account->browser_trusted_login_state = account->login_state.value();
    }
  }
}

void FedCmAccountsFetcher::HandleAccountsFetchFailure(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::optional<bool> old_idp_signin_status,
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<TokenStatus> token_status,
    const IdpNetworkRequestManager::FetchStatus& status) {
  if (status.parse_status != IdpNetworkRequestManager::ParseStatus::kSuccess) {
    webid::MaybeAddResponseCodeToConsole(
        *render_frame_host_, "accounts endpoint", status.response_code);
  }
  if (!old_idp_signin_status.has_value()) {
    if (params_.rp_mode == blink::mojom::RpMode::kActive) {
      federated_auth_request_impl_->MaybeShowActiveModeModalDialog(
          idp_info->provider->config->config_url,
          idp_info->metadata.idp_login_url);
      return;
    }
    federated_auth_request_impl_->OnFetchDataForIdpFailed(
        std::move(idp_info), result, token_status,
        /*should_delay_callback=*/true);
    return;
  }

  if (!IsFrameActive(render_frame_host_->GetMainFrame())) {
    federated_auth_request_impl_->CompleteRequestWithError(
        FederatedAuthRequestResult::kRpPageNotVisible,
        TokenStatus::kRpPageNotVisible,
        /*should_delay_callback=*/true);
    return;
  }

  if (params_.mediation_requirement == MediationRequirement::kSilent) {
    // By this moment we know that the user has granted permission in the
    // past for the RP/IdP. Because otherwise we have returned already in
    // `ShouldFailBeforeFetchingAccounts`. It means that we can do the
    // following without privacy cost:
    // 1. Reject the promise immediately without delay
    // 2. Not to show any UI to respect `mediation: silent`
    // TODO(crbug.com/40266561): validate the statement above with
    // stakeholders
    federated_auth_request_impl_->OnFetchDataForIdpFailed(
        std::move(idp_info),
        FederatedAuthRequestResult::kSilentMediationFailure,
        TokenStatus::kSilentMediationFailure,
        /*should_delay_callback=*/false);
    return;
  }

  // We are going to show mismatch UI, so fetch the brand icon URL (needed
  // at least for passive mode). We currently never need the RP icon for
  // mismatch UI.
  network_manager_->FetchIdpBrandIcon(
      std::move(idp_info), base::BindOnce(&FedCmAccountsFetcher::OnIdpMismatch,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void FedCmAccountsFetcher::OnIdpMismatch(
    std::unique_ptr<IdentityProviderInfo> idp_info) {
  const std::string idp_for_display =
      webid::FormatUrlForDisplay(idp_info->provider->config->config_url);
  idp_info->data = base::MakeRefCounted<IdentityProviderData>(
      idp_for_display, idp_info->metadata,
      ClientMetadata{GURL(), GURL(), GURL(), gfx::Image()},
      idp_info->rp_context, idp_info->format,
      GetDisclosureFields(idp_info->provider->fields),
      /*has_login_status_mismatch=*/true);
  federated_auth_request_impl_->OnIdpMismatch(std::move(idp_info));
}

void FedCmAccountsFetcher::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint,
    blink::mojom::FederatedAuthRequestResult result,
    bool did_show_ui) {
  DCHECK(IsFedCmMetricsEndpointEnabled());
  if (!metrics_endpoint.is_valid()) {
    return;
  }

  network_manager_->SendFailedTokenRequestMetrics(
      metrics_endpoint, did_show_ui,
      FederatedAuthRequestResultToMetricsEndpointErrorCode(result));
}

}  // namespace content

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/accounts_fetcher.h"

#include <set>

#include "base/containers/contains.h"
#include "content/browser/webid/config_fetcher.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/mappers.h"
#include "content/browser/webid/request_service.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-data-view.h"

using ::blink::mojom::FederatedAuthRequestResult;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInStateMatchStatus = content::webid::SignInStateMatchStatus;
using TokenStatus = content::webid::RequestIdTokenStatus;

namespace content::webid {

namespace {
static constexpr char kVcSdJwt[] = "vc+sd-jwt";

bool IsFrameActive(RenderFrameHost* frame) {
  return frame && frame->IsActive();
}

bool ValidateWellKnownFormatForClientMetadata(
    const IdpNetworkRequestManager::WellKnown& well_known,
    bool has_client_metadata_endpoint) {
  if (!has_client_metadata_endpoint) {
    return true;
  }

  // client_metadata endpoint exists - require direct endpoints format
  // Check if both accounts_endpoint and login_url are present (direct endpoints
  // format)
  if (well_known.accounts.is_empty() || well_known.login_url.is_empty()) {
    return false;
  }

  return true;
}
}  // namespace

AccountsFetcher::IdentityProviderGetInfo::IdentityProviderGetInfo(
    blink::mojom::IdentityProviderRequestOptionsPtr provider,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    std::optional<blink::mojom::Format> format)
    : provider(std::move(provider)),
      rp_context(rp_context),
      rp_mode(rp_mode),
      format(format) {}

AccountsFetcher::IdentityProviderGetInfo::~IdentityProviderGetInfo() = default;
AccountsFetcher::IdentityProviderGetInfo::IdentityProviderGetInfo(
    const IdentityProviderGetInfo& other) {
  *this = other;
}

AccountsFetcher::IdentityProviderGetInfo&
AccountsFetcher::IdentityProviderGetInfo::operator=(
    const IdentityProviderGetInfo& other) {
  provider = other.provider->Clone();
  rp_context = other.rp_context;
  rp_mode = other.rp_mode;
  format = other.format;
  return *this;
}

AccountsFetcher::FedCmFetchingParams::FedCmFetchingParams(
    blink::mojom::RpMode rp_mode,
    int icon_ideal_size,
    int icon_minimum_size,
    MediationRequirement mediation_requirement)
    : rp_mode(rp_mode),
      icon_ideal_size(icon_ideal_size),
      icon_minimum_size(icon_minimum_size),
      mediation_requirement(mediation_requirement) {}

AccountsFetcher::FedCmFetchingParams::~FedCmFetchingParams() = default;

AccountsFetcher::AccountsFetcher(
    RenderFrameHost& render_frame_host,
    IdpNetworkRequestManager* network_manager,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
    FederatedIdentityPermissionContextDelegate* permission_delegate,
    FedCmFetchingParams params,
    RequestService* federated_auth_request_impl)
    : render_frame_host_(render_frame_host),
      network_manager_(network_manager),
      api_permission_delegate_(api_permission_delegate),
      permission_delegate_(permission_delegate),
      params_(params),
      federated_auth_request_impl_(federated_auth_request_impl) {}

AccountsFetcher::~AccountsFetcher() = default;

void AccountsFetcher::FetchEndpointsForIdps(
    const std::set<GURL>& idp_config_urls) {
  std::vector<ConfigFetcher::FetchRequest> idps;
  base::flat_map<GURL, IdentityProviderGetInfo>& token_request_get_infos =
      federated_auth_request_impl_->GetTokenRequestGetInfos();
  for (const auto& idp : idp_config_urls) {
    auto idp_get = token_request_get_infos.find(idp);
    CHECK(idp_get != token_request_get_infos.end());
    idps.emplace_back(
        idp, idp_get->second.provider->config->from_idp_registration_api);
  }

  config_fetcher_ =
      std::make_unique<ConfigFetcher>(*render_frame_host_, network_manager_);
  config_fetcher_->Start(
      idps, params_.rp_mode, params_.icon_ideal_size, params_.icon_minimum_size,
      base::BindOnce(&AccountsFetcher::OnAllConfigAndWellKnownFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountsFetcher::SendAllFailedTokenRequestMetrics(
    blink::mojom::FederatedAuthRequestResult result,
    bool did_show_ui) {
  DCHECK(IsMetricsEndpointEnabled());
  for (const auto& metrics_endpoint_kv : metrics_endpoints_) {
    SendFailedTokenRequestMetrics(metrics_endpoint_kv.second, result,
                                  did_show_ui);
  }
}

void AccountsFetcher::SendSuccessfulTokenRequestMetrics(
    const GURL& idp_config_url,
    base::TimeDelta api_call_to_show_dialog_time,
    base::TimeDelta show_dialog_to_continue_clicked_time,
    base::TimeDelta account_selected_to_token_response_time,
    base::TimeDelta api_call_to_token_response_time,
    bool did_show_ui) {
  DCHECK(IsMetricsEndpointEnabled());

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

void AccountsFetcher::OnAllConfigAndWellKnownFetched(
    std::vector<ConfigFetcher::FetchResult> fetch_results) {
  config_fetcher_.reset();

  base::TimeTicks well_known_and_config_fetched_time = base::TimeTicks::Now();
  federated_auth_request_impl_->SetWellKnownAndConfigFetchedTime(
      well_known_and_config_fetched_time);

  base::flat_map<GURL, IdentityProviderGetInfo>& token_request_get_infos =
      federated_auth_request_impl_->GetTokenRequestGetInfos();
  for (const ConfigFetcher::FetchResult& fetch_result : fetch_results) {
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
      const ConfigFetcher::FetchError& fetch_error = *fetch_result.error;
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

    // Check if this IDP has a client_metadata endpoint
    bool has_client_metadata_endpoint =
        !fetch_result.endpoints.client_metadata.is_empty();

    if (!ValidateWellKnownFormatForClientMetadata(
            fetch_result.wellknown, has_client_metadata_endpoint)) {
      render_frame_host_->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "The FedCM configuration uses client_metadata but the "
          ".well-known/web-identity file is missing required endpoints. "
          "When client_metadata is used, both 'accounts_endpoint' and "
          "'login_url' must be explicitly included in the well-known file "
          "for privacy reasons. This will become a hard requirement in Chrome "
          "145. Please update your .well-known/web-identity file to include "
          "these endpoints.");

      if (IsWellKnownEndpointValidationEnabled()) {
        federated_auth_request_impl_->OnFetchDataForIdpFailed(
            std::move(idp_info),
            FederatedAuthRequestResult::kWellKnownInvalidResponse,
            TokenStatus::kWellKnownInvalidResponse,
            /*should_delay_callback=*/false);
        continue;
      }
    }

    if (IsIdPRegistrationEnabled()) {
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
      if (!IsDelegationEnabled() ||
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
    // Do not fetch accounts if the IDP is registered.
    if (idp_info->provider->config->from_idp_registration_api) {
      accounts_endpoint = GURL();
    }
    std::string client_id = idp_info->provider->config->client_id;
    const GURL& config_url = idp_info->provider->config->config_url;

    if (network_manager_->SendAccountsRequest(
            url::Origin::Create(config_url), accounts_endpoint, client_id,
            base::BindOnce(&AccountsFetcher::OnAccountsResponseReceived,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(idp_info)))) {
      federated_auth_request_impl_->fedcm_metrics()->RecordAccountsRequestSent(
          config_url);
    }
  }
}

void AccountsFetcher::OnAccountsResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    FetchStatus status,
    std::vector<IdentityRequestAccountPtr> accounts) {
  federated_auth_request_impl_->SetAccountsFetchedTime(base::TimeTicks::Now());

  GURL idp_config_url = idp_info->provider->config->config_url;
  const std::optional<bool> old_idp_signin_status =
      permission_delegate_->GetIdpSigninStatus(
          url::Origin::Create(idp_config_url));
  webid::UpdateIdpSigninStatusForAccountsEndpointResponse(
      *render_frame_host_, idp_config_url, status,
      idp_info->has_failing_idp_signin_status, permission_delegate_);

  if (status.parse_status != ParseStatus::kSuccess) {
    std::pair<FederatedAuthRequestResult, TokenStatus> resultAndTokenStatus =
        AccountParseStatusToRequestResultAndTokenStatus(status.parse_status);
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               resultAndTokenStatus.first,
                               resultAndTokenStatus.second, status);
    return;
  }
  RecordRawAccountsSize(accounts.size());
  RecordAccountFieldsType(accounts);
  FilterAccountsWithLabel(idp_info->metadata.requested_label, accounts);
  FilterAccountsWithLoginHint(idp_info->provider->login_hint, accounts);
  FilterAccountsWithDomainHint(idp_info->provider->domain_hint, accounts);
  auto filter = [](const IdentityRequestAccountPtr& account) {
    return account->is_filtered_out;
  };
  if (!federated_auth_request_impl_->HasUserTriedToSignInToIdp(
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
    // ParseStatus::kEmptyListError.
    HandleAccountsFetchFailure(std::move(idp_info), old_idp_signin_status,
                               FederatedAuthRequestResult::kAccountsListEmpty,
                               TokenStatus::kAccountsListEmpty, status);
    return;
  }
  RecordReadyToShowAccountsSize(accounts.size());
  ComputeLoginStates(idp_info->provider->config->config_url, accounts);
  ComputeAccountFields(GetDisclosureFields(idp_info->provider->fields),
                       accounts);

  OnAccountsFetchSucceeded(std::move(idp_info), status, std::move(accounts));
}

void AccountsFetcher::OnAccountsFetchSucceeded(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    FetchStatus status,
    std::vector<IdentityRequestAccountPtr> accounts) {
  bool need_client_metadata = false;
  if (IsIframeOriginEnabled()) {
    // For cross-site iframes, we need to fetch client metadata in case the
    // IDP sends `client_is_third_party_to_top_frame_origin: true`.
    url::Origin embedding_origin =
        render_frame_host_->GetMainFrame()->GetLastCommittedOrigin();
    url::Origin rp_origin = render_frame_host_->GetLastCommittedOrigin();
    need_client_metadata |=
        !net::SchemefulSite::IsSameSite(embedding_origin, rp_origin);
  }
  if (!need_client_metadata &&
      !idp_info->provider->config->from_idp_registration_api &&
      !GetDisclosureFields(idp_info->provider->fields).empty()) {
    for (const auto& account : accounts) {
      if (account->idp_claimed_login_state.value_or(
              account->browser_trusted_login_state) == LoginState::kSignUp) {
        need_client_metadata |= true;
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
        base::BindOnce(&AccountsFetcher::OnClientMetadataResponseReceived,
                       weak_ptr_factory_.GetWeakPtr(), std::move(idp_info),
                       std::move(accounts)));
  } else {
    GURL idp_brand_icon_url = idp_info->metadata.brand_icon_url;
    network_manager_->FetchAccountPicturesAndBrandIcons(
        std::move(accounts), std::move(idp_info),
        /*rp_brand_icon_url=*/GURL(),
        base::BindOnce(&AccountsFetcher::OnFetchDataForIdpSucceeded,
                       weak_ptr_factory_.GetWeakPtr(),
                       IdpNetworkRequestManager::ClientMetadata()));
  }
}

void AccountsFetcher::OnClientMetadataResponseReceived(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::vector<IdentityRequestAccountPtr>&& accounts,
    FetchStatus status,
    IdpNetworkRequestManager::ClientMetadata client_metadata) {
  federated_auth_request_impl_->SetClientMetadataFetchedTime(
      base::TimeTicks::Now());

  // TODO(yigu): Clean up the client metadata related errors for metrics and
  // console logs.

  GURL rp_brand_icon_url = client_metadata.brand_icon_url;
  network_manager_->FetchAccountPicturesAndBrandIcons(
      std::move(accounts), std::move(idp_info), rp_brand_icon_url,
      base::BindOnce(&AccountsFetcher::OnFetchDataForIdpSucceeded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(client_metadata)));
}

void AccountsFetcher::OnFetchDataForIdpSucceeded(
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
  idp_info->client_is_third_party_to_top_frame_origin =
      client_metadata.client_is_third_party_to_top_frame_origin;
  for (auto& account : accounts) {
    account->identity_provider = idp_info->data;
  }

  federated_auth_request_impl_->OnFetchDataForIdpSucceeded(std::move(accounts),
                                                           std::move(idp_info));
}

void AccountsFetcher::FilterAccountsWithLabel(
    const std::string& label,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (label.empty()) {
    return;
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
}

void AccountsFetcher::FilterAccountsWithLoginHint(
    const std::string& login_hint,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (login_hint.empty()) {
    return;
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
}

void AccountsFetcher::FilterAccountsWithDomainHint(
    const std::string& domain_hint,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  if (domain_hint.empty()) {
    return;
  }

  size_t accounts_remaining = 0u;
  for (auto& account : accounts) {
    if (account->is_filtered_out) {
      continue;
    }
    if (domain_hint == RequestService::kWildcardDomainHint) {
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
}

void AccountsFetcher::ComputeLoginStates(
    const GURL& idp_config_url,
    std::vector<IdentityRequestAccountPtr>& accounts) {
  url::Origin idp_origin = url::Origin::Create(idp_config_url);
  // Populate the accounts login state.
  for (auto& account : accounts) {
    // Record when IDP and browser have different user sign-in states.
    bool idp_claimed_sign_in =
        account->idp_claimed_login_state == LoginState::kSignIn;
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

    if (webid::HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
            *render_frame_host_, /*provider_url=*/idp_config_url,
            federated_auth_request_impl_->GetEmbeddingOrigin(),
            render_frame_host_->GetLastCommittedOrigin(), account->id,
            permission_delegate_, api_permission_delegate_)) {
      LoginState browser_observed_login_state =
          account->last_used_timestamp.has_value() ? LoginState::kSignIn
                                                   : LoginState::kSignUp;
      // At this moment we can trust login_state even though it's controlled
      // by IdP. If it's kSignUp, it could mean that the browser's sharing
      // permission is obsolete.
      account->browser_trusted_login_state =
          account->idp_claimed_login_state.value_or(
              browser_observed_login_state);
    }
  }
}

void AccountsFetcher::HandleAccountsFetchFailure(
    std::unique_ptr<IdentityProviderInfo> idp_info,
    std::optional<bool> old_idp_signin_status,
    blink::mojom::FederatedAuthRequestResult result,
    std::optional<TokenStatus> token_status,
    const FetchStatus& status) {
  if (status.parse_status != ParseStatus::kSuccess) {
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
    // `ShouldFailBeforeFetchingAccounts`. It means that we don't need to show
    // any UI to respect `mediation: silent`.
    federated_auth_request_impl_->OnFetchDataForIdpFailed(
        std::move(idp_info),
        FederatedAuthRequestResult::kSilentMediationFailure,
        TokenStatus::kSilentMediationFailure,
        /*should_delay_callback=*/true);
    return;
  }

  // We are going to show mismatch UI, so fetch the brand icon URL (needed
  // at least for passive mode). We currently never need the RP icon for
  // mismatch UI.
  network_manager_->FetchIdpBrandIcon(
      std::move(idp_info), base::BindOnce(&AccountsFetcher::OnIdpMismatch,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void AccountsFetcher::OnIdpMismatch(
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

void AccountsFetcher::SendFailedTokenRequestMetrics(
    const GURL& metrics_endpoint,
    blink::mojom::FederatedAuthRequestResult result,
    bool did_show_ui) {
  DCHECK(IsMetricsEndpointEnabled());
  if (!metrics_endpoint.is_valid()) {
    return;
  }

  network_manager_->SendFailedTokenRequestMetrics(
      metrics_endpoint, did_show_ui,
      FederatedAuthRequestResultToMetricsEndpointErrorCode(result));
}

}  // namespace content::webid

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_credential_source_impl.h"

#include "base/functional/callback.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/request_page_data.h"
#include "content/browser/webid/request_service.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content::webid {

DOCUMENT_USER_DATA_KEY_IMPL(IdentityCredentialSourceImpl);

IdentityCredentialSourceImpl::IdentityCredentialSourceImpl(RenderFrameHost* rfh)
    : DocumentUserData(rfh),
      network_manager_(IdpNetworkRequestManager::Create(
          static_cast<RenderFrameHostImpl*>(rfh))),
      api_permission_delegate_(
          rfh->GetBrowserContext()->GetFederatedIdentityApiPermissionContext()),
      permission_delegate_(
          rfh->GetBrowserContext()->GetFederatedIdentityPermissionContext()) {}

IdentityCredentialSourceImpl::~IdentityCredentialSourceImpl() = default;

void IdentityCredentialSourceImpl::GetIdentityCredentialSuggestions(
    const std::vector<GURL>& embedder_requested_idps,
    GetIdentityCredentialSuggestionsCallback callback) {
  // Cancel previous request if any.
  weak_ptr_factory_.InvalidateWeakPtrs();
  accounts_fetcher_.reset();
  metrics_.reset();

  callback_ = std::move(callback);

  // TODO(crbug.com/485628241): should we check that embedder_requested_idps is
  // the same as the currently requested IDPs?
  RequestPageData* page_data = GetPageData(render_frame_host().GetPage());
  // Note that the pending FedCM request may come from an iframe. For now, we
  // assume these will login the user to the top-level page, and return these
  // options.
  if (page_data && page_data->PendingWebIdentityRequest()) {
    RequestService* request_service = page_data->PendingWebIdentityRequest();
    // These are the accounts that would be displayed in the UI, so filters such
    // as login hint have been applied already. But they may be in the accounts
    // in edge cases where they will be shown in the UI.
    const std::vector<IdentityRequestAccountPtr>& request_accounts =
        request_service->GetAccounts();
    std::vector<IdentityRequestAccountPtr> signin_accounts;
    for (const auto& account : request_accounts) {
      if (!account->is_filtered_out &&
          account->idp_claimed_login_state.value_or(
              account->browser_trusted_login_state) ==
              IdentityRequestAccount::LoginState::kSignIn) {
        signin_accounts.push_back(account);
      }
    }

    if (!signin_accounts.empty()) {
      std::move(callback_).Run(signin_accounts);
      return;
    }
  }

  std::vector<ConfigFetcher::FetchRequest> fetch_requests;
  base::flat_map<GURL, AccountsFetcher::IdentityProviderGetInfo>
      token_request_get_infos;

  // TODO(crbug.com/475510317): Instead of just fetching IdPs specified by the
  // embedder, we should consider other sources such as IdPs who are using the
  // passive mode etc. e.g. if there's a pending FedCM request from an IdP, we
  // can skip fetching accounts for that IdP if its accounts are already
  // fetched.
  // TODO(crbug.com/475473059): Right now the caller must pass in the IdP's
  // configURL instead of its eTLD+1. We should support the latter and fetch the
  // accounts based on it.
  for (const auto& idp : embedder_requested_idps) {
    if (permission_delegate_->GetIdpSigninStatus(url::Origin::Create(idp))
            .value_or(true) == false) {
      continue;
    }
    fetch_requests.emplace_back(idp,
                                /*force_skip_well_known_enforcement=*/false);
    blink::mojom::IdentityProviderRequestOptionsPtr options =
        blink::mojom::IdentityProviderRequestOptions::New();
    options->config = blink::mojom::IdentityProviderConfig::New();
    options->config->config_url = idp;
    // We don't have the client_id here, so we pass an empty string.
    options->config->client_id = "";
    token_request_get_infos.emplace(
        idp, AccountsFetcher::IdentityProviderGetInfo(
                 std::move(options), blink::mojom::RpContext::kSignIn,
                 blink::mojom::RpMode::kPassive, /*format=*/std::nullopt));
  }

  if (fetch_requests.empty()) {
    std::move(callback_).Run(
        std::vector<scoped_refptr<IdentityRequestAccount>>());
    return;
  }

  metrics_ =
      std::make_unique<Metrics>(render_frame_host().GetPageUkmSourceId());
  // TODO(crbug.com/475864620): Currently the main frame's RFH is passed to the
  // fetcher but it's possible that the federated login lives in a cross-origin
  // iframe who has different RFH. We should understand the impact using main
  // RFH and find the right RFH if needed.
  accounts_fetcher_ = std::make_unique<AccountsFetcher>(
      render_frame_host(), network_manager_.get(), api_permission_delegate_,
      permission_delegate_,
      AccountsFetcher::FedCmFetchingParams(
          blink::mojom::RpMode::kPassive, /*icon_ideal_size=*/0,
          /*icon_minimum_size=*/0, MediationRequirement::kOptional),
      base::BindOnce(&IdentityCredentialSourceImpl::OnAccountsFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetch accounts from eligible IdPs. The sign-in accounts filtering happens
  // after the accounts fetch is completed.
  accounts_fetcher_->FetchEndpointsForIdps(
      fetch_requests, token_request_get_infos, metrics_.get(),
      render_frame_host().GetLastCommittedOrigin(),
      /*filter_accounts_callback=*/base::DoNothing());
}

bool IdentityCredentialSourceImpl::SelectAccount(
    const url::Origin& idp_origin,
    const std::string& account_id) {
  RequestPageData* page_data = GetPageData(render_frame_host().GetPage());
  if (!page_data) {
    return false;
  }
  RequestService* request_service = page_data->PendingWebIdentityRequest();
  if (!request_service) {
    return false;
  }
  const auto& accounts = request_service->GetAccounts();
  for (const auto& account : accounts) {
    const GURL& idp_config_url =
        account->identity_provider->idp_metadata.config_url;
    if (!account->is_filtered_out && account->id == account_id &&
        idp_origin == url::Origin::Create(idp_config_url)) {
      CHECK_EQ(account->idp_claimed_login_state.value_or(
                   account->browser_trusted_login_state),
               IdentityRequestAccount::LoginState::kSignIn);
      request_service->OnAccountSelected(idp_config_url, account->id,
                                         /*is_sign_in=*/true);
      return true;
    }
  }

  // Account not found
  return false;
}

void IdentityCredentialSourceImpl::SetNetworkManagerForTests(
    std::unique_ptr<IdpNetworkRequestManager> network_manager) {
  network_manager_ = std::move(network_manager);
}

void IdentityCredentialSourceImpl::SetPermissionDelegateForTests(
    FederatedIdentityPermissionContextDelegate* permission_delegate) {
  permission_delegate_ = permission_delegate;
}

void IdentityCredentialSourceImpl::OnAccountsFetchCompleted(
    base::TimeTicks,
    std::vector<AccountsFetcher::Result> results) {
  std::vector<scoped_refptr<IdentityRequestAccount>> accounts;
  std::string site =
      FormatUrlToSite(render_frame_host().GetLastCommittedOrigin().GetURL());
  for (const auto& result : results) {
    if (result.accounts.has_value()) {
      auto potentially_sign_in_accounts =
          result.accounts->PotentialAccountsForSite(site);
      accounts.insert(accounts.end(), potentially_sign_in_accounts.begin(),
                      potentially_sign_in_accounts.end());
    }
  }
  std::move(callback_).Run(accounts);
}

// static
IdentityCredentialSource* IdentityCredentialSource::FromPage(
    content::Page& page) {
  return IdentityCredentialSourceImpl::GetOrCreateForCurrentDocument(
      &page.GetMainDocument());
}

}  // namespace content::webid

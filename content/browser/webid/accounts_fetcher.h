// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_ACCOUNTS_FETCHER_H_
#define CONTENT_BROWSER_WEBID_ACCOUNTS_FETCHER_H_

#include <set>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/config_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityPermissionContextDelegate;
class FederatedIdentityApiPermissionContextDelegate;
class RenderFrameHost;

namespace webid {

class RequestService;

// A class that fetches accounts from a set of IDPs. Currently only handles
// config and well-known fetches.
// TODO(crbug.com/417197032): handle accounts fetches in this class.
class AccountsFetcher {
 public:
  struct IdentityProviderGetInfo {
    IdentityProviderGetInfo(blink::mojom::IdentityProviderRequestOptionsPtr,
                            blink::mojom::RpContext rp_context,
                            blink::mojom::RpMode rp_mode,
                            std::optional<blink::mojom::Format> format);
    ~IdentityProviderGetInfo();
    IdentityProviderGetInfo(const IdentityProviderGetInfo&);
    IdentityProviderGetInfo& operator=(const IdentityProviderGetInfo& other);

    blink::mojom::IdentityProviderRequestOptionsPtr provider;
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
    blink::mojom::RpMode rp_mode{blink::mojom::RpMode::kPassive};
    std::optional<blink::mojom::Format> format;
  };

  struct FedCmFetchingParams {
    FedCmFetchingParams(blink::mojom::RpMode rp_mode,
                        int icon_ideal_size,
                        int icon_minimum_size,
                        MediationRequirement mediation_requirement);
    ~FedCmFetchingParams();

    blink::mojom::RpMode rp_mode;
    int icon_ideal_size;
    int icon_minimum_size;
    MediationRequirement mediation_requirement;
  };

  AccountsFetcher(
      RenderFrameHost& render_frame_host,
      IdpNetworkRequestManager* network_manager,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      FedCmFetchingParams fetching_params,
      RequestService* federated_auth_request_impl);
  ~AccountsFetcher();

  // Fetch well-known, config, accounts and client metadata endpoints for
  // passed-in IdPs. Uses parameters from `token_request_get_infos_`.
  void FetchEndpointsForIdps(const std::set<GURL>& idp_config_urls);

  // Notifies metrics endpoint that either the user did not select the IDP in
  // the prompt or that there was an error in fetching data for the IDP.
  void SendAllFailedTokenRequestMetrics(
      blink::mojom::FederatedAuthRequestResult result,
      bool did_show_ui);
  void SendSuccessfulTokenRequestMetrics(
      const GURL& idp_config_url,
      base::TimeDelta api_call_to_show_dialog_time,
      base::TimeDelta show_dialog_to_continue_clicked_time,
      base::TimeDelta account_selected_to_token_response_time,
      base::TimeDelta api_call_to_token_response_time,
      bool did_show_ui);

 private:
  void OnAllConfigAndWellKnownFetched(
      std::vector<ConfigFetcher::FetchResult> fetch_results);

  void OnAccountsResponseReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      FetchStatus status,
      std::vector<IdentityRequestAccountPtr> accounts);

  void OnAccountsFetchSucceeded(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      FetchStatus status,
      std::vector<IdentityRequestAccountPtr> accounts);

  void OnClientMetadataResponseReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      std::vector<IdentityRequestAccountPtr>&& accounts,
      FetchStatus status,
      IdpNetworkRequestManager::ClientMetadata client_metadata);

  void OnFetchDataForIdpSucceeded(
      const IdpNetworkRequestManager::ClientMetadata& client_metadata,
      std::vector<IdentityRequestAccountPtr> accounts,
      std::unique_ptr<IdentityProviderInfo> idp_info,
      const gfx::Image& rp_brand_icon);

  void FilterAccountsWithLabel(
      const std::string& label,
      std::vector<IdentityRequestAccountPtr>& accounts);
  void FilterAccountsWithLoginHint(
      const std::string& login_hint,
      std::vector<IdentityRequestAccountPtr>& accounts);
  void FilterAccountsWithDomainHint(
      const std::string& domain_hint,
      std::vector<IdentityRequestAccountPtr>& accounts);

  // Computes the login state of accounts. It uses the IDP-provided signal, if
  // it had been populated. Otherwise, it uses the browser knowledge on which
  // accounts are returning and which are not.
  void ComputeLoginStates(const GURL& idp_config_url,
                          std::vector<IdentityRequestAccountPtr>& accounts);

  // Updates the IdpSigninStatus in case of accounts fetch failure and shows a
  // failure UI if applicable.
  void HandleAccountsFetchFailure(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      std::optional<bool> old_idp_signin_status,
      blink::mojom::FederatedAuthRequestResult result,
      std::optional<webid::RequestIdTokenStatus> token_status,
      const FetchStatus& status);

  void OnIdpMismatch(std::unique_ptr<IdentityProviderInfo> idp_info);

  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint,
      blink::mojom::FederatedAuthRequestResult result,
      bool did_show_ui);

  std::unique_ptr<ConfigFetcher> config_fetcher_;

  // Populated in OnAllConfigAndWellKnownFetched().
  base::flat_map<GURL, GURL> metrics_endpoints_;

  // Owned by RequestService.
  raw_ref<RenderFrameHost> render_frame_host_;
  raw_ptr<IdpNetworkRequestManager> network_manager_;
  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_;

  FedCmFetchingParams params_;

  raw_ptr<RequestService> federated_auth_request_impl_;

  base::WeakPtrFactory<AccountsFetcher> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_ACCOUNTS_FETCHER_H_

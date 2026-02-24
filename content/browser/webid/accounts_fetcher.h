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
#include "content/browser/webid/identity_provider_info.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityPermissionContextDelegate;
class FederatedIdentityApiPermissionContextDelegate;
class RenderFrameHost;

namespace webid {

class Metrics;

// A class that fetches accounts from a set of IDPs.
class AccountsFetcher {
 public:
  static constexpr char kWildcardDomainHint[] = "any";

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

  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);

    GURL idp_config_url;
    std::unique_ptr<IdentityProviderInfo> idp_info;
    std::optional<IdpNetworkRequestManager::AccountsResponse> accounts;
    std::vector<IdentityRequestAccountPtr> filtered_accounts;
    std::optional<blink::mojom::FederatedAuthRequestResult> error;
    std::optional<webid::RequestIdTokenStatus> token_status;
    // Whether the callback should be delayed for this result.
    // TODO(crbug.com/475277488): Remove this as callback delay should not be
    // per-result. Also consider removing `show_active_mode_modal_dialog` as
    // well.
    bool should_delay_callback = false;
    bool is_mismatch = false;
    bool show_active_mode_modal_dialog = false;
    base::TimeTicks accounts_fetched_time;
    base::TimeTicks client_metadata_fetched_time;
  };

  using AccountsFetcherCallback =
      base::OnceCallback<void(base::TimeTicks, std::vector<Result>)>;
  using FilterAccountsCallback = base::RepeatingCallback<void(
      const GURL&,
      const GURL&,
      std::vector<scoped_refptr<content::IdentityRequestAccount>>&)>;

  AccountsFetcher(
      RenderFrameHost& render_frame_host,
      IdpNetworkRequestManager* network_manager,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      FedCmFetchingParams fetching_params,
      AccountsFetcherCallback callback);
  ~AccountsFetcher();

  // Fetch well-known, config, accounts and client metadata endpoints for
  // passed-in IdPs. Uses parameters from `token_request_get_infos`.
  void FetchEndpointsForIdps(
      const std::vector<ConfigFetcher::FetchRequest>& idps,
      const base::flat_map<GURL, IdentityProviderGetInfo>&
          token_request_get_infos,
      Metrics* fedcm_metrics,
      const url::Origin& embedding_origin,
      FilterAccountsCallback filter_accounts_callback);

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
      IdpNetworkRequestManager::AccountsResponse accounts);

  void OnAccountsFetchSucceeded(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      FetchStatus status,
      IdpNetworkRequestManager::AccountsResponse accounts,
      std::vector<IdentityRequestAccountPtr> filtered_accounts,
      base::TimeTicks accounts_fetched_time);

  void OnClientMetadataResponseReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      IdpNetworkRequestManager::AccountsResponse&& accounts,
      std::vector<IdentityRequestAccountPtr> filtered_accounts,
      base::TimeTicks accounts_fetched_time,
      FetchStatus status,
      IdpNetworkRequestManager::ClientMetadata client_metadata);

  void OnFetchDataForIdpSucceeded(
      const IdpNetworkRequestManager::ClientMetadata& client_metadata,
      std::vector<IdentityRequestAccountPtr> filtered_accounts,
      base::TimeTicks accounts_fetched_time,
      base::TimeTicks client_metadata_fetched_time,
      IdpNetworkRequestManager::AccountsResponse accounts,
      std::unique_ptr<IdentityProviderInfo> idp_info,
      const gfx::Image& rp_brand_icon);

  void MarkAccountsWithLabel(const std::string& label,
                             std::vector<IdentityRequestAccountPtr>& accounts);
  void MarkAccountsWithLoginHint(
      const std::string& login_hint,
      std::vector<IdentityRequestAccountPtr>& accounts);
  void MarkAccountsWithDomainHint(
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
      const FetchStatus& status,
      std::vector<IdentityRequestAccountPtr> filtered_accounts,
      base::TimeTicks accounts_fetched_time);

  void OnIdpMismatch(base::TimeTicks accounts_fetched_time,
                     std::unique_ptr<IdentityProviderInfo> idp_info);

  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint,
      blink::mojom::FederatedAuthRequestResult result,
      bool did_show_ui);

  // Adds a fetch result to the end of the results_ vector and decrements
  // pending_requests_. If pending_requests_ reaches 0, runs the callback_.
  void AddResult(Result&& result);

  base::flat_map<GURL,
                 std::pair<blink::mojom::FederatedAuthRequestResult,
                           content::webid::RequestIdTokenStatus>>
      idp_config_url_to_result_;

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

  AccountsFetcherCallback callback_;
  FilterAccountsCallback filter_accounts_callback_;

  base::flat_map<GURL, IdentityProviderGetInfo> request_get_infos_;
  raw_ptr<Metrics> fedcm_metrics_;
  url::Origin embedding_origin_;

  int num_pending_requests_ = 0;
  std::vector<Result> results_;
  base::TimeTicks well_known_and_config_fetched_time_;

  base::WeakPtrFactory<AccountsFetcher> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_ACCOUNTS_FETCHER_H_

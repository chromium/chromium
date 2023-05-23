// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedAuthUserInfoRequest;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityAutoReauthnPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class MDocProvider;
class RenderFrameHost;

using MediationRequirement = ::password_manager::CredentialMediationRequirement;

// FederatedAuthRequestImpl handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing DocumentService, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
// Create() creates a self-managed instance of FederatedAuthRequestImpl and
// binds it to the receiver.
class CONTENT_EXPORT FederatedAuthRequestImpl
    : public DocumentService<blink::mojom::FederatedAuthRequest>,
      public FederatedIdentityPermissionContextDelegate::
          IdpSigninStatusObserver,
      public content::FederatedIdentityModalDialogViewDelegate {
 public:
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);
  static FederatedAuthRequestImpl& CreateForTesting(
      RenderFrameHost&,
      FederatedIdentityApiPermissionContextDelegate*,
      FederatedIdentityAutoReauthnPermissionContextDelegate*,
      FederatedIdentityPermissionContextDelegate*,
      IdentityRegistry*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  ~FederatedAuthRequestImpl() override;

  // blink::mojom::FederatedAuthRequest:
  void RequestToken(std::vector<blink::mojom::IdentityProviderGetParametersPtr>
                        idp_get_params_ptrs,
                    MediationRequirement requirement,
                    RequestTokenCallback) override;
  void RequestUserInfo(blink::mojom::IdentityProviderConfigPtr provider,
                       RequestUserInfoCallback) override;
  void CancelTokenRequest() override;
  void ResolveTokenRequest(const std::string& token,
                           ResolveTokenRequestCallback callback) override;
  void LogoutRps(std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
                 LogoutRpsCallback) override;
  void SetIdpSigninStatus(const url::Origin& origin,
                          blink::mojom::IdpSigninStatus status) override;
  void RegisterIdP(const ::GURL& idp, RegisterIdPCallback) override;
  void UnregisterIdP(const ::GURL& idp, UnregisterIdPCallback) override;
  void CloseModalDialogView() override;

  void PreventSilentAccess(PreventSilentAccessCallback callback) override;

  // FederatedIdentityPermissionContextDelegate::IdpSigninStatusObserver:
  void OnIdpSigninStatusChanged(const url::Origin& idp_config_origin,
                                bool idp_signin_status) override;

  void SetTokenRequestDelayForTests(base::TimeDelta delay);
  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);

  // content::FederatedIdentityModalDialogViewDelegate:
  void NotifyClose() override;
  bool NotifyResolve(const std::string& token) override;

  // Rejects the pending request if it has not been resolved naturally yet.
  void OnRejectRequest();

  struct IdentityProviderGetInfo {
    IdentityProviderGetInfo(blink::mojom::IdentityProviderConfigPtr,
                            blink::mojom::RpContext rp_context);
    ~IdentityProviderGetInfo();
    IdentityProviderGetInfo(const IdentityProviderGetInfo&);
    IdentityProviderGetInfo& operator=(const IdentityProviderGetInfo& other);

    blink::mojom::IdentityProviderConfigPtr provider;
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
  };

  struct IdentityProviderInfo {
    IdentityProviderInfo(const blink::mojom::IdentityProviderConfigPtr&,
                         IdpNetworkRequestManager::Endpoints,
                         IdentityProviderMetadata,
                         blink::mojom::RpContext rp_context);
    ~IdentityProviderInfo();
    IdentityProviderInfo(const IdentityProviderInfo&);

    blink::mojom::IdentityProviderConfigPtr provider;
    IdpNetworkRequestManager::Endpoints endpoints;
    IdentityProviderMetadata metadata;
    bool has_failing_idp_signin_status{false};
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
    absl::optional<IdentityProviderData> data;
  };

  // For use by the devtools protocol for browser automation.
  IdentityRequestDialogController* GetDialogController() {
    return request_dialog_controller_.get();
  }

  const std::vector<IdentityProviderData>& GetSortedIdpData() const {
    return idp_data_for_display_;
  }

  bool IsAutoReauthn() { return auto_reauthn_; }

  void AcceptAccountsDialogForDevtools(const GURL& config_url,
                                       const IdentityRequestAccount& account);
  void DismissAccountsDialogForDevtools(bool should_embargo);

  // Check if the scope of the request allows the browser to mediate
  // or delegate (to the IdP) the authorization.
  static bool ShouldMediateAuthz(const std::vector<std::string>& scope);

 private:
  friend class FederatedAuthRequestImplTest;

  struct FetchData {
    FetchData();
    ~FetchData();

    // Set of config URLs of IDPs that have yet to be processed.
    std::set<GURL> pending_idps;

    // Whether accounts endpoint fetch succeeded for at least one IdP.
    bool did_succeed_for_at_least_one_idp{false};

    // Whether the fetch was triggered by an IdP sign-in status update.
    bool for_idp_signin{false};
  };

  FederatedAuthRequestImpl(
      RenderFrameHost&,
      FederatedIdentityApiPermissionContextDelegate*,
      FederatedIdentityAutoReauthnPermissionContextDelegate*,
      FederatedIdentityPermissionContextDelegate*,
      IdentityRegistry*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  bool HasPendingRequest() const;

  // Fetch well-known, config, accounts and client metadata endpoints for
  // passed-in IdPs. Uses parameters from `token_request_get_infos_`.
  // `for_idp_signin` indicates whether the fetch is as a result of an IdP
  // sign-in status update.
  void FetchEndpointsForIdps(const std::set<GURL>& idp_config_urls,
                             bool for_idp_signin);

  void OnAllConfigAndWellKnownFetched(
      std::vector<FederatedProviderFetcher::FetchResult> fetch_results);
  void OnClientMetadataResponseReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      const IdpNetworkRequestManager::AccountList& accounts,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::ClientMetadata client_metadata);

  // Called when all of the data needed to display the FedCM prompt has been
  // fetched for `idp_info`.
  void OnFetchDataForIdpSucceeded(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      const IdpNetworkRequestManager::AccountList& accounts,
      const IdpNetworkRequestManager::ClientMetadata& client_metadata);

  // Called when there is an error in fetching information to show the prompt
  // for a given IDP - `idp_info`.
  void OnFetchDataForIdpFailed(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      blink::mojom::FederatedAuthRequestResult result,
      absl::optional<content::FedCmRequestIdTokenStatus> token_status,
      bool should_delay_callback);

  void MaybeShowAccountsDialog();
  void ShowModalDialog(const GURL& url);

  // Updates the IdpSigninStatus in case of accounts fetch failure and shows a
  // failure UI if applicable.
  void HandleAccountsFetchFailure(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      absl::optional<bool> old_idp_signin_status,
      blink::mojom::FederatedAuthRequestResult result,
      absl::optional<content::FedCmRequestIdTokenStatus> token_status);

  void OnAccountsResponseReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::AccountList accounts);
  void OnAccountSelected(const GURL& idp_config_url,
                         const std::string& account_id,
                         bool is_sign_in);
  void OnDismissFailureDialog(
      blink::mojom::FederatedAuthRequestResult result,
      absl::optional<content::FedCmRequestIdTokenStatus> token_status,
      bool should_delay_callback,
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void OnDialogDismissed(
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void CompleteTokenRequest(blink::mojom::IdentityProviderConfigPtr idp,
                            IdpNetworkRequestManager::FetchStatus status,
                            const std::string& token);
  void OnTokenResponseReceived(blink::mojom::IdentityProviderConfigPtr idp,
                               IdpNetworkRequestManager::FetchStatus status,
                               const std::string& token);
  void OnContinueOnResponseReceived(
      blink::mojom::IdentityProviderConfigPtr idp,
      IdpNetworkRequestManager::FetchStatus status,
      const GURL& url);
  void DispatchOneLogout();
  void OnLogoutCompleted();

  void CompleteRequestWithError(
      blink::mojom::FederatedAuthRequestResult result,
      absl::optional<content::FedCmRequestIdTokenStatus> token_status,
      bool should_delay_callback);

  // Completes request. Displays a dialog if there is an error and the error is
  // during a fetch triggered by an IdP sign-in status change.
  void CompleteRequest(
      blink::mojom::FederatedAuthRequestResult result,
      absl::optional<content::FedCmRequestIdTokenStatus> token_status,
      const absl::optional<GURL>& selected_idp_config_url,
      const std::string& token,
      bool should_delay_callback);
  void CompleteLogoutRequest(blink::mojom::LogoutRpsStatus);
  void CompleteUserInfoRequest(
      RequestUserInfoCallback callback,
      blink::mojom::RequestUserInfoStatus status,
      absl::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info);
  void CompleteMDocRequest(std::string mdoc);

  // Notifies metrics endpoint that either the user did not select the IDP in
  // the prompt or that there was an error in fetching data for the IDP.
  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint,
      blink::mojom::FederatedAuthRequestResult result);

  void CleanUp();

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager();
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();
  std::unique_ptr<MDocProvider> CreateMDocProvider();

  // Creates an inspector issue related to a federated authentication request to
  // the Issues panel in DevTools.
  void AddInspectorIssue(blink::mojom::FederatedAuthRequestResult result);

  // Adds a console error message related to a federated authentication request
  // issue. The Issues panel is preferred, but for now we also surface console
  // error messages since it is much simpler to add.
  // TODO(crbug.com/1294415): When the FedCM API is more stable, we should
  // ensure that the Issues panel contains all of the needed debugging
  // information and then we can remove the console error messages.
  void AddConsoleErrorMessage(blink::mojom::FederatedAuthRequestResult result);

  void MaybeAddResponseCodeToConsole(const char* fetch_description,
                                     int response_code);

  // Computes the login state of accounts. It uses the IDP-provided signal, if
  // it had been populated. Otherwise, it uses the browser knowledge on which
  // accounts are returning and which are not. In either case, this method also
  // reorders accounts so that those that are considered returning users are
  // before users that are not returning.
  void ComputeLoginStateAndReorderAccounts(
      const blink::mojom::IdentityProviderConfigPtr& idp,
      IdpNetworkRequestManager::AccountList& accounts);

  url::Origin GetEmbeddingOrigin() const;

  // Returns true and the `IdentityProviderData` + `IdentityRequestAccount` for
  // the only returning account. Returns false if there are multiple returning
  // accounts or no returning account.
  bool GetSingleReturningAccount(const IdentityProviderData** out_idp_data,
                                 const IdentityRequestAccount** out_account);

  // Check if auto re-authn is available so we can skip fetching accounts if the
  // auto re-authn flow is guaranteed to fail.
  bool ShouldFailBeforeFetchingAccounts(const GURL& config_url);

  // Check if the site requires user mediation due to a previous
  // `preventSilentAccess` call.
  bool RequiresUserMediation();
  void SetRequiresUserMediation(bool requires_user_mediation);

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  std::unique_ptr<IdentityRequestDialogController> request_dialog_controller_;

  // Replacements for testing.
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  std::unique_ptr<IdentityRequestDialogController> mock_dialog_controller_;

  // Helper that records FedCM UMA and UKM metrics. Initialized in the
  // RequestToken() method, so all metrics must be recorded after that.
  std::unique_ptr<FedCmMetrics> fedcm_metrics_;

  // Populated in OnAllConfigAndWellKnownFetched().
  base::flat_map<GURL, GURL> metrics_endpoints_;

  // Populated by MaybeShowAccountsDialog().
  base::flat_map<GURL, std::unique_ptr<IdentityProviderInfo>> idp_infos_;
  std::vector<IdentityProviderData> idp_data_for_display_;

  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityAutoReauthnPermissionContextDelegate>
      auto_reauthn_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;
  raw_ptr<IdentityRegistry> identity_registry_ = nullptr;

  // The account that was selected by the user. This is only applicable to the
  // mediation flow.
  std::string account_id_;
  base::TimeTicks start_time_;
  base::TimeTicks show_accounts_dialog_time_;
  base::TimeTicks select_account_time_;
  base::TimeTicks token_response_time_;
  base::TimeDelta token_request_delay_;
  bool errors_logged_to_console_{false};
  // This gets set at the beginning of a request. It indicates whether we
  // should bypass the delay to notify the renderer, for use in automated
  // tests when the delay is irrelevant to the test but slows it down
  // unncessarily.
  bool should_complete_request_immediately_{false};
  // While there could be both token request and user info request when a user
  // visits a site, it's worth noting that they must be from different render
  // frames. e.g. one top frame rp.example and one iframe idp.example.
  // Therefore,
  // 1. if one of the requests exists, the other one shouldn't. e.g. if the
  // iframe requests user info, it cannot request token at the same time.
  // 2. the user info request should not set "HasPendingRequest" on the page
  // (multiple per page). It's OK to have multiple concurrent user info requests
  // since there's no browser UI involved. e.g. rp.example embeds
  // iframe1.example and iframe2.example. Both iframes can request user info
  // simultaneously.
  RequestTokenCallback auth_request_token_callback_;

  std::unique_ptr<FederatedProviderFetcher> provider_fetcher_;

  // Only one user info request allowed at a time per frame. Can be done in
  // parallel with token requests.
  std::unique_ptr<FederatedAuthUserInfoRequest> user_info_request_;

  base::queue<blink::mojom::LogoutRpsRequestPtr> logout_requests_;
  LogoutRpsCallback logout_callback_;

  // TODO(crbug.com/1361649): Refactor these member variables introduced through
  // the multi IDP prototype implementation to make them less confusing.

  // Parameters passed to RequestToken().
  base::flat_map<GURL, IdentityProviderGetInfo> token_request_get_infos_;

  // Data related to in-progress FetchEndpointsForIdps() fetch.
  FetchData fetch_data_;

  // List of config URLs of IDPs in the same order as the providers specified in
  // the navigator.credentials.get call.
  std::vector<GURL> idp_order_;

  // Auto re-authentication.
  bool auto_reauthn_{false};
  MediationRequirement mediation_requirement_;

  std::unique_ptr<MDocProvider> mdoc_provider_;
  RequestTokenCallback mdoc_request_callback_;

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

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
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedAuthUserInfoRequest;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class RenderFrameHost;

// FederatedAuthRequestImpl handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing DocumentService, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
// Create() creates a self-managed instance of FederatedAuthRequestImpl and
// binds it to the receiver.
class CONTENT_EXPORT FederatedAuthRequestImpl
    : public DocumentService<blink::mojom::FederatedAuthRequest> {
 public:
  static void Create(RenderFrameHost*,
                     mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);
  static FederatedAuthRequestImpl& CreateForTesting(
      RenderFrameHost&,
      FederatedIdentityApiPermissionContextDelegate*,
      FederatedIdentityPermissionContextDelegate*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  FederatedAuthRequestImpl(const FederatedAuthRequestImpl&) = delete;
  FederatedAuthRequestImpl& operator=(const FederatedAuthRequestImpl&) = delete;

  ~FederatedAuthRequestImpl() override;

  // blink::mojom::FederatedAuthRequest:
  void RequestToken(std::vector<blink::mojom::IdentityProviderGetParametersPtr>
                        idp_get_params_ptrs,
                    RequestTokenCallback) override;
  void RequestUserInfo(blink::mojom::IdentityProviderConfigPtr provider,
                       RequestUserInfoCallback) override;
  void CancelTokenRequest() override;
  void LogoutRps(std::vector<blink::mojom::LogoutRpsRequestPtr> logout_requests,
                 LogoutRpsCallback) override;
  void SetIdpSigninStatus(const url::Origin& origin,
                          blink::mojom::IdpSigninStatus status) override;

  void SetTokenRequestDelayForTests(base::TimeDelta delay);
  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);

  // Rejects the pending request if it has not been resolved naturally yet.
  void OnRejectRequest();

  struct IdentityProviderGetInfo {
    IdentityProviderGetInfo(blink::mojom::IdentityProviderConfigPtr,
                            bool prefer_auto_signin,
                            blink::mojom::RpContext rp_context);
    ~IdentityProviderGetInfo();
    IdentityProviderGetInfo(const IdentityProviderGetInfo&);
    IdentityProviderGetInfo& operator=(const IdentityProviderGetInfo& other);

    blink::mojom::IdentityProviderConfigPtr provider;
    bool prefer_auto_signin{false};
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
  };

  struct IdentityProviderInfo {
    IdentityProviderInfo(blink::mojom::IdentityProviderConfigPtr,
                         IdpNetworkRequestManager::Endpoints,
                         IdentityProviderMetadata,
                         bool prefer_auto_signin,
                         blink::mojom::RpContext rp_context);
    ~IdentityProviderInfo();
    IdentityProviderInfo(const IdentityProviderInfo&);

    blink::mojom::IdentityProviderConfigPtr provider;
    IdpNetworkRequestManager::Endpoints endpoints;
    IdentityProviderMetadata metadata;
    bool prefer_auto_signin{false};
    bool has_failing_idp_signin_status{false};
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
    absl::optional<IdentityProviderData> data;
  };

 private:
  friend class FederatedAuthRequestImplTest;

  FederatedAuthRequestImpl(
      RenderFrameHost&,
      FederatedIdentityApiPermissionContextDelegate*,
      FederatedIdentityPermissionContextDelegate*,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>);

  bool HasPendingRequest() const;

  void OnAllConfigAndWellKnownFetched(
      base::flat_map<GURL, IdentityProviderGetInfo> get_infos,
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
  void DispatchOneLogout();
  void OnLogoutCompleted();

  void CompleteRequestWithError(
      blink::mojom::FederatedAuthRequestResult result,
      absl::optional<content::FedCmRequestIdTokenStatus> token_status,
      bool should_delay_callback);
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

  // Notifies metrics endpoint that either the user did not select the IDP in
  // the prompt or that there was an error in fetching data for the IDP.
  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint,
      blink::mojom::FederatedAuthRequestResult result);

  void CleanUp();

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager();
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();

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

  bool ShouldCompleteRequestImmediately();

  // Computes the login state of accounts. It uses the IDP-provided signal, if
  // it had been populated. Otherwise, it uses the browser knowledge on which
  // accounts are returning and which are not. In either case, this method also
  // reorders accounts so that those that are considered returning users are
  // before users that are not returning.
  void ComputeLoginStateAndReorderAccounts(
      const blink::mojom::IdentityProviderConfigPtr& idp,
      IdpNetworkRequestManager::AccountList& accounts);

  url::Origin GetEmbeddingOrigin() const;

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

  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;

  // The account that was selected by the user. This is only applicable to the
  // mediation flow.
  std::string account_id_;
  base::TimeTicks start_time_;
  base::TimeTicks show_accounts_dialog_time_;
  base::TimeTicks select_account_time_;
  base::TimeTicks token_response_time_;
  base::TimeDelta token_request_delay_;
  bool errors_logged_to_console_{false};
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

  // Set of config URLs of IDPs that have yet to be processed.
  std::set<GURL> pending_idps_;
  // List of config URLs of IDPs in the same order as the providers specified in
  // the navigator.credentials.get call.
  std::vector<GURL> idp_order_;

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

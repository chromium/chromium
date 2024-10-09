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
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/federated_provider_fetcher.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

namespace content {

class FederatedAuthDisconnectRequest;
class FederatedAuthUserInfoRequest;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityAutoReauthnPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class RenderFrameHost;

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using MediationRequirement = ::password_manager::CredentialMediationRequirement;
using RpMode = blink::mojom::RpMode;
using TokenError = IdentityCredentialTokenError;

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
  static constexpr char kWildcardDomainHint[] = "any";

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
  void ResolveTokenRequest(const std::optional<std::string>& account_id,
                           const std::string& token,
                           ResolveTokenRequestCallback callback) override;
  void SetIdpSigninStatus(const url::Origin& origin,
                          blink::mojom::IdpSigninStatus status) override;
  void RegisterIdP(const ::GURL& idp, RegisterIdPCallback) override;
  void UnregisterIdP(const ::GURL& idp, UnregisterIdPCallback) override;
  void CloseModalDialogView() override;
  void PreventSilentAccess(PreventSilentAccessCallback callback) override;
  void Disconnect(blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
                  DisconnectCallback) override;

  // FederatedIdentityPermissionContextDelegate::IdpSigninStatusObserver:
  void OnIdpSigninStatusReceived(const url::Origin& idp_config_origin,
                                 bool idp_signin_status) override;

  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  void SetDialogControllerForTests(
      std::unique_ptr<IdentityRequestDialogController> controller);

  // content::FederatedIdentityModalDialogViewDelegate:
  void OnClose() override;
  bool OnResolve(GURL idp_config_url,
                 const std::optional<std::string>& account_id,
                 const std::string& token) override;

  // To be called on the FederatedAuthRequest object corresponding to a
  // popup opened by ShowModalDialog, specifically for the case when
  // ShowModalDialog returned null (particularly Android). In that case,
  // we can only set up the IdentityRegistry object when we get a call
  // from the popup context.
  // Returns false when no identity registry could be created (e.g. this
  // is not in a context created by ShowModalDialog).
  bool SetupIdentityRegistryFromPopup();

  // Rejects the pending request if it has not been resolved naturally yet.
  void OnRejectRequest();

  // Returns whether the API is enabled or not.
  FederatedIdentityApiPermissionContextDelegate::PermissionStatus
  GetApiPermissionStatus();

  struct IdentityProviderGetInfo {
    IdentityProviderGetInfo(blink::mojom::IdentityProviderRequestOptionsPtr,
                            blink::mojom::RpContext rp_context,
                            blink::mojom::RpMode rp_mode);
    ~IdentityProviderGetInfo();
    IdentityProviderGetInfo(const IdentityProviderGetInfo&);
    IdentityProviderGetInfo& operator=(const IdentityProviderGetInfo& other);

    blink::mojom::IdentityProviderRequestOptionsPtr provider;
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
    blink::mojom::RpMode rp_mode{blink::mojom::RpMode::kPassive};
  };

  struct IdentityProviderInfo {
    IdentityProviderInfo(const blink::mojom::IdentityProviderRequestOptionsPtr&,
                         IdpNetworkRequestManager::Endpoints,
                         IdentityProviderMetadata,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode);
    ~IdentityProviderInfo();
    IdentityProviderInfo(const IdentityProviderInfo&);

    blink::mojom::IdentityProviderRequestOptionsPtr provider;
    IdpNetworkRequestManager::Endpoints endpoints;
    IdentityProviderMetadata metadata;
    bool has_failing_idp_signin_status{false};
    blink::mojom::RpContext rp_context{blink::mojom::RpContext::kSignIn};
    blink::mojom::RpMode rp_mode{blink::mojom::RpMode::kPassive};
    IdentityProviderDataPtr data;
  };

  struct IdentityProviderLoginUrlInfo {
    std::string login_hint;
    std::string domain_hint;
  };

  // For use by the devtools protocol for browser automation.
  IdentityRequestDialogController* GetDialogController() {
    return request_dialog_controller_.get();
  }

  const std::vector<IdentityProviderDataPtr>& GetSortedIdpData() const {
    return idp_data_for_display_;
  }

  const std::vector<IdentityRequestAccountPtr>& GetAccounts() const {
    return accounts_;
  }

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum DialogType {
    kNone = 0,
    kSelectAccount = 1,
    kAutoReauth = 2,
    kConfirmIdpLogin = 3,
    kError = 4,
    // Popups are not technically dialogs in the strict sense, but because they
    // are mutually exclusive with browser UI dialogs we use this enum for them
    // as well.
    kLoginToIdpPopup = 5,
    kContinueOnPopup = 6,
    kErrorUrlPopup = 7,
    kMaxValue = kErrorUrlPopup
  };
  DialogType GetDialogType() const { return dialog_type_; }

  enum IdentitySelectionType { kExplicit, kAutoPassive, kAutoActive };

  bool ShouldNotifyDevtoolsForDialogType(DialogType type);

  void AcceptAccountsDialogForDevtools(const GURL& config_url,
                                       const IdentityRequestAccount& account);
  void DismissAccountsDialogForDevtools(bool should_embargo);
  void AcceptConfirmIdpLoginDialogForDevtools();
  void DismissConfirmIdpLoginDialogForDevtools();
  bool UseAnotherAccountForDevtools(const IdentityProviderData& provider);
  bool HasMoreDetailsButtonForDevtools();
  void ClickErrorDialogGotItForDevtools();
  void ClickErrorDialogMoreDetailsForDevtools();
  void DismissErrorDialogForDevtools();

  // Returns a list of fields that we should mediate authorization for. If
  // empty, we should not show a permission request dialog.
  std::vector<IdentityRequestDialogDisclosureField> GetDisclosureFields(
      const blink::mojom::IdentityProviderRequestOptions& provider);

  // Whether we can show the continue_on popup (not using mediation: silent,
  // etc.)
  bool CanShowContinueOnPopup() const;

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
      std::vector<IdentityRequestAccountPtr>&& accounts,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::ClientMetadata client_metadata);

  // Called when all of the data needed to display the FedCM prompt has been
  // fetched for `idp_info`.
  void OnFetchDataForIdpSucceeded(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      std::vector<IdentityRequestAccountPtr>&& accounts,
      const IdpNetworkRequestManager::ClientMetadata& client_metadata);

  // Called when there is an error in fetching information to show the prompt
  // for a given IDP - `idp_info`, but we do not need to show failure UI for the
  // IDP.
  void OnFetchDataForIdpFailed(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      blink::mojom::FederatedAuthRequestResult result,
      std::optional<content::FedCmRequestIdTokenStatus> token_status,
      bool should_delay_callback);

  // Called when there is an error fetching information to show the prompt for a
  // given IDP, and because of the mismatch this IDP must be present in the
  // dialog we show to the user.
  void OnIdpMismatch(std::unique_ptr<IdentityProviderInfo> idp_info);

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
  MaybeAddRegisteredProviders(
      std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>& providers);

  void MaybeShowAccountsDialog();
  void ShowModalDialog(DialogType dialog_type,
                       const GURL& idp_config_url,
                       const GURL& url_to_show);
  void ShowErrorDialog(const GURL& idp_config_url,
                       IdpNetworkRequestManager::FetchStatus status,
                       std::optional<TokenError> error);
  // Called when we should show a failure dialog in the case where a single IDP
  // account fetch resulted in a mismatch with its login status.
  void ShowSingleIdpFailureDialog();
  void OnAccountsDisplayed();

  // Updates the IdpSigninStatus in case of accounts fetch failure and shows a
  // failure UI if applicable.
  void HandleAccountsFetchFailure(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      std::optional<bool> old_idp_signin_status,
      blink::mojom::FederatedAuthRequestResult result,
      std::optional<content::FedCmRequestIdTokenStatus> token_status);

  void OnAccountsResponseReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      IdpNetworkRequestManager::FetchStatus status,
      std::vector<IdentityRequestAccountPtr> accounts);
  // Fetches the account pictures for |accounts| and calls
  // OnFetchDataForIdpSucceeded when done.
  void FetchAccountPictures(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const IdpNetworkRequestManager::ClientMetadata& client_metadata);
  void OnAccountPictureReceived(base::RepeatingClosure cb,
                                GURL url,
                                const gfx::Image& image);
  void OnAllAccountPicturesReceived(
      std::unique_ptr<IdentityProviderInfo> idp_info,
      std::vector<IdentityRequestAccountPtr>&& accounts,
      const IdpNetworkRequestManager::ClientMetadata& client_metadata);
  void OnAccountSelected(const GURL& idp_config_url,
                         const std::string& account_id,
                         bool is_sign_in);
  void OnDismissFailureDialog(
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void OnDismissErrorDialog(
      const GURL& idp_config_url,
      IdpNetworkRequestManager::FetchStatus status,
      std::optional<TokenError> token_error,
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void OnDialogDismissed(
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void CompleteTokenRequest(const GURL& idp_config_url,
                            IdpNetworkRequestManager::FetchStatus status,
                            std::optional<std::string> token,
                            std::optional<TokenError> token_error,
                            bool should_delay_callback);
  void OnTokenResponseReceived(
      blink::mojom::IdentityProviderRequestOptionsPtr idp,
      IdpNetworkRequestManager::FetchStatus status,
      IdpNetworkRequestManager::TokenResult result);
  void OnContinueOnResponseReceived(
      blink::mojom::IdentityProviderRequestOptionsPtr idp,
      IdpNetworkRequestManager::FetchStatus status,
      const GURL& url);

  // Called after we get at token (either from the ID assertion endpoint or
  // from IdentityProvider.resolve) to update our various permissions.
  void MarkUserAsSignedIn(const GURL& idp_config_url,
                          const std::string& account_id);

  void CompleteRequestWithError(
      blink::mojom::FederatedAuthRequestResult result,
      std::optional<content::FedCmRequestIdTokenStatus> token_status,
      std::optional<TokenError> token_error,
      bool should_delay_callback);

  // Completes request. Displays a dialog if there is an error and the error is
  // during a fetch triggered by an IdP sign-in status change.
  void CompleteRequest(
      blink::mojom::FederatedAuthRequestResult result,
      std::optional<content::FedCmRequestIdTokenStatus> token_status,
      std::optional<TokenError> token_error,
      const std::optional<GURL>& selected_idp_config_url,
      const std::string& token,
      bool should_delay_callback);
  void CompleteUserInfoRequest(
      FederatedAuthUserInfoRequest* request,
      RequestUserInfoCallback callback,
      blink::mojom::RequestUserInfoStatus status,
      std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info);

  // Notifies metrics endpoint that either the user did not select the IDP in
  // the prompt or that there was an error in fetching data for the IDP.
  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint,
      blink::mojom::FederatedAuthRequestResult result);
  void SendSuccessfulTokenRequestMetrics(const GURL& idp_config_url);

  void CleanUp();

  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager();
  std::unique_ptr<IdentityRequestDialogController> CreateDialogController();

  // Creates an inspector issue related to a federated authentication request to
  // the Issues panel in DevTools.
  void AddDevToolsIssue(blink::mojom::FederatedAuthRequestResult result);

  // Adds a console error message related to a federated authentication request
  // issue. The Issues panel is preferred, but for now we also surface console
  // error messages since it is much simpler to add.
  void AddConsoleErrorMessage(blink::mojom::FederatedAuthRequestResult result);

  void MaybeAddResponseCodeToConsole(const char* fetch_description,
                                     int response_code);

  // Computes the login state of accounts. It uses the IDP-provided signal, if
  // it had been populated. Otherwise, it uses the browser knowledge on which
  // accounts are returning and which are not.
  void ComputeLoginStates(const GURL& idp_config_url,
                          std::vector<IdentityRequestAccountPtr>& accounts);

  url::Origin GetEmbeddingOrigin() const;

  // Returns true and the `IdentityProviderData` + `IdentityRequestAccount` for
  // the only returning account. Returns false if there are multiple returning
  // accounts or no returning account.
  bool GetAccountForAutoReauthn(IdentityProviderDataPtr* out_idp_data,
                                IdentityRequestAccountPtr* out_account);

  // Check if auto re-authn is available so we can skip fetching accounts if the
  // auto re-authn flow is guaranteed to fail.
  bool ShouldFailBeforeFetchingAccounts(const GURL& config_url);

  // Check if the site requires user mediation due to a previous
  // `preventSilentAccess` call.
  bool RequiresUserMediation();
  void SetRequiresUserMediation(bool requires_user_mediation,
                                base::OnceClosure callback);

  // Trigger a dialog to prompt the user to login to the IdP. `can_append_hints`
  // is true if the caller allows the login url to be augmented with login and
  // domain hints.
  void LoginToIdP(bool can_append_hints,
                  const GURL& idp_config_url,
                  GURL login_url);

  void MaybeShowActiveModeModalDialog(const GURL& idp_config_url,
                                      const GURL& idp_login_url);

  void CompleteDisconnectRequest(DisconnectCallback callback,
                                 blink::mojom::DisconnectStatus status);

  void RecordErrorMetrics(
      blink::mojom::IdentityProviderRequestOptionsPtr idp,
      IdpNetworkRequestManager::FedCmTokenResponseType token_response_type,
      std::optional<IdpNetworkRequestManager::FedCmErrorDialogType>
          error_dialog_type,
      std::optional<IdpNetworkRequestManager::FedCmErrorUrlType>
          error_url_type);

  void OnRegisterIdPPermissionResponse(RegisterIdPCallback callback,
                                       const GURL& idp,
                                       bool accepted);
  void MaybeCreateFedCmMetrics();

  bool IsNewlyLoggedIn(const IdentityRequestAccount& account);

  // Returns whether there are accounts remaining after applying the account
  // label filter.
  bool FilterAccountsWithLabel(
      const std::string& label,
      std::vector<IdentityRequestAccountPtr>& accounts);
  // Returns whether there are accounts remaining after applying the login hint
  // filter.
  bool FilterAccountsWithLoginHint(
      const std::string& login_hint,
      std::vector<IdentityRequestAccountPtr>& accounts);
  // Returns whether there are accounts remaining after applying the domain hint
  // filter.
  bool FilterAccountsWithDomainHint(
      const std::string& domain_hint,
      std::vector<IdentityRequestAccountPtr>& accounts);

  RpMode GetRpMode() const { return rp_mode_; }

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

  // Populated by OnFetchDataForIdpSucceeded() and OnIdpMismatch().
  base::flat_map<GURL, std::unique_ptr<IdentityProviderInfo>> idp_infos_;
  // Populated by MaybeShowAccountsDialog().
  std::vector<IdentityProviderDataPtr> idp_data_for_display_;

  // Populated by OnFetchDataForIdpSucceeded(). Contains the accounts of each
  // IDP. Used to later set accounts_ in the order in which the IDPs are
  // requested.
  base::flat_map<GURL, std::vector<IdentityRequestAccountPtr>> idp_accounts_;
  // The accounts to be displayed by the UI.
  std::vector<IdentityRequestAccountPtr> accounts_;
  // The newly logged in accounts, to be prioritized by the UI. Subset of
  // `accounts_`.
  std::vector<IdentityRequestAccountPtr> new_accounts_;

  // Contains the set of account IDs of an IDP before a login URL is displayed
  // to the user. Used to compute the account ID of the account that the user
  // logs in to. Populated by LoginToIdP().
  base::flat_set<std::string> account_ids_before_login_;

  // Maps the login URL to the info that may be added as query parameters to
  // that URL. Populated by OnAllConfigAndWellKnownFetched().
  base::flat_map<GURL, IdentityProviderLoginUrlInfo> idp_login_infos_;

  // The downloaded image data.
  std::map<GURL, gfx::Image> downloaded_images_;

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
  base::TimeTicks well_known_and_config_fetched_time_;
  base::TimeTicks accounts_fetched_time_;
  base::TimeTicks client_metadata_fetched_time_;
  base::TimeTicks ready_to_display_accounts_dialog_time_;
  base::TimeTicks accounts_dialog_display_time_;
  base::TimeTicks select_account_time_;
  base::TimeTicks id_assertion_response_time_;
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

  // Set of pending user info requests.
  base::flat_set<std::unique_ptr<FederatedAuthUserInfoRequest>>
      user_info_requests_;

  // Pending disconnect request.
  std::unique_ptr<FederatedAuthDisconnectRequest> disconnect_request_;

  // TODO(crbug.com/40238075): Refactor these member variables introduced
  // through the multi IDP prototype implementation to make them less confusing.

  // Parameters passed to RequestToken().
  base::flat_map<GURL, IdentityProviderGetInfo> token_request_get_infos_;

  // Data related to in-progress FetchEndpointsForIdps() fetch.
  FetchData fetch_data_;

  // List of config URLs of IDPs in the same order as the providers specified in
  // the navigator.credentials.get call.
  std::vector<GURL> idp_order_;

  // If dialog_type_ is kConfirmIdpLogin, this is the login URL for the IDP. If
  // LoginToIdp() is called, this is the login URL for the IDP.
  GURL login_url_;

  // If dialog_type_ is kError or a popup is open, this is the config URL for
  // the IDP.
  GURL config_url_;

  // If dialog_type_ is kError, this is the fetch status of the token request.
  IdpNetworkRequestManager::FetchStatus token_request_status_;

  // If dialog_type_ is kError, this is the token error.
  std::optional<TokenError> token_error_;

  DialogType dialog_type_ = kNone;
  MediationRequirement mediation_requirement_;
  IdentitySelectionType identity_selection_type_ = kExplicit;
  RpMode rp_mode_{RpMode::kPassive};

  // Time when the accounts dialog is last shown for metrics purposes.
  std::optional<base::TimeTicks> accounts_dialog_shown_time_;

  // Time when the mismatch dialog is last shown for metrics purposes.
  std::optional<base::TimeTicks> mismatch_dialog_shown_time_;
  // Whether a mismatch dialog has been shown for the current request.
  bool has_shown_mismatch_{false};

  // Type of error URL for metrics and devtools issue purposes.
  std::optional<IdpNetworkRequestManager::FedCmErrorUrlType> error_url_type_;

  // Number of navigator.credentials.get() requests made for metrics purposes.
  // Requests made when there is a pending FedCM request or for the purpose of
  // Wallets or multi-IDP are not counted.
  int num_requests_{0};

  // The active flow requires user activation to be kicked off. We'd also need
  // this information along the way. e.g. showing pop-up window when accounts
  // fetch is failed. However, the function `HasTransientUserActivation` may
  // return false at that time because the network requests may be very slow
  // such that the previous user gesture is expired. Therefore we store the
  // information to use it during the entire the active flow.
  bool had_transient_user_activation_{false};

  base::WeakPtrFactory<FederatedAuthRequestImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_REQUEST_IMPL_H_

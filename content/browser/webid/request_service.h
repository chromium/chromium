// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
#define CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/webid/accounts_fetcher.h"
#include "content/browser/webid/delegation/federated_sd_jwt_handler.h"
#include "content/browser/webid/identity_provider_info.h"
#include "content/browser/webid/identity_registry.h"
#include "content/browser/webid/identity_registry_delegate.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/browser/webid/idp_registration_handler.h"
#include "content/browser/webid/metrics.h"
#include "content/browser/webid/url_computations.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/autofill_source.h"
#include "content/public/browser/webid/federated_identity_api_permission_context_delegate.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

class FederatedIdentityAutoReauthnPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
class NavigationHandle;
class RenderFrameHost;

namespace webid {

class DisconnectRequest;
class UserInfoRequest;

using blink::mojom::IdentityProviderGetParametersPtr;
using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityProviderGetInfo = AccountsFetcher::IdentityProviderGetInfo;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using MediationRequirement = ::password_manager::CredentialMediationRequirement;
using RpMode = blink::mojom::RpMode;
using TokenError = IdentityCredentialTokenError;

// RequestService handles mojo connections from the renderer to
// fulfill WebID-related requests.
//
// In practice, it is owned and managed by a RenderFrameHost. It accomplishes
// that via subclassing DocumentUserData, which observes the lifecycle of a
// RenderFrameHost and manages its own memory.
class CONTENT_EXPORT RequestService
    : public DocumentUserData<RequestService>,
      public blink::mojom::FederatedAuthRequest,
      public content::FederatedIdentityPermissionContextDelegate::
          IdpSigninStatusObserver,
      public IdentityRegistryDelegate,
      public webid::AutofillSource {
 public:
  static constexpr char kWildcardDomainHint[] = "any";

  DOCUMENT_USER_DATA_KEY_DECL();

  explicit RequestService(RenderFrameHost* rfh);

  RequestService(
      RenderFrameHost* rfh,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      FederatedIdentityAutoReauthnPermissionContextDelegate*
          auto_reauthn_permission_delegate,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      IdentityRegistry* identity_registry);

  RequestService(const RequestService&) = delete;
  RequestService& operator=(const RequestService&) = delete;

  ~RequestService() override;

  // Creates a RequestService for testing and binds it to the receiver.
  static RequestService& CreateForTesting(
      RenderFrameHost& rfh,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      FederatedIdentityAutoReauthnPermissionContextDelegate*
          auto_reauthn_permission_delegate,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      IdentityRegistry* identity_registry,
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>
          pending_receiver);

  void BindReceiver(mojo::PendingReceiver<blink::mojom::FederatedAuthRequest>
                        pending_receiver);

  void ReportBadMessage(const char* message);

  // Unassociates and deletes `this` from the document for use by tests.
  // TODO(crbug.com/459135671): Change tests to navigate instead and remove
  // this method.
  void ResetAndDeleteThisForTesting();

  // An overload of the mojo version of RequestToken. If |navigation_handle|
  // is provided, that handle is checked to see if user activation is present.
  // This is virtual so that it can be mocked.MockNavigationThrottleRegistry
  virtual void RequestToken(
      std::vector<blink::mojom::IdentityProviderGetParametersPtr>
          idp_get_params_ptrs,
      MediationRequirement requirement,
      NavigationHandle* navigation_handle,
      RequestTokenCallback);

  // blink::mojom::FederatedAuthRequest:
  void RequestToken(std::vector<blink::mojom::IdentityProviderGetParametersPtr>
                        idp_get_params_ptrs,
                    MediationRequirement requirement,
                    RequestTokenCallback) override;
  void RequestUserInfo(blink::mojom::IdentityProviderConfigPtr provider,
                       RequestUserInfoCallback) override;
  void CancelTokenRequest() override;
  void ResolveTokenRequest(const std::optional<std::string>& account_id,
                           base::Value token,
                           ResolveTokenRequestCallback callback) override;
  void SetIdpSigninStatus(
      const url::Origin& origin,
      blink::mojom::IdpSigninStatus status,
      const std::optional<::blink::common::webid::LoginStatusOptions>& options,
      SetIdpSigninStatusCallback) override;
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
                 const base::Value& token) override;
  void OnOriginMismatch(Method method,
                        const url::Origin& expected,
                        const url::Origin& actual) override;

  // content::webid::AutofillSource
  const std::optional<std::vector<IdentityRequestAccountPtr>>
  GetAutofillSuggestions() const override;
  void NotifyAutofillSuggestionAccepted(
      const GURL& idp,
      const std::string& account_id,
      bool show_modal,
      OnFederatedTokenReceivedCallback callback) override;

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

  MediationRequirement GetMediationRequirement() const {
    return mediation_requirement_;
  }

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DialogType)

  enum class DialogType {
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

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmDialogType)

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

  // Whether we can show the continue_on popup (not using mediation: silent,
  // etc.)
  bool CanShowContinueOnPopup() const;

  bool HasUserTriedToSignInToIdp(const GURL& idp_config_url) {
    return idps_user_tried_to_signin_to_.contains(idp_config_url);
  }

  UseOtherAccountResult ComputeUseOtherAccountResult(
      blink::mojom::FederatedAuthRequestResult result,
      const std::optional<GURL>& selected_idp_config_url);

  void SetIdpLoginInfo(const GURL& idp_login_url,
                       const std::string& login_hint,
                       const std::string& domain_hint);
  void SetWellKnownAndConfigFetchedTime(base::TimeTicks time);

  // Called when there is an error in fetching information to show the prompt
  // for a given IDP - `idp_info`, but we do not need to show failure UI for the
  // IDP.
  void OnFetchDataForIdpFailed(std::unique_ptr<IdentityProviderInfo> idp_info,
                               blink::mojom::FederatedAuthRequestResult result,
                               std::optional<RequestIdTokenStatus> token_status,
                               bool should_delay_callback);

  // Called when all of the data needed to display the FedCM prompt has been
  // fetched for `idp_info`. Accounts should be moved instead of copied to this
  // function.
  void OnFetchDataForIdpSucceeded(
      std::vector<IdentityRequestAccountPtr> accounts,
      std::unique_ptr<IdentityProviderInfo> idp_info);

  void MaybeShowActiveModeModalDialog(const GURL& idp_config_url,
                                      const GURL& idp_login_url);

  void SetAccountsFetchedTime(base::TimeTicks time) {
    accounts_fetched_time_ = time;
  }
  void SetClientMetadataFetchedTime(base::TimeTicks time) {
    client_metadata_fetched_time_ = time;
  }

  url::Origin GetEmbeddingOrigin() const;

  // TODO(crbug.com/417197032): Remove these once code has been refactored.
  base::flat_map<GURL, IdentityProviderGetInfo>& GetTokenRequestGetInfos() {
    return token_request_get_infos_;
  }
  GURL login_url() { return login_url_; }
  bool HadAccountIdBeforeLogin(const std::string& account_id) {
    return account_ids_before_login_.contains(account_id);
  }
  // Return the FedCmMetrics for use by FedCmAccountsFetcher.
  // TODO(crbug.com/417784830): Remove this once code has been refactored and
  // FedCmAccountsFetcher can hold a raw pointer to FedCmMetrics.
  Metrics* fedcm_metrics() { return fedcm_metrics_.get(); }

  // Called when there is an error fetching information to show the prompt for a
  // given IDP, and because of the mismatch this IDP must be present in the
  // dialog we show to the user.
  void OnIdpMismatch(std::unique_ptr<IdentityProviderInfo> idp_info);

  void CompleteRequestWithError(
      blink::mojom::FederatedAuthRequestResult result,
      std::optional<RequestIdTokenStatus> token_status,
      bool should_delay_callback);

  // Completes request. Displays a dialog if there is an error and the error is
  // during a fetch triggered by an IdP sign-in status change.
  void CompleteRequest(blink::mojom::FederatedAuthRequestResult result,
                       std::optional<RequestIdTokenStatus> token_status,
                       std::optional<TokenError> token_error,
                       const std::optional<GURL>& selected_idp_config_url,
                       std::optional<base::Value> token_data,
                       bool should_delay_callback);

 private:
  friend class RequestServiceTest;

  struct FetchData {
    FetchData();
    ~FetchData();

    // Set of config URLs of IDPs that have yet to be processed.
    std::set<GURL> pending_idps;

    // Whether accounts endpoint fetch succeeded for at least one IdP.
    bool did_succeed_for_at_least_one_idp{false};
  };

  bool HasPendingRequest() const;

  // Fetch well-known, config, accounts and client metadata endpoints for
  // passed-in IdPs. Uses parameters from `token_request_get_infos_`.
  void FetchEndpointsForIdps(const std::set<GURL>& idp_config_urls);

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
  MaybeAddRegisteredProviders(
      std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>& providers);

  void MaybeShowAccountsDialog();
  void OnShouldShowAccountsPassiveDialogResult(
      const std::set<GURL>& unique_idps,
      bool should_show);
  // To be called immediately after ShowAccountsDialog for correct devtools
  // integration and metrics reporting.
  // `did_succeed_for_at_least_one_idp` needs to be passed as a parameter
  // because `fetch_data_` has been cleared at this point.
  void AfterAccountsDialogShown(bool did_succeed_for_at_least_one_idp);
  void ShowModalDialog(DialogType dialog_type,
                       const GURL& idp_config_url,
                       const GURL& url_to_show);
  void ShowErrorDialog(const GURL& idp_config_url,
                       FetchStatus status,
                       std::optional<TokenError> error);
  // Called when we should show a failure dialog in the case where a single IDP
  // account fetch resulted in a mismatch with its login status.
  void ShowSingleIdpFailureDialog();
  void OnAccountsDisplayed();

  void OnAccountSelected(const GURL& idp_config_url,
                         const std::string& account_id,
                         bool is_sign_in);
  void OnDismissFailureDialog(
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void OnDismissErrorDialog(
      const GURL& idp_config_url,
      FetchStatus status,
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void OnDialogDismissed(
      IdentityRequestDialogController::DismissReason dismiss_reason);
  void CompleteTokenRequest(const GURL& idp_config_url,
                            FetchStatus status,
                            std::optional<base::Value> token,
                            std::optional<TokenError> token_error,
                            bool should_delay_callback);
  void OnTokenResponseReceived(
      blink::mojom::IdentityProviderRequestOptionsPtr idp,
      FetchStatus status,
      IdpNetworkRequestManager::TokenResult&& result);
  void OnContinueOnResponseReceived(
      blink::mojom::IdentityProviderRequestOptionsPtr idp,
      FetchStatus status,
      const GURL& url);

  // Called after we get at token (either from the ID assertion endpoint or
  // from IdentityProvider.resolve) to update our various permissions.
  void MarkUserAsSignedIn(const GURL& idp_config_url,
                          const std::string& account_id);

  void CompleteUserInfoRequest(
      UserInfoRequest* request,
      RequestUserInfoCallback callback,
      blink::mojom::RequestUserInfoStatus status,
      std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info);

  // Validates the input from the renderer and signals to terminate the request
  // if needed.
  bool ShouldTerminateRequest(
      const std::vector<IdentityProviderGetParametersPtr>& idp_get_params_ptrs,
      const MediationRequirement& requirement);

  // If a new request is associated with active mode, it can replace the pending
  // request with passive mode. Otherwise a new request will be cancelled when
  // there's a pending request. Returns `true` if the new request needs to be
  // cancelled.
  bool HandlePendingRequestAndCancelNewRequest(
      const std::vector<GURL>& old_idp_order,
      const std::vector<IdentityProviderGetParametersPtr>& idps,
      const MediationRequirement& requirement);

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

  void CompleteDisconnectRequest(DisconnectCallback callback,
                                 blink::mojom::DisconnectStatus status);

  void RecordErrorMetrics(
      blink::mojom::IdentityProviderRequestOptionsPtr idp,
      IdpNetworkRequestManager::FedCmTokenResponseType token_response_type,
      std::optional<IdpNetworkRequestManager::FedCmErrorDialogType>
          error_dialog_type,
      std::optional<IdpNetworkRequestManager::FedCmErrorUrlType>
          error_url_type);

  void OnIdpRegistrationConfigFetched(
      RegisterIdPCallback callback,
      const GURL& idp,
      std::vector<ConfigFetcher::FetchResult> fetch_results);
  void OnRegisterIdPPermissionResponse(RegisterIdPCallback callback,
                                       const GURL& idp,
                                       bool accepted);
  std::unique_ptr<Metrics> CreateFedCmMetrics();

  bool IsNewlyLoggedIn(const IdentityRequestAccount& account);

  RpMode GetRpMode() const { return rp_mode_; }

  // If the client metadata has not been received yet the UI may not be able to
  // show a correct title, so we need to indicate that in the RelyingPartyData.
  RelyingPartyData CreateRpData(bool client_metadata_received) const;

  std::unique_ptr<IdpNetworkRequestManager> network_manager_;
  std::unique_ptr<IdentityRequestDialogController> request_dialog_controller_;

  // Replacements for testing.
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  std::unique_ptr<IdentityRequestDialogController> mock_dialog_controller_;

  // Helper that records FedCM UMA and UKM metrics. Initialized in the
  // RequestToken() method, so all metrics must be recorded after that.
  std::unique_ptr<Metrics> fedcm_metrics_;

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

  // Contains the set of account IDs of an IDP before a login URL is displayed
  // to the user. Used to compute the account ID of the account that the user
  // logs in to. Populated by LoginToIdP().
  base::flat_set<std::string> account_ids_before_login_;

  // Maps the login URL to the info that may be added as query parameters to
  // that URL. Populated by OnAllConfigAndWellKnownFetched().
  base::flat_map<GURL, IdentityProviderLoginUrlInfo> idp_login_infos_;

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

  OnFederatedTokenReceivedCallback token_received_callback_for_autofill_;

  std::unique_ptr<AccountsFetcher> fedcm_accounts_fetcher_;

  std::unique_ptr<FederatedSdJwtHandler> federated_sdjwt_handler_;

  std::unique_ptr<IdpRegistrationHandler> fedcm_idp_registration_handler_;

  // Set of pending user info requests.
  base::flat_set<std::unique_ptr<UserInfoRequest>> user_info_requests_;

  // Pending disconnect request.
  std::unique_ptr<DisconnectRequest> disconnect_request_;

  // TODO(crbug.com/40238075): Refactor these member variables introduced
  // through the multi IDP prototype implementation to make them less confusing.

  // Parameters passed to RequestToken().
  base::flat_map<GURL, IdentityProviderGetInfo> token_request_get_infos_;

  // Data related to in-progress FetchEndpointsForIdps() fetch.
  FetchData fetch_data_;

  // The set of IDPs that the user has tried to sign in to since the start of
  // the current request.
  base::flat_set<GURL> idps_user_tried_to_signin_to_;

  // List of config URLs of IDPs in the same order as the providers specified in
  // the navigator.credentials.get call.
  std::vector<GURL> idp_order_;

  // If dialog_type_ is kConfirmIdpLogin, this is the login URL for the IDP. If
  // LoginToIdp() is called, this is the login URL for the IDP. Does not include
  // the filters as query parameters, if any.
  GURL login_url_;

  // If dialog_type_ is kError or a popup is open, this is the config URL for
  // the IDP.
  GURL config_url_;

  // If dialog_type_ is kError, this is the fetch status of the token request.
  FetchStatus token_request_status_;

  // If dialog_type_ is kError, this is the token error.
  std::optional<TokenError> token_error_;

  DialogType dialog_type_ = DialogType::kNone;
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

  // Keeps track of the state of the use other account flow. Is std::nullopt
  // when the flow is not active.
  std::optional<UseOtherAccountResult> use_other_account_account_result_;

  // Whether a token request has been sent.
  bool has_sent_token_request_{false};

  // Keeps track of the state of the verifying dialog. Is std::nullopt when the
  // verifying dialog has not been shown.
  std::optional<VerifyingDialogResult> verifying_dialog_result_;

  perfetto::NamedTrack perfetto_track_;

  mojo::Receiver<blink::mojom::FederatedAuthRequest> receiver_{this};

  base::WeakPtrFactory<RequestService> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_

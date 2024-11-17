// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_
#define CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_

#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace base {
class TimeDelta;
}

namespace content {

using IdentityProviderDataPtr = scoped_refptr<IdentityProviderData>;
using MediationRequirement = ::password_manager::CredentialMediationRequirement;
using RpMode = blink::mojom::RpMode;

// This enum describes the status of a request id token call to the FedCM API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmRequestIdTokenStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kSuccessUsingTokenInHttpResponse = 0,
  kTooManyRequests = 1,
  kAborted = 2,
  kUnhandledRequest = 3,
  kIdpNotPotentiallyTrustworthy = 4,
  kNotSelectAccount = 5,
  kConfigHttpNotFound = 6,
  kConfigNoResponse = 7,
  kConfigInvalidResponse = 8,
  kClientMetadataHttpNotFound = 9,      // obsolete
  kClientMetadataNoResponse = 10,       // obsolete
  kClientMetadataInvalidResponse = 11,  // obsolete
  kAccountsHttpNotFound = 12,
  kAccountsNoResponse = 13,
  kAccountsInvalidResponse = 14,
  kIdTokenHttpNotFound = 15,
  kIdTokenNoResponse = 16,
  kIdTokenInvalidResponse = 17,
  kIdTokenInvalidRequest = 18,                  // obsolete
  kClientMetadataMissingPrivacyPolicyUrl = 19,  // obsolete
  kThirdPartyCookiesBlocked = 20,               // obsolete
  kDisabledInSettings = 21,
  kDisabledInFlags = 22,
  kWellKnownHttpNotFound = 23,
  kWellKnownNoResponse = 24,
  kWellKnownInvalidResponse = 25,
  kConfigNotInWellKnown = 26,
  kWellKnownTooBig = 27,
  kDisabledEmbargo = 28,
  kUserInterfaceTimedOut = 29,  // obsolete
  kRpPageNotVisible = 30,
  kShouldEmbargo = 31,
  kNotSignedInWithIdp = 32,
  kAccountsListEmpty = 33,
  kWellKnownListEmpty = 34,
  kWellKnownInvalidContentType = 35,
  kConfigInvalidContentType = 36,
  kAccountsInvalidContentType = 37,
  kIdTokenInvalidContentType = 38,
  kSilentMediationFailure = 39,
  kIdTokenIdpErrorResponse = 40,
  kIdTokenCrossSiteIdpErrorResponse = 41,
  kOtherIdpChosen = 42,
  kMissingTransientUserActivation = 43,
  kReplacedByActiveMode = 44,
  kContinuationPopupClosedByUser = 45,
  kSuccessUsingIdentityProviderResolve = 46,
  kContinuationPopupClosedByIdentityProviderClose = 47,
  kInvalidFieldsSpecified = 48,
  kRpOriginIsOpaque = 49,
  kConfigNotMatchingType = 50,

  kMaxValue = kConfigNotMatchingType
};

// This enum describes whether user sign-in states between IDP and browser
// match.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmSignInStateMatchStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kMatch = 0,
  kIdpClaimedSignIn = 1,
  kBrowserObservedSignIn = 2,

  kMaxValue = kBrowserObservedSignIn
};

// This enum describes whether the browser's knowledge of whether the user is
// signed into the IDP based on observing signin/signout HTTP headers matches
// the information returned by the accounts endpoint.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmIdpSigninMatchStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kMatchWithAccounts = 0,
  kMatchWithoutAccounts = 1,
  kUnknownStatusWithAccounts = 2,
  kUnknownStatusWithoutAccounts = 3,
  kMismatchWithNetworkError = 4,
  kMismatchWithNoContent = 5,
  kMismatchWithInvalidResponse = 6,
  kMismatchWithUnexpectedAccounts = 7,

  kMaxValue = kMismatchWithUnexpectedAccounts
};

// This enum describes the type of frame that invokes a FedCM API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmRequesterFrameType {
  // Do not change the meaning or order of these values since they are being
  // recorded in metrics and in sync with the counterpart in enums.xml.
  kMainFrame = 0,
  kSameSiteIframe = 1,
  kCrossSiteIframe = 2,

  kMaxValue = kCrossSiteIframe
};

// This enum describes the status of a disconnect call to the FedCM API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmDisconnectStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kSuccess = 0,
  kTooManyRequests = 1,
  kUnhandledRequest = 2,
  kNoAccountToDisconnect = 3,
  kDisconnectUrlIsCrossOrigin = 4,
  kDisconnectFailedOnServer = 5,
  kConfigHttpNotFound = 6,
  kConfigNoResponse = 7,
  kConfigInvalidResponse = 8,
  kDisabledInSettings = 9,
  kDisabledInFlags = 10,
  kWellKnownHttpNotFound = 11,
  kWellKnownNoResponse = 12,
  kWellKnownInvalidResponse = 13,
  kWellKnownListEmpty = 14,
  kConfigNotInWellKnown = 15,
  kWellKnownTooBig = 16,
  kWellKnownInvalidContentType = 17,
  kConfigInvalidContentType = 18,
  kIdpNotPotentiallyTrustworthy = 19,

  kMaxValue = kIdpNotPotentiallyTrustworthy
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmSetLoginStatusIgnoredReason {
  kFrameTreeLookupFailed = 0,
  kInFencedFrame = 1,
  kCrossOrigin = 2,

  kMaxValue = kCrossOrigin
};

// This enum describes the result of the error dialog.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmErrorDialogResult {
  kMoreDetails = 0,
  kGotItWithoutMoreDetails = 1,
  kGotItWithMoreDetails = 2,
  kCloseWithoutMoreDetails = 3,
  kCloseWithMoreDetails = 4,
  kSwipeWithoutMoreDetails = 5,
  kSwipeWithMoreDetails = 6,
  kOtherWithoutMoreDetails = 7,
  kOtherWithMoreDetails = 8,

  kMaxValue = kOtherWithMoreDetails
};

// Whether we were able to open the continue_on popup and the reason if not.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmContinueOnPopupStatus {
  kPopupOpened = 0,
  kUrlNotSameOrigin = 1,
  kPopupNotAllowed = 2,
  kUrlNotSameOriginAndPopupNotAllowed = 3,

  kMaxValue = kUrlNotSameOriginAndPopupNotAllowed
};

// The result of the continue_on popup.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmContinueOnPopupResult {
  kTokenReceived = 0,
  kWindowClosed = 1,
  kClosedByIdentityProviderClose = 2,

  kMaxValue = kClosedByIdentityProviderClose
};

// This enum is used when we fail a FedCM request due to a bad
// lifecycle state.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmLifecycleStateFailureReason {
  kOther = 0,
  kSpeculative = 1,
  kPendingCommit = 2,
  kPrerendering = 3,
  kInBackForwardCache = 4,
  kRunningUnloadHandlers = 5,
  kReadyToBeDeleted = 6,

  kMaxValue = kReadyToBeDeleted
};

// This enum is used when a token request is invoked while there's a pending
// one. These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FedCmMultipleRequestsRpMode {
  kPassiveThenPassive = 0,
  kPassiveThenActive = 1,
  kActiveThenPassive = 2,
  kActiveThenActive = 3,

  kMaxValue = kActiveThenActive
};

// This enum tracks whether the RP requested additional scopes and/or
// parameters. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class FedCmRpParameters {
  kHasParameters = 0,
  kHasNonDefaultScope = 1,
  kHasParametersAndNonDefaultScope = 2,

  kMaxValue = kHasParametersAndNonDefaultScope
};

class CONTENT_EXPORT FedCmMetrics {
 public:
  explicit FedCmMetrics(const ukm::SourceId page_source_id);

  ~FedCmMetrics();

  // Records the number of times navigator.credentials.get() is called in a
  // document. Requests made when FedCM is disabled or when there is a pending
  // FedCM request are not counted.
  static void RecordNumRequestsPerDocument(ukm::SourceId page_source_id,
                                           const int num_requests);

  // Records whether the browser's knowledge of whether the user is signed into
  // the IDP based on observing signin/signout HTTP headers matches the
  // information returned by the accounts endpoint.
  static void RecordIdpSigninMatchStatus(
      std::optional<bool> idp_signin_status,
      IdpNetworkRequestManager::ParseStatus accounts_endpoint_status);

  void SetSessionID(int session_id);

  // Records the time from when a call to the API was made to when the accounts
  // dialog is shown. This does not include flows that involve LoginToIdP. e.g.
  // mismatch flow or active flow with users whose login status is "logged-out".
  void RecordShowAccountsDialogTime(
      const std::vector<IdentityProviderDataPtr>& providers,
      base::TimeDelta duration);

  // Records the time from when a call to the API was made to when the accounts
  // dialog is shown in breakdown. In case of multi-IdP, this records the max
  // time across IdPs. This does not include flows that involve LoginToIdP. e.g.
  // mismatch flow or active flow with users whose login status is "logged-out".
  void RecordShowAccountsDialogTimeBreakdown(
      base::TimeDelta well_known_and_config_fetch_duration,
      base::TimeDelta accounts_fetch_duration,
      base::TimeDelta client_metadata_fetch_duration);

  // Records the time from when a call to the API was made to when the
  // well-known and config files are fetched. This helps with measuring when the
  // login_url could be available.
  void RecordWellKnownAndConfigFetchTime(base::TimeDelta duration);

  // Records the time from when the accounts dialog is shown to when the user
  // presses the Continue active of an account of the given provider.
  void RecordContinueOnPopupTime(const GURL& provider,
                                 base::TimeDelta duration);

  // Records metrics when the user explicitly closes the accounts dialog without
  // selecting any accounts. `duration` is the time from when the accounts
  // dialog was shown to when the user closed the dialog.
  void RecordCancelOnDialogTime(
      const std::vector<IdentityProviderDataPtr>& providers,
      base::TimeDelta duration);

  // Records the duration from when an accounts dialog is shown to when it is
  // destroyed.
  void RecordAccountsDialogShownDuration(
      const std::vector<IdentityProviderDataPtr>& providers,
      base::TimeDelta duration);

  // Records the duration from when a mismatch dialog is shown to when it is
  // destroyed or user triggers IDP sign-in pop-up window.
  void RecordMismatchDialogShownDuration(
      const std::vector<IdentityProviderDataPtr>& providers,
      base::TimeDelta duration);

  // Records the reason that closed accounts dialog without selecting any
  // accounts. Unlike RecordCancelOnDialogTime() this metric is recorded in
  // cases that the acccounts dialog was closed without an explicit user action.
  void RecordCancelReason(
      IdentityRequestDialogController::DismissReason dismiss_reason);

  // Records the time from when the user presses the Continue active to when the
  // token response is received. Also records the overall time from when the API
  // is called to when the token response is received.
  void RecordTokenResponseAndTurnaroundTime(const GURL& provider,
                                            base::TimeDelta token_response_time,
                                            base::TimeDelta turnaround_time);

  // Records the time from when the user presses the Continue active to when
  // the continue_on response is received. Also records the overall time from
  // when the API is called to when the IdentityProvider.resolve token is
  // received.
  void RecordContinueOnResponseAndTurnaroundTime(
      base::TimeDelta token_response_time,
      base::TimeDelta turnaround_time);

  // Records the status of the |RequestToken| call. Also records the number of
  // IDPs requested and the number of IDPs for which a mismatch was found.
  // |requested_providers| contains all IDPs that were requested in the get()
  // call.
  void RecordRequestTokenStatus(
      FedCmRequestIdTokenStatus status,
      MediationRequirement requirement,
      const std::vector<GURL>& requested_providers,
      int num_idps_mismatch,
      const std::optional<GURL>& selected_idp_config_url,
      const RpMode& rp_mode);

  // Records whether user sign-in states between IDP and browser match.
  void RecordSignInStateMatchStatus(const GURL& provider,
                                    FedCmSignInStateMatchStatus status);

  // Records whether the user selected account is for sign-in or not.
  void RecordIsSignInUser(bool is_sign_in);

  // Records whether the frame is visible or active upon ready to show accounts
  // UI.
  void RecordWebContentsStatusUponReadyToShowDialog(bool is_visible,
                                                    bool is_active);

  // This enum is used in histograms. Do not remove or modify existing entries.
  // You may add entries at the end, and update |kMaxValue|.
  enum class NumAccounts {
    kZero = 0,
    kOne = 1,
    kMultiple = 2,
    kMaxValue = kMultiple
  };

  // Records several auto reauthn metrics using the given parameters.
  // |has_single_returning_account| is nullopt when we are recording the metrics
  // during a failure that happened before the accounts fetch.
  void RecordAutoReauthnMetrics(
      std::optional<bool> has_single_returning_account,
      const IdentityRequestAccount* auto_signin_account,
      bool auto_reauthn_success,
      bool is_auto_reauthn_setting_blocked,
      bool is_auto_reauthn_embargoed,
      std::optional<base::TimeDelta> time_from_embargo,
      bool requires_user_mediation);

  // Records a sample when an accounts dialog is shown.
  void RecordAccountsDialogShown(
      const std::vector<IdentityProviderDataPtr>& providers);

  // This enum is used in histograms. Do not remove or modify existing entries.
  // You may add entries at the end, and update |kMaxValue|.
  enum class MismatchDialogType {
    kFirstWithoutHints,
    kFirstWithHints,
    kRepeatedWithoutHints,
    kRepeatedWithHints,

    kMaxValue = kRepeatedWithHints
  };

  // Records a sample when a single IDP mismatch dialog is shown. Also records
  // whether this is a mismatch seen for the first time or a if there has
  // already been a mismatch dialog for this call. Finally, records when there
  // is a repeated mismatch and hints were requested in the call.
  void RecordSingleIdpMismatchDialogShown(const IdentityProviderData& provider,
                                          bool has_shown_mismatch,
                                          bool has_hints);

  // Records a sample when an accounts request is sent.
  void RecordAccountsRequestSent(const GURL& provider_url);

  // Records metrics for a disconnect call. `duration` is nullopt if the
  // disconnect fetch request was not sent, in which case we do not log the
  // metric. Because this is a separate API from a token request, a different
  // session ID is passed to this metric.
  void RecordDisconnectMetrics(FedCmDisconnectStatus status,
                               std::optional<base::TimeDelta> duration,
                               const RenderFrameHost& rfh,
                               const url::Origin& requester,
                               const url::Origin& embedder,
                               const GURL& provider_url,
                               int disconnect_session_id);

  // Records the status of opening the continue_on dialog.
  void RecordContinueOnPopupStatus(FedCmContinueOnPopupStatus status);

  // Records the outcome of the continue_on dialog.
  void RecordContinueOnPopupResult(FedCmContinueOnPopupResult result);

  // Records whether parameters or scopes were specified.
  void RecordRpParameters(FedCmRpParameters parameters);

  // Records the outcome of the error dialog.
  void RecordErrorDialogResult(FedCmErrorDialogResult result,
                               const GURL& provider_url);

  // Records metrics before the error dialog has been shown.
  void RecordErrorMetricsBeforeShowingErrorDialog(
      IdpNetworkRequestManager::FedCmTokenResponseType response_type,
      std::optional<IdpNetworkRequestManager::FedCmErrorDialogType> dialog_type,
      std::optional<IdpNetworkRequestManager::FedCmErrorUrlType> url_type,
      const GURL& provider_url);

  // Records the RpMode of two consecutive requests when one is invoked while
  // the other is pending.
  void RecordMultipleRequestsRpMode(
      blink::mojom::RpMode pending_request_rp_mode,
      blink::mojom::RpMode new_request_rp_mode,
      const std::vector<GURL>& requested_providers);

  // Records the time from when a User Info API call, if any, most likely upon
  // page load, to when the first Active Mode API is called afterwards, if any.
  void RecordTimeBetweenUserInfoAndActiveModeAPI(base::TimeDelta duration);

  // Records the number of accounts matching a given filter, when the FedCM call
  // involved filtering out accounts with that filter. Filter must be one of
  // "LoginHint", "DomainHint", and "AccountLabel".
  void RecordNumMatchingAccounts(size_t accounts_remaining,
                                 const std::string& filter_type);

  int session_id() { return session_id_; }

 private:
  ukm::SourceId GetOrCreateProviderSourceId(const GURL& provider);

  // The page's SourceId. Used to log the UKM event Blink.FedCm.
  ukm::SourceId page_source_id_;

  // The SourceId to be used to log the UKM event Blink.FedCmIdp. Maps a
  // provider's config URL to its UKM SourceId.
  std::map<GURL, ukm::SourceId> provider_source_ids_;

  // The session ID associated to the FedCM token request for which this object
  // is recording metrics. Each FedCM call gets a random integer session id,
  // which helps group UKM events by the session id.
  int session_id_ = -1;
};

// The following metric is recorded for UMA and UKM, but does not require an
// existing FedCM call. Records metrics associated with a preventSilentAccess()
// call from the given RenderFrameHost.
void RecordPreventSilentAccess(RenderFrameHost& rfh,
                               const url::Origin& requester,
                               const url::Origin& embedder);

// The following are UMA-only recordings, hence do not need to be in the
// FedCmMetrics class.

// Records whether an IDP returns an approved clients list in the response.
void RecordApprovedClientsExistence(bool has_approved_clients);

// Records the size of the approved clients list if applicable.
void RecordApprovedClientsSize(int size);

// Records the net::Error received from the accounts list endpoint when the IDP
// SignIn status is set to SignedOut due to no accounts received.
void RecordIdpSignOutNetError(int response_code);

// Records why there's no valid account in the response.
void RecordAccountsResponseInvalidReason(
    IdpNetworkRequestManager::AccountsResponseInvalidReason reason);

// Records the reason why we ignored an attempt to set a login status.
void RecordSetLoginStatusIgnoredReason(FedCmSetLoginStatusIgnoredReason reason);

// Records the lifecycle state if we fail a FedCM request due to a page not
// being primary.
void RecordLifecycleStateFailureReason(FedCmLifecycleStateFailureReason reason);

// Records the number of accounts received before applying login/domain hints
// filter.
void RecordRawAccountsSize(int size);

// Records the number of accounts received after applying login/domain hints
// filter. If no account left, nothing will be recorded.
void RecordReadyToShowAccountsSize(int size);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_

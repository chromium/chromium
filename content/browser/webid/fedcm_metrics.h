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

namespace base {
class TimeDelta;
}

namespace content {

using MediationRequirement = ::password_manager::CredentialMediationRequirement;

// This enum describes the status of a request id token call to the FedCM API.
enum class FedCmRequestIdTokenStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kSuccess,
  kTooManyRequests,
  kAborted,
  kUnhandledRequest,
  kIdpNotPotentiallyTrustworthy,
  kNotSelectAccount,
  kConfigHttpNotFound,
  kConfigNoResponse,
  kConfigInvalidResponse,
  kClientMetadataHttpNotFound,     // obsolete
  kClientMetadataNoResponse,       // obsolete
  kClientMetadataInvalidResponse,  // obsolete
  kAccountsHttpNotFound,
  kAccountsNoResponse,
  kAccountsInvalidResponse,
  kIdTokenHttpNotFound,
  kIdTokenNoResponse,
  kIdTokenInvalidResponse,
  kIdTokenInvalidRequest,                  // obsolete
  kClientMetadataMissingPrivacyPolicyUrl,  // obsolete
  kThirdPartyCookiesBlocked,
  kDisabledInSettings,
  kDisabledInFlags,
  kWellKnownHttpNotFound,
  kWellKnownNoResponse,
  kWellKnownInvalidResponse,
  kConfigNotInWellKnown,
  kWellKnownTooBig,
  kDisabledEmbargo,
  kUserInterfaceTimedOut,  // obsolete
  kRpPageNotVisible,
  kShouldEmbargo,
  kNotSignedInWithIdp,
  kAccountsListEmpty,
  kWellKnownListEmpty,
  kWellKnownInvalidContentType,
  kConfigInvalidContentType,
  kAccountsInvalidContentType,
  kIdTokenInvalidContentType,
  kSilentMediationFailure,

  kMaxValue = kSilentMediationFailure
};

// This enum describes whether user sign-in states between IDP and browser
// match.
enum class FedCmSignInStateMatchStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kMatch,
  kIdpClaimedSignIn,
  kBrowserObservedSignIn,

  kMaxValue = kBrowserObservedSignIn
};

// This enum describes whether the browser's knowledge of whether the user is
// signed into the IDP based on observing signin/signout HTTP headers matches
// the information returned by the accounts endpoint.
enum class FedCmIdpSigninMatchStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kMatchWithAccounts,
  kMatchWithoutAccounts,
  kUnknownStatusWithAccounts,
  kUnknownStatusWithoutAccounts,
  kMismatchWithNetworkError,
  kMismatchWithNoContent,
  kMismatchWithInvalidResponse,
  kMismatchWithUnexpectedAccounts,

  kMaxValue = kMismatchWithUnexpectedAccounts
};

// This enum describes the type of frame that invokes preventSilentAccess.
enum class PreventSilentAccessFrameType {
  // Do not change the meaning or order of these values since they are being
  // recorded in metrics and in sync with the counterpart in enums.xml.
  kMainFrame,
  kSameSiteIframe,
  kCrossSiteIframe,

  kMaxValue = kCrossSiteIframe
};

class CONTENT_EXPORT FedCmMetrics {
 public:
  FedCmMetrics(const GURL& provider,
               const ukm::SourceId page_source_id,
               int session_id,
               bool is_disabled);

  ~FedCmMetrics() = default;

  // Records the time from when a call to the API was made to when the accounts
  // dialog is shown.
  void RecordShowAccountsDialogTime(base::TimeDelta duration);

  // Records the time from when the accounts dialog is shown to when the user
  // presses the Continue button.
  void RecordContinueOnDialogTime(base::TimeDelta duration);

  // Records metrics when the user explicitly closes the accounts dialog without
  // selecting any accounts. `duration` is the time from when the accounts
  // dialog was shown to when the user closed the dialog.
  void RecordCancelOnDialogTime(base::TimeDelta duration);

  // Records the reason that closed accounts dialog without selecting any
  // accounts. Unlike RecordCancelOnDialogTime() this metric is recorded in
  // cases that the acccounts dialog was closed without an explicit user action.
  void RecordCancelReason(
      IdentityRequestDialogController::DismissReason dismiss_reason);

  // Records the time from when the user presses the Continue button to when the
  // token response is received. Also records the overall time from when the API
  // is called to when the token response is received.
  void RecordTokenResponseAndTurnaroundTime(base::TimeDelta token_response_time,
                                            base::TimeDelta turnaround_time);

  // Records the status of the |RequestToken| call.
  void RecordRequestTokenStatus(FedCmRequestIdTokenStatus status,
                                MediationRequirement requirement);

  // Records whether user sign-in states between IDP and browser match.
  void RecordSignInStateMatchStatus(FedCmSignInStateMatchStatus status);

  // Records whether the browser's knowledge of whether the user is signed into
  // the IDP based on observing signin/signout HTTP headers matches the
  // information returned by the accounts endpoint.
  void RecordIdpSigninMatchStatus(
      absl::optional<bool> idp_signin_status,
      IdpNetworkRequestManager::ParseStatus accounts_endpoint_status);

  // Records whether the user selected account is for sign-in or not.
  void RecordIsSignInUser(bool is_sign_in);

  // Records whether a user has left the page where the API is called when the
  // browser is ready to show the accounts dialog.
  void RecordWebContentsVisibilityUponReadyToShowDialog(bool is_visible);

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
      absl::optional<bool> has_single_returning_account,
      const IdentityRequestAccount* auto_signin_account,
      bool auto_reauthn_success,
      bool is_auto_reauthn_setting_blocked,
      bool is_auto_reauthn_embargoed,
      absl::optional<base::TimeDelta> time_from_embargo,
      bool requires_user_mediation);

 private:
  // The page's SourceId. Used to log the UKM event Blink.FedCm.
  ukm::SourceId page_source_id_;

  // The SourceId to be used to log the UKM event Blink.FedCmIdp. Uses
  // |provider_| as the URL.
  ukm::SourceId provider_source_id_;

  // Whether a RequestTokenStatus has been recorded.
  bool request_token_status_recorded_{false};

  // The session ID associated to the FedCM call for which this object is
  // recording metrics. Each FedCM call gets a random integer session id, which
  // helps group UKM events by the session id.
  int session_id_;

  // Whether metrics recording is disabled for the request.
  bool is_disabled_{false};
};

// The following metric is recorded for UMA and UKM, but does not require an
// existing FedCM call. Records metrics associated with a preventSilentAccess()
// call from the given RenderFrameHost.
void RecordPreventSilentAccess(RenderFrameHost& rfh,
                               PreventSilentAccessFrameType frame_type);

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
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_

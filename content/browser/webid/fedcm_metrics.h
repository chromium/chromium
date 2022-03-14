// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_
#define CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_

#include "services/metrics/public/cpp/ukm_builders.h"

namespace base {
class TimeDelta;
}

namespace content {

// This enum describes the status of a request id token call to the FedCM API.
enum class FedCmRequestIdTokenStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kSuccess,
  kTooManyRequests,
  kAborted,
  kUnhandledRequest,
  kNoNetworkManager,
  kNotSelectAccount,
  kManifestHttpNotFound,
  kManifestNoResponse,
  kManifestInvalidResponse,
  kClientMetadataHttpNotFound,
  kClientMetadataNoResponse,
  kClientMetadataInvalidResponse,
  kAccountsHttpNotFound,
  kAccountsNoResponse,
  kAccountsInvalidResponse,
  kIdTokenHttpNotFound,
  kIdTokenNoResponse,
  kIdTokenInvalidResponse,
  kIdTokenInvalidRequest,
  kClientMetadataMissingPrivacyPolicyUrl,
  kThirdPartyCookiesBlocked,
  kDisabledInSettings,

  kMaxValue = kDisabledInSettings
};

// This enum describes the status of a revocation call to the FedCM API.
enum class FedCmRevokeStatus {
  // Don't change the meaning or the order of these values because they are
  // being recorded in metrics and in sync with the counterpart in enums.xml.
  kSuccess,
  kTooManyRequests,
  kUnhandledRequest,
  kNoNetworkManager,
  kNoAccountToRevoke,
  kRevokeUrlIsCrossOrigin,
  kRevocationFailedOnServer,
  kManifestHttpNotFound,
  kManifestNoResponse,
  kManifestInvalidResponse,
  kDisabledInSettings,

  kMaxValue = kDisabledInSettings
};

// Records the time from when a call to the API was made to when the accounts
// dialog is shown.
void RecordShowAccountsDialogTime(base::TimeDelta duration,
                                  ukm::SourceId source_id);

// Records the time from when the accounts dialog is shown to when the user
// presses the Continue button.
void RecordContinueOnDialogTime(base::TimeDelta duration,
                                ukm::SourceId source_id);

// Records the time from when the accounts dialog is shown to when the user
// closes the dialog without selecting any account.
void RecordCancelOnDialogTime(base::TimeDelta duration,
                              ukm::SourceId source_id);

// Records the time from when the user presses the Continue button to when the
// idtoken response is received. Also records the overall time from when the API
// is called to when the idtoken response is received.
void RecordIdTokenResponseAndTurnaroundTime(
    base::TimeDelta id_token_response_time,
    base::TimeDelta turnaround_time,
    ukm::SourceId source_id);

// Records the status of the |RequestIdToken| call.
void RecordRequestIdTokenStatus(FedCmRequestIdTokenStatus status,
                                ukm::SourceId source_id);

// Records the status of the |Revoke| call.
void RecordRevokeStatus(FedCmRevokeStatus status, ukm::SourceId source_id);

// Records whether the user selected account is for sign-in or not.
void RecordIsSignInUser(bool is_sign_in);

// Records whether a user has left the page where the API is called when the
// browser is ready to show the accounts dialog.
void RecordWebContentsVisibilityUponReadyToShowDialog(bool is_visible);

// Records whether an IDP returns an approved clients list in the response.
void RecordApprovedClientsExistence(bool has_approved_clients);

// Records the size of the approved clients list if applicable.
void RecordApprovedClientsSize(int size);
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_

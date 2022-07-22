// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "url/gurl.h"

namespace content {

FedCmMetrics::FedCmMetrics(const GURL& provider,
                           ukm::SourceId page_source_id,
                           int session_id)
    : page_source_id_(page_source_id),
      provider_source_id_(ukm::UkmRecorder::GetSourceIdForWebIdentityFromScope(
          base::PassKey<FedCmMetrics>(),
          provider)),
      session_id_(session_id) {}

void FedCmMetrics::RecordShowAccountsDialogTime(base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ShowAccountsDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ShowAccountsDialog", duration);
}

void FedCmMetrics::RecordContinueOnDialogTime(base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ContinueOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ContinueOnDialog", duration);
}

void FedCmMetrics::RecordCancelOnDialogTime(base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_CancelOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void FedCmMetrics::RecordCancelReason(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Blink.FedCm.CancelReason", dismiss_reason,
      IdentityRequestDialogController::DismissReason::COUNT);
}

void FedCmMetrics::RecordTokenResponseAndTurnaroundTime(
    base::TimeDelta token_response_time,
    base::TimeDelta turnaround_time) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder
        .SetTiming_IdTokenResponse(ukm::GetExponentialBucketMinForUserTiming(
            token_response_time.InMilliseconds()))
        .SetTiming_TurnaroundTime(ukm::GetExponentialBucketMinForUserTiming(
            turnaround_time.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.IdTokenResponse",
                             token_response_time);
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.TurnaroundTime",
                             turnaround_time);
}

void FedCmMetrics::RecordRequestTokenStatus(FedCmRequestIdTokenStatus status) {
  // If the request has failed but we have not yet rejected the promise,
  // e.g. when the user has declined the permission or the API is disabled
  // etc., we have already recorded a RequestTokenStatus. i.e.
  // `request_token_status_recorded_` would be true. In this case, we
  // shouldn't record another RequestTokenStatus.
  if (request_token_status_recorded_) {
    return;
  }
  request_token_status_recorded_ = true;

  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_RequestIdToken(static_cast<int>(status));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.RequestIdToken", status);
}

void FedCmMetrics::RecordSignInStateMatchStatus(
    FedCmSignInStateMatchStatus status) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_SignInStateMatch(static_cast<int>(status));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.SignInStateMatch", status);
}

void RecordIsSignInUser(bool is_sign_in) {
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.IsSignInUser", is_sign_in);
}

void RecordWebContentsVisibilityUponReadyToShowDialog(bool is_visible) {
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.WebContentsVisible", is_visible);
}

void RecordApprovedClientsExistence(bool has_approved_clients) {
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.ApprovedClientsExistence",
                        has_approved_clients);
}

void RecordApprovedClientsSize(int size) {
  UMA_HISTOGRAM_COUNTS_10000("Blink.FedCm.ApprovedClientsSize", size);
}

}  // namespace content

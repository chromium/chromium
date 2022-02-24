// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/metrics_utils.h"

namespace content {

void RecordShowAccountsDialogTime(base::TimeDelta duration,
                                  ukm::SourceId source_id) {
  ukm::builders::Blink_FedCm builder(source_id);
  builder.SetTiming_ShowAccountsDialog(
      ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ShowAccountsDialog", duration);
}

void RecordContinueOnDialogTime(base::TimeDelta duration,
                                ukm::SourceId source_id) {
  ukm::builders::Blink_FedCm builder(source_id);
  builder.SetTiming_ContinueOnDialog(
      ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ContinueOnDialog", duration);
}

void RecordCancelOnDialogTime(base::TimeDelta duration,
                              ukm::SourceId source_id) {
  ukm::builders::Blink_FedCm builder(source_id);
  builder.SetTiming_CancelOnDialog(
      ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void RecordIdTokenResponseAndTurnaroundTime(
    base::TimeDelta id_token_response_time,
    base::TimeDelta turnaround_time,
    ukm::SourceId source_id) {
  ukm::builders::Blink_FedCm builder(source_id);
  builder
      .SetTiming_IdTokenResponse(ukm::GetExponentialBucketMinForUserTiming(
          id_token_response_time.InMilliseconds()))
      .SetTiming_TurnaroundTime(ukm::GetExponentialBucketMinForUserTiming(
          turnaround_time.InMilliseconds()));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.IdTokenResponse",
                             id_token_response_time);
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.TurnaroundTime",
                             turnaround_time);
}

void RecordRequestIdTokenStatus(FedCmRequestIdTokenStatus status,
                                ukm::SourceId source_id) {
  ukm::builders::Blink_FedCm builder(source_id);
  builder.SetStatus_RequestIdToken(static_cast<int>(status));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.RequestIdToken", status);
}

void RecordRevokeStatus(FedCmRevokeStatus status, ukm::SourceId source_id) {
  ukm::builders::Blink_FedCm builder(source_id);
  builder.SetStatus_Revoke(static_cast<int>(status));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.Revoke", status);
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

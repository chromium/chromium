// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "services/metrics/public/cpp/metrics_utils.h"

namespace content {

// The only purpose of this class currently is to be friended by the
// UkmRecorder. It therefore cannot be in the anonymous namespace.
// TODO(crbug.com/1334210): Add sourceId and provider to FedCmMetrics so the
// recording methods do not all have to pass these.
class FedCmMetrics {
 public:
  // Gets the ukm source id for a web identity provider.
  static ukm::SourceId GetUkmSourceIdForWebIdentityFromScope(
      const GURL& provider) {
    return ukm::UkmRecorder::GetSourceIdForWebIdentityFromScope(
        base::PassKey<FedCmMetrics>(), provider);
  }
};

void RecordShowAccountsDialogTime(base::TimeDelta duration,
                                  ukm::SourceId source_id,
                                  const GURL& provider) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ShowAccountsDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(source_id);
  RecordUkm(fedcm_builder);
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      FedCmMetrics::GetUkmSourceIdForWebIdentityFromScope(provider));
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ShowAccountsDialog", duration);
}

void RecordContinueOnDialogTime(base::TimeDelta duration,
                                ukm::SourceId source_id,
                                const GURL& provider) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ContinueOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(source_id);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      FedCmMetrics::GetUkmSourceIdForWebIdentityFromScope(provider));
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ContinueOnDialog", duration);
}

void RecordCancelOnDialogTime(base::TimeDelta duration,
                              ukm::SourceId source_id,
                              const GURL& provider) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_CancelOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(source_id);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      FedCmMetrics::GetUkmSourceIdForWebIdentityFromScope(provider));
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void RecordTokenResponseAndTurnaroundTime(base::TimeDelta token_response_time,
                                          base::TimeDelta turnaround_time,
                                          ukm::SourceId source_id,
                                          const GURL& provider) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder
        .SetTiming_IdTokenResponse(ukm::GetExponentialBucketMinForUserTiming(
            token_response_time.InMilliseconds()))
        .SetTiming_TurnaroundTime(ukm::GetExponentialBucketMinForUserTiming(
            turnaround_time.InMilliseconds()));
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(source_id);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      FedCmMetrics::GetUkmSourceIdForWebIdentityFromScope(provider));
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.IdTokenResponse",
                             token_response_time);
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.TurnaroundTime",
                             turnaround_time);
}

void RecordRequestTokenStatus(FedCmRequestIdTokenStatus status,
                              ukm::SourceId source_id,
                              const GURL& provider) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_RequestIdToken(static_cast<int>(status));
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(source_id);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      FedCmMetrics::GetUkmSourceIdForWebIdentityFromScope(provider));
  RecordUkm(fedcm_idp_builder);

  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.RequestIdToken", status);
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

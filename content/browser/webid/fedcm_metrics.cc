// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/metrics_utils.h"

namespace content {

void RecordShowAccountsDialogTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ShowAccountsDialog", duration);
}

void RecordContinueOnDialogTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.ContinueOnDialog", duration);
}

void RecordCancelOnDialogTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void RecordIdTokenResponseAndTurnaroundTime(
    base::TimeDelta id_token_response_time,
    base::TimeDelta turnaround_time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.IdTokenResponse",
                             id_token_response_time);
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.FedCm.Timing.TurnaroundTime",
                             turnaround_time);
}

void RecordRequestIdTokenStatus(FedCmRequestIdTokenStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.RequestIdToken", status);
}

void RecordRevokeStatus(FedCmRevokeStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Status.Revoke", status);
}

}  // namespace content

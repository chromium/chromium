// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"

namespace network {

namespace internal {

const char kTrustTokenTotalTimeHistogramNameBase[] =
    "Net.TrustTokens.OperationTotalTime";
const char kTrustTokenFinalizeTimeHistogramNameBase[] =
    "Net.TrustTokens.OperationFinalizeTime";
const char kTrustTokenBeginTimeHistogramNameBase[] =
    "Net.TrustTokens.OperationBeginTime";
const char kTrustTokenServerTimeHistogramNameBase[] =
    "Net.TrustTokens.OperationServerTime";

}  // namespace internal

namespace {

// These must stay in sync with the corresponding histogram suffixes in
// histograms.xml.
std::string_view StatusToSuccessOrFailure(
    mojom::TrustTokenOperationStatus status) {
  switch (status) {
    case mojom::TrustTokenOperationStatus::kOk:
    case mojom::TrustTokenOperationStatus::kAlreadyExists:
    case mojom::TrustTokenOperationStatus::
        kOperationSuccessfullyFulfilledLocally:
      return "Success";
    default:
      return "Failure";
  }
}

// These must stay in sync with the corresponding histogram suffixes in
// histograms.xml.
std::string_view TypeToString(mojom::TrustTokenOperationType type) {
  switch (type) {
    case mojom::TrustTokenOperationType::kIssuance:
      return "Issuance";
    case mojom::TrustTokenOperationType::kRedemption:
      return "Redemption";
    case mojom::TrustTokenOperationType::kSigning:
      return "Signing";
  }
}

const char kHistogramPartsSeparator[] = ".";

}  // namespace

TrustTokenOperationMetricsRecorder::TrustTokenOperationMetricsRecorder(
    mojom::TrustTokenOperationType type)
    : type_(type) {}
TrustTokenOperationMetricsRecorder::~TrustTokenOperationMetricsRecorder() =
    default;

void TrustTokenOperationMetricsRecorder::BeginBegin() {
  begin_start_ = base::TimeTicks::Now();
}

void TrustTokenOperationMetricsRecorder::FinishBegin(
    mojom::TrustTokenOperationStatus status) {
  begin_end_ = base::TimeTicks::Now();

  base::UmaHistogramTimes(
      base::JoinString({internal::kTrustTokenBeginTimeHistogramNameBase,
                        StatusToSuccessOrFailure(status), TypeToString(type_)},
                       kHistogramPartsSeparator),
      begin_end_ - begin_start_);
}

void TrustTokenOperationMetricsRecorder::BeginFinalize() {
  // Wait until FinishFinalize to determine whether to log the server time as a
  // success or a failure.
  finalize_start_ = base::TimeTicks::Now();
}

void TrustTokenOperationMetricsRecorder::FinishFinalize(
    mojom::TrustTokenOperationStatus status) {
  base::TimeTicks finalize_end = base::TimeTicks::Now();

  base::UmaHistogramTimes(
      base::JoinString({internal::kTrustTokenServerTimeHistogramNameBase,
                        StatusToSuccessOrFailure(status), TypeToString(type_)},
                       kHistogramPartsSeparator),
      finalize_start_ - begin_end_);

  base::UmaHistogramTimes(
      base::JoinString({internal::kTrustTokenTotalTimeHistogramNameBase,
                        StatusToSuccessOrFailure(status), TypeToString(type_)},
                       kHistogramPartsSeparator),
      finalize_end - begin_start_);

  base::UmaHistogramTimes(
      base::JoinString({internal::kTrustTokenFinalizeTimeHistogramNameBase,
                        StatusToSuccessOrFailure(status), TypeToString(type_)},
                       kHistogramPartsSeparator),
      finalize_end - finalize_start_);
}

void HistogramTrustTokenOperationNetError(
    network::mojom::TrustTokenOperationType type,
    network::mojom::TrustTokenOperationStatus status,
    int net_error) {
  base::UmaHistogramSparse(
      base::JoinString({"Net.TrustTokens.NetErrorForTrustTokenOperation",
                        StatusToSuccessOrFailure(status), TypeToString(type)},
                       kHistogramPartsSeparator),
      net_error);
}

}  // namespace  network

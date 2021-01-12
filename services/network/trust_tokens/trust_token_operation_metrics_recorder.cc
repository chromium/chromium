// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
base::StringPiece StatusToSuccessOrFailure(
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
base::StringPiece TypeToString(mojom::TrustTokenOperationType type) {
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

// If |operation_is_platform_provided| indicates that the Trust Tokens operation
// corresponding to the metric name in |pieces| is platform-provided, adds an
// element to |pieces| mentioning this fact.
//
// Note: As of writing during the initial platform-provided issuance
// implementation, issuance is the only platform-provided operation and its
// control flow never enters TrustTokenRequestHelper::Finalize call, so the only
// metrics this is expected (initially) to affect are
// OperationBeginTime.Issuance.Success and OperationBeginTime.Issuance.Failure.
std::vector<base::StringPiece> MaybeAppendPlatformProvidedIndicator(
    std::vector<base::StringPiece> pieces,
    bool operation_is_platform_provided) {
  if (operation_is_platform_provided)
    pieces.push_back("PlatformProvided");
  return pieces;
}

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
      base::JoinString(
          MaybeAppendPlatformProvidedIndicator(
              {internal::kTrustTokenBeginTimeHistogramNameBase,
               StatusToSuccessOrFailure(status), TypeToString(type_)},
              operation_is_platform_provided_),
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
      base::JoinString(
          MaybeAppendPlatformProvidedIndicator(
              {internal::kTrustTokenServerTimeHistogramNameBase,
               StatusToSuccessOrFailure(status), TypeToString(type_)},
              operation_is_platform_provided_),
          kHistogramPartsSeparator),
      finalize_start_ - begin_end_);

  base::UmaHistogramTimes(
      base::JoinString(
          MaybeAppendPlatformProvidedIndicator(
              {internal::kTrustTokenTotalTimeHistogramNameBase,
               StatusToSuccessOrFailure(status), TypeToString(type_)},
              operation_is_platform_provided_),
          kHistogramPartsSeparator),
      finalize_end - begin_start_);

  base::UmaHistogramTimes(
      base::JoinString(
          MaybeAppendPlatformProvidedIndicator(
              {internal::kTrustTokenFinalizeTimeHistogramNameBase,
               StatusToSuccessOrFailure(status), TypeToString(type_)},
              operation_is_platform_provided_),
          kHistogramPartsSeparator),
      finalize_end - finalize_start_);
}

void TrustTokenOperationMetricsRecorder::
    WillExecutePlatformProvidedOperation() {
  operation_is_platform_provided_ = true;
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

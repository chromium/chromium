// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_OPERATION_METRICS_RECORDER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_OPERATION_METRICS_RECORDER_H_

#include "base/time/time.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace network {

namespace internal {

// The following templates, which allow constructing names for the various Trust
// Tokens timing histograms, are exposed for testing.
extern const char kTrustTokenServerTimeHistogramNameBase[];
extern const char kTrustTokenTotalTimeHistogramNameBase[];
extern const char kTrustTokenFinalizeTimeHistogramNameBase[];
extern const char kTrustTokenBeginTimeHistogramNameBase[];

}  // namespace internal

// A TrustTokenOperationMetricsRecorder records timing and success metrics for a
// single Trust Tokens operation. To use, call BeginBegin at the time the Begin
// (outbound) part of the operation starts and FinishBegin at the time the Begin
// part finishes; if the Begin part was successful, call BeginFinalize and
// FinishFinalize analogously during the Finalize (inbound) part of the
// operation.
class TrustTokenOperationMetricsRecorder {
 public:
  explicit TrustTokenOperationMetricsRecorder(
      mojom::TrustTokenOperationType type);
  ~TrustTokenOperationMetricsRecorder();

  TrustTokenOperationMetricsRecorder(
      const TrustTokenOperationMetricsRecorder&) = delete;
  TrustTokenOperationMetricsRecorder& operator=(
      const TrustTokenOperationMetricsRecorder&) = delete;

  void BeginBegin();
  void FinishBegin(mojom::TrustTokenOperationStatus status);

  void BeginFinalize();
  void FinishFinalize(mojom::TrustTokenOperationStatus status);

 private:
  mojom::TrustTokenOperationType type_;

  // Start and end times for the Begin part of the operation:
  base::TimeTicks begin_start_;
  base::TimeTicks begin_end_;

  // Start time for the Finalize part of the operation:
  base::TimeTicks finalize_start_;
};

// HistogramTrustTokenOperationNetError logs a //net error code corresponding to
// a Trust Tokens operation. This is a temporary measure for helping understand
// why "Failed to fetch" errors occur quite often in live testing: see
// https://crbug.com/1128174.
void HistogramTrustTokenOperationNetError(
    network::mojom::TrustTokenOperationType type,
    network::mojom::TrustTokenOperationStatus status,
    int net_error);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_OPERATION_METRICS_RECORDER_H_

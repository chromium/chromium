// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/operation_timing_request_helper_wrapper.h"

namespace network {

OperationTimingRequestHelperWrapper::OperationTimingRequestHelperWrapper(
    std::unique_ptr<TrustTokenOperationMetricsRecorder> metrics_recorder,
    std::unique_ptr<TrustTokenRequestHelper> helper)
    : recorder_(std::move(metrics_recorder)), helper_(std::move(helper)) {}

OperationTimingRequestHelperWrapper::~OperationTimingRequestHelperWrapper() =
    default;

void OperationTimingRequestHelperWrapper::Begin(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  recorder_->BeginBegin();
  helper_->Begin(
      request, base::BindOnce(&OperationTimingRequestHelperWrapper::FinishBegin,
                              weak_factory_.GetWeakPtr(), std::move(done)));
}

void OperationTimingRequestHelperWrapper::Finalize(
    mojom::URLResponseHead* response,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  recorder_->BeginFinalize();
  helper_->Finalize(
      response,
      base::BindOnce(&OperationTimingRequestHelperWrapper::FinishFinalize,
                     weak_factory_.GetWeakPtr(), std::move(done)));
}

void OperationTimingRequestHelperWrapper::FinishBegin(
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenOperationStatus status) {
  recorder_->FinishBegin(status);
  std::move(done).Run(status);
}

void OperationTimingRequestHelperWrapper::FinishFinalize(
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenOperationStatus status) {
  recorder_->FinishFinalize(status);
  std::move(done).Run(status);
}

mojom::TrustTokenOperationResultPtr
OperationTimingRequestHelperWrapper::CollectOperationResultWithStatus(
    mojom::TrustTokenOperationStatus status) {
  return helper_->CollectOperationResultWithStatus(status);
}

}  // namespace network
